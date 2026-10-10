// TSR Q · DSP core (C++17, header-only, real-time safe: nessuna allocazione nel process)
// Porting 1:1 del core verificato nel prototipo + All Pass, knee, detector Peak/RMS, sidechain.
#pragma once
#include <cmath>
#include <atomic>
#include <algorithm>
#include <vector>
#include <cstring>

namespace tsrq {

constexpr double kPi = 3.14159265358979323846;
enum Type { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, FlatTilt, AllPass, NumTypes };
enum Place { Stereo = 0, Left, Right, Mid, Side };
enum DetMode { DetRMS = 0, DetPeak };
constexpr int kMaxBands = 32;
constexpr int kMaxSecs = 16;

inline bool hasGain (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == TiltShelf || t == FlatTilt; }
inline bool canDyn  (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == TiltShelf; }
inline bool hasSlope(int t) { return t == LowCut || t == HighCut || t == LowShelf || t == HighShelf || t == BandPass; }

struct BandParams {
    bool used = false, bypass = false;
    int type = Bell; double f = 1000, gain = 0, q = 1; int slope = 12; int place = Stereo;
    bool dyn = false; double thr = -24, range = -6, att = 5, rel = 80, knee = 6; int det = DetRMS; bool sc = false;
    bool gq = false;   // Gain-Q interaction (solo Bell): Q effettivo = Q * (1 + |guadagno| / 15), massimo 40
};
struct Sec { double f0, n[3], d[3]; };
struct SecSet { int n = 0; double k = 1; Sec s[kMaxSecs]; };
struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
struct BandCoefs { int n = 0; Coef c[kMaxSecs]; };

// ---------- prototipi analogici (s normalizzata su f0) ----------
inline int butterQs (int N, double* q) {
    int c = 0;
    if (N % 2 == 0) for (int k = 0; k < N / 2; ++k) q[c++] = 1.0 / (2.0 * std::cos (kPi * (2 * k + 1) / (2.0 * N)));
    else            for (int k = 1; k <= (N - 1) / 2; ++k) q[c++] = 1.0 / (2.0 * std::cos (kPi * k / N));
    std::sort (q, q + c);
    return c;
}
inline double secMag2 (const Sec& s, double fr) {
    const double x = fr / s.f0, rn = s.n[0] - s.n[2] * x * x, in = s.n[1] * x, rd = s.d[0] - s.d[2] * x * x, id = s.d[1] * x;
    return (rn * rn + in * in) / (rd * rd + id * id);
}
inline double anaMag2 (const SecSet& set, double fr) { double m = set.k * set.k; for (int i = 0; i < set.n; ++i) m *= secMag2 (set.s[i], fr); return m; }
inline void push (SecSet& o, double f0, double n0, double n1, double n2, double d0, double d1, double d2) {
    if (o.n >= kMaxSecs) return;
    Sec& s = o.s[o.n++]; s.f0 = f0; s.n[0] = n0; s.n[1] = n1; s.n[2] = n2; s.d[0] = d0; s.d[1] = d1; s.d[2] = d2;
}
inline void sectionsFor (const BandParams& b, double gainDb, SecSet& o) {
    o.n = 0; o.k = 1;
    const double f = b.f, Q = b.q, A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
    switch (b.type) {
        case Bell: { const double Qe = b.gq ? std::min (40.0, Q * (1.0 + std::abs (gainDb) / 15.0)) : Q; push (o, f, 1, A / Qe, 1, 1, 1 / (A * Qe), 1); break; }
        case LowShelf:  if (b.slope <= 6) push (o, f, A * A, A, 0, 1, A, 0); else push (o, f, A * A, A * sA / Q, A, 1, sA / Q, A); break;
        case HighShelf: if (b.slope <= 6) push (o, f, A, A * A, 0, A, 1, 0); else push (o, f, A, A * sA / Q, A * A, A, sA / Q, 1); break;
        case TiltShelf: push (o, f, 1, A, 0, A, 1, 0); break;
        case Notch:     push (o, f, 1, 0, 1, 1, 1 / Q, 1); break;
        case BandPass: {   // 6 dB/oct = una sezione passa-banda; 12…96 dB/oct = passa-alto + passa-basso Butterworth di ordine slope/6 ai bordi della banda, picco a 0 dB
            const int N = std::clamp ((int) std::lround (b.slope / 6.0), 1, 16);
            if (N == 1) { push (o, f, 0, 1 / Q, 0, 1, 1 / Q, 1); break; }
            const double r = std::sqrt (1 + 1 / (4 * Q * Q)), fl = f * (r - 1 / (2 * Q)), fh = f * (r + 1 / (2 * Q)); double qs[8]; const int nq = butterQs (N, qs);
            if (N % 2) { push (o, fl, 0, 1, 0, 1, 1, 0); push (o, fh, 1, 0, 0, 1, 1, 0); }
            for (int i = 0; i < nq; ++i) { push (o, fl, 0, 0, 1, 1, 1 / qs[i], 1); push (o, fh, 1, 0, 0, 1, 1 / qs[i], 1); }
            o.k = 1.0 / std::sqrt (anaMag2 (o, f));
            break;
        }
        case AllPass:   push (o, f, 1, -1 / Q, 1, 1, 1 / Q, 1); break;   // progettato a parte (fase non minima)
        case LowCut: case HighCut: {
            const int N = std::clamp ((int) std::lround (b.slope / 6.0), 1, 16); double qs[8]; const int nq = butterQs (N, qs);
            // Low/High Cut: solo taglio Butterworth, nessuna risonanza (mai sopra 0 dB): il Q non si usa
            const bool hp = b.type == LowCut;
            if (N % 2) { if (hp) push (o, f, 0, 1, 0, 1, 1, 0); else push (o, f, 1, 0, 0, 1, 1, 0); }
            for (int i = 0; i < nq; ++i) { if (hp) push (o, f, 0, 0, 1, 1, 1 / qs[i], 1); else push (o, f, 1, 0, 0, 1, 1 / qs[i], 1); }
            break;
        }
        case FlatTilt: {
            const double S = gainDb / std::log2 (1000.0), D = 1.5, r = std::pow (10.0, S * D / 20.0);
            for (double z = 4; z < 60000; z *= std::pow (2.0, D)) push (o, z, 1, 1, 0, 1, 1 / r, 0);
            o.k = 1.0 / std::sqrt (anaMag2 (o, f));
            break;
        }
        default: break;
    }
}

// ---------- progetto digitale: tre candidati, vince il minor errore massimo vs analogico ----------
struct Grid {
    double fs = 0; double f[32], p0[32], p1[32], p2[32]; double ef[24];
    void init (double sampleRate) {
        fs = sampleRate;
        for (int i = 0; i < 32; ++i) {
            const double fr = 10.0 * std::pow (fs / 2 * 0.999 / 10.0, i / 31.0), w = 2 * kPi * fr / fs; double s = std::sin (w / 2); s *= s;
            f[i] = fr; p0[i] = 1 - s; p1[i] = s; p2[i] = 4 * (1 - s) * s;
        }
        const double top = std::min (20000.0, fs / 2 * 0.98);
        for (int i = 0; i < 24; ++i) ef[i] = 20.0 * std::pow (top / 20.0, i / 23.0);
    }
};
inline void poles (const double* d, double w0, double& a1, double& a2) {
    const double d0 = d[0], d1 = d[1], d2 = d[2];
    if (d2 == 0) { const double p = -d0 / d1 * w0; a1 = -std::exp (p); a2 = 0; return; }
    const double disc = d1 * d1 - 4 * d2 * d0;
    if (disc < 0) {
        double re = -d1 / (2 * d2) * w0, im = std::sqrt (-disc) / (2 * d2) * w0; const double wn = std::sqrt (re * re + im * im), lim = 0.98 * kPi;
        if (wn > lim) { re *= lim / wn; im *= lim / wn; }
        const double r = std::exp (re); a1 = -2 * r * std::cos (im); a2 = r * r; return;
    }
    const double sq = std::sqrt (disc), p1 = (-d1 + sq) / (2 * d2) * w0, p2 = (-d1 - sq) / (2 * d2) * w0;
    a1 = -(std::exp (p1) + std::exp (p2)); a2 = std::exp (p1 + p2);
}
inline double coefMag2 (const Coef& c, double fr, double fs) {
    const double w = 2 * kPi * fr / fs, cw = std::cos (w), c2w = std::cos (2 * w);
    const double num = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2 + 2 * (c.b0 * c.b1 + c.b1 * c.b2) * cw + 2 * c.b0 * c.b2 * c2w;
    const double den = 1 + c.a1 * c.a1 + c.a2 * c.a2 + 2 * (c.a1 + c.a1 * c.a2) * cw + 2 * c.a2 * c2w;
    return num / den;
}
inline bool fromB (double B0, double B1, double B2, double* out) {
    if (B0 < 0 && B0 > -1e-12) B0 = 0;   // zero numerico (es. passa-alto: |N(1)|^2 = 0): stesso esito in JS e C++
    if (B1 < 0 && B1 > -1e-12) B1 = 0;
    if (! (B0 >= 0) || ! (B1 >= 0)) return false;
    const double s0 = std::sqrt (B0), s1 = std::sqrt (B1), W = 0.5 * (s0 + s1), d = W * W + B2;
    if (! (d >= 0)) return false;
    const double b0 = 0.5 * (W + std::sqrt (d));
    out[0] = b0; out[1] = 0.5 * (s0 - s1); out[2] = b0 > 0 ? -B2 / (4 * b0) : 0; return true;
}
inline Coef design3pt (const Sec& sc, double fs) {
    Coef c; const double w0 = 2 * kPi * sc.f0 / fs; poles (sc.d, w0, c.a1, c.a2);
    const double a1 = c.a1, a2 = c.a2, A0 = (1 + a1 + a2) * (1 + a1 + a2), A1 = (1 - a1 + a2) * (1 - a1 + a2), A2 = -4 * a2;
    const double wm = std::min (std::max (w0, 1e-5), 0.8 * kPi); double p1 = std::sin (wm / 2); p1 *= p1; const double p0 = 1 - p1, p2 = 4 * p0 * p1;
    const double B0 = secMag2 (sc, 0) * A0, B1 = secMag2 (sc, fs / 2) * A1;
    const double B2 = (secMag2 (sc, wm * fs / (2 * kPi)) * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;
    const double s0 = std::sqrt (B0), s1 = std::sqrt (B1), W = 0.5 * (s0 + s1); double disc = W * W + B2; if (disc < 0) disc = 0;
    c.b0 = 0.5 * (W + std::sqrt (disc)); c.b1 = 0.5 * (s0 - s1); c.b2 = c.b0 > 0 ? -B2 / (4 * c.b0) : 0;
    return c;
}
inline void zerosPoly (const double* n, double w0, double* z) {
    if (n[0] == 0 && n[1] == 0) { z[0] = 1; z[1] = -2; z[2] = 1; return; }
    if (n[0] == 0 && n[2] == 0) { z[0] = 1; z[1] = -1; z[2] = 0; return; }
    if (n[0] == 0) { const double e = std::exp (-n[1] / n[2] * w0); z[0] = 1; z[1] = -(1 + e); z[2] = e; return; }
    if (n[1] == 0 && n[2] == 0) { z[0] = 1; z[1] = 0; z[2] = 0; return; }
    double a1, a2; poles (n, w0, a1, a2); z[0] = 1; z[1] = a1; z[2] = a2;
}
inline Coef designMZ (const Sec& sc, double fs) {
    Coef c; const double w0 = 2 * kPi * sc.f0 / fs; poles (sc.d, w0, c.a1, c.a2);
    double z[3]; zerosPoly (sc.n, w0, z); c.b0 = z[0]; c.b1 = z[1]; c.b2 = z[2];
    const double ref = secMag2 (sc, 0) > 1e-12 ? 1e-3 : fs / 2 * 0.999, dg = coefMag2 (c, ref, fs), g = dg > 0 ? std::sqrt (secMag2 (sc, ref) / dg) : 1;
    c.b0 *= g; c.b1 *= g; c.b2 *= g; return c;
}
inline bool solve5 (double* M, double* v, double* x) {
    for (int i = 0; i < 5; ++i) {
        int p = i; for (int r = i + 1; r < 5; ++r) if (std::abs (M[r * 5 + i]) > std::abs (M[p * 5 + i])) p = r;
        if (p != i) { for (int c = 0; c < 5; ++c) std::swap (M[i * 5 + c], M[p * 5 + c]); std::swap (v[i], v[p]); }
        const double dd = M[i * 5 + i]; if (! (std::abs (dd) > 1e-300)) return false;
        for (int r = i + 1; r < 5; ++r) { const double fct = M[r * 5 + i] / dd; for (int c = i; c < 5; ++c) M[r * 5 + c] -= fct * M[i * 5 + c]; v[r] -= fct * v[i]; }
    }
    for (int i = 4; i >= 0; --i) { double s = v[i]; for (int c = i + 1; c < 5; ++c) s -= M[i * 5 + c] * x[c]; x[i] = s / M[i * 5 + i]; }
    return true;
}
inline bool designIRLS (const Sec& sc, const Grid& g, const Coef& init, Coef& out) {
    double PF[40], P0[40], P1[40], P2[40], TT[40]; int NP = 0; const double fs = g.fs, ny = fs / 2 * 0.999;
    for (int i = 0; i < 32; ++i) if (i % 4 != 3) PF[NP++] = g.f[i];
    for (int i = -4; i <= 4; ++i) { const double fr = sc.f0 * std::pow (2.0, i / 8.0); if (fr > 5 && fr < ny) PF[NP++] = fr; }
    for (int i = 0; i < NP; ++i) { const double w = 2 * kPi * PF[i] / fs; double s = std::sin (w / 2); s *= s; P0[i] = 1 - s; P1[i] = s; P2[i] = 4 * (1 - s) * s; TT[i] = secMag2 (sc, PF[i]); }
    double a1 = init.a1, a2 = init.a2; bool ok = false;
    for (int it = 0; it < 4; ++it) {
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2); if (! (A0 > 1e-14)) break;
        const double A1 = (1 - a1 + a2) * (1 - a1 + a2) / A0, A2 = -4 * a2 / A0;
        double M[25] = {}, V[5] = {}, x[5];
        for (int i = 0; i < NP; ++i) {
            const double T = TT[i], D = P0[i] + A1 * P1[i] + A2 * P2[i], td = T * D, w = 1 / (td * td + 1e-24), y = T * P0[i];
            const double rw[5] = { P0[i], P1[i], P2[i], -T * P1[i], -T * P2[i] };
            for (int r = 0; r < 5; ++r) { const double wr = w * rw[r]; V[r] += wr * y; for (int c = 0; c < 5; ++c) M[r * 5 + c] += wr * rw[c]; }
        }
        if (! solve5 (M, V, x)) break;
        double den[3], num[3]; if (! fromB (1, x[3], x[4], den) || ! fromB (x[0], x[1], x[2], num)) break;
        const double a0 = den[0]; a1 = den[1] / a0; a2 = den[2] / a0;
        if (! (std::abs (a2) < 0.99999 && std::abs (a1) < 1 + a2)) break;
        out.b0 = num[0] / a0; out.b1 = num[1] / a0; out.b2 = num[2] / a0; out.a1 = a1; out.a2 = a2; ok = true;
    }
    return ok;
}
inline double secErr (const Coef& c, const Sec& sc, const Grid& g) {
    double m = 0; const double top = g.ef[23];
    for (int i = 0; i < 33; ++i) {
        double fr; if (i < 24) fr = g.ef[i]; else { fr = sc.f0 * std::pow (2.0, (i - 28) / 8.0); if (fr < 20 || fr > top) continue; }
        const double d = std::max (coefMag2 (c, fr, g.fs), 1e-6), a = std::max (secMag2 (sc, fr), 1e-6), e = std::abs (10 * std::log10 (d / a));
        if (! (e < 1e9)) return 1e9; m = std::max (m, e);
    }
    return m;
}
inline bool stable (const Coef& c) { return std::isfinite (c.b0 + c.b1 + c.b2 + c.a1 + c.a2) && std::abs (c.a2) < 1 && std::abs (c.a1) < 1 + c.a2; }
inline Coef withZeros (const Coef& c, const Sec& sc, double fs) {
    const double w0 = 2 * kPi * sc.f0 / fs * std::sqrt (sc.n[0] / sc.n[2]); Coef q = c; q.b0 = 1; q.b1 = -2 * std::cos (w0); q.b2 = 1;
    const double g = std::sqrt (secMag2 (sc, 0) / coefMag2 (q, 1e-3, fs)); q.b0 = g; q.b1 *= g; q.b2 = g; return q;
}
inline Coef designSec (const Sec& sc, const Grid& g) {
    const bool jw = sc.n[1] == 0 && sc.n[0] > 0 && sc.n[2] > 0;
    Coef mz = designMZ (sc, g.fs), best = mz; double be = stable (mz) ? secErr (mz, sc, g) : 1e9;
    if (be < 0.01) return best;
    Coef c3 = design3pt (sc, g.fs); if (jw) c3 = withZeros (c3, sc, g.fs);
    const double e3 = stable (c3) ? secErr (c3, sc, g) : 1e9; if (e3 < be) { best = c3; be = e3; }
    if (sc.n[0] == 0 && sc.n[1] == 0) return best;   // passa-alto puro: il fit IRLS è mal condizionato (|N(1)|^2 = 0) e darebbe risultati diversi tra JS e C++
    Coef ci; if (designIRLS (sc, g, stable (mz) ? mz : c3, ci)) { if (jw) ci = withZeros (ci, sc, g.fs); if (stable (ci)) { const double ei = secErr (ci, sc, g); if (ei < be) { best = ci; be = ei; } } }
    return best;
}
inline void designBand (const BandParams& b, double gainDb, const Grid& g, BandCoefs& out) {
    SecSet set; sectionsFor (b, gainDb, set); out.n = set.n;
    if (b.type == AllPass) {                       // all pass: poli mappati esattamente, numeratore = denominatore invertito
        Coef c; poles (set.s[0].d, 2 * kPi * std::min (b.f, g.fs * 0.49) / g.fs, c.a1, c.a2);
        c.b0 = c.a2; c.b1 = c.a1; c.b2 = 1; out.c[0] = c; out.n = 1; return;
    }
    for (int i = 0; i < set.n; ++i) out.c[i] = designSec (set.s[i], g);
    out.c[0].b0 *= set.k; out.c[0].b1 *= set.k; out.c[0].b2 *= set.k;
}
inline double bandMag2 (const BandCoefs& bc, double fr, double fs) { double m = 1; for (int i = 0; i < bc.n; ++i) m *= coefMag2 (bc.c[i], fr, fs); return m; }

// ---------- dinamica ----------
inline double kneeFn (double over, double W) { if (W <= 0) return std::max (0.0, over); if (over <= -W / 2) return 0; if (over >= W / 2) return over; const double t = over + W / 2; return t * t / (2 * W); }
constexpr double kDynSlope = 0.75;    // 4:1 dentro il RANGE
inline double dynDelta (double levelDb, const BandParams& b) {
    double amt = kneeFn (levelDb - b.thr, b.knee) * kDynSlope; const double r = std::abs (b.range);
    if (amt > r) amt = r; return b.range < 0 ? -amt : amt;
}

// ---------- oversampling 4x per il Character: halfband IIR polifase (2 catene di passa-tutto del 1° ordine), 12 coefficienti,
// banda di transizione 0.02: banda passante piatta fino a 0.23·fs2, attenuazione fuori banda > 120 dB. Nessun ritardo puro (fase minima).
constexpr double kHB[12] = { 0.027155856726483182, 0.10300238556004077, 0.21303004592041422, 0.33933626891036295, 0.46602752012040527, 0.58224701385700428,
                             0.68259076485235193, 0.76595528238687738, 0.83396924102510472, 0.88969265368750716, 0.93680311116581549, 0.97923872973422221 };
struct AllpassChain {   // 6 passa-tutto (a + z^-1)/(1 + a z^-1) alla frequenza della catena
    double x1[6] = {}, y1[6] = {}; int off = 0;
    double run (double x) { for (int i = 0; i < 6; ++i) { const double a = kHB[off + 2 * i], y = a * (x - y1[i]) + x1[i]; x1[i] = x; y1[i] = y; x = y; } return x; }
    void reset() { for (int i = 0; i < 6; ++i) x1[i] = y1[i] = 0; }
};
struct HalfbandUp   { AllpassChain a, b; HalfbandUp()   { a.off = 0; b.off = 1; } void run (double x, double& y0, double& y1) { y0 = a.run (x); y1 = b.run (x); } void reset() { a.reset(); b.reset(); } };
struct HalfbandDown { AllpassChain a, b; double prev = 0; HalfbandDown() { a.off = 0; b.off = 1; } double run (double e, double o) { const double y = 0.5 * (a.run (e) + b.run (prev)); prev = o; return y; } void reset() { a.reset(); b.reset(); prev = 0; } };
inline double characterShape (int chm, double x) {
    if (chm == 1) return x + 0.03 * x * x - 0.06 * x * x * x;
    return std::tanh (1.8 * x) / 1.8 + 0.04 * x * x;
}
struct Oversampled4x {   // x -> 4 campioni -> saturazione -> 1 campione
    HalfbandUp u1, u2; HalfbandDown d2, d1;   // u2 e d2 lavorano sul flusso a 2x in sequenza (un solo stato)
    double run (int chm, double x) {
        double a, b, c0, c1, c2, c3; u1.run (x, a, b); u2.run (a, c0, c1); u2.run (b, c2, c3);
        c0 = characterShape (chm, c0); c1 = characterShape (chm, c1); c2 = characterShape (chm, c2); c3 = characterShape (chm, c3);
        const double e = d2.run (c0, c1), o = d2.run (c2, c3); return d1.run (e, o);
    }
    void reset() { u1.reset(); u2.reset(); d2.reset(); d1.reset(); }
};

// ---------- motore ----------
class Engine {
public:
    struct Global { double inDb = 0, outDb = 0, autoGainDb = 0, scale = 1; int character = 0; bool bypass = false; int solo = -1;
                    double pan = 0; bool panMS = false; bool invert = false; bool gq = false; };   // pan -1..+1 (L/R o M/S), polarità invertita, Gain-Q
    std::atomic<float> meterDelta[kMaxBands], meterLevel[kMaxBands];

    Engine() { for (int i = 0; i < kMaxBands; ++i) { meterDelta[i] = 0; meterLevel[i] = -120; } }
    void prepare (double sampleRate, int maxBlock) {
        fs = sampleRate; grid.init (fs); N = std::max (maxBlock, 32);
        for (auto* v : { &L, &R, &M, &S, &dL, &dR, &sL, &sR, &sM, &sS }) v->assign ((size_t) N, 0.0);
        for (auto& b : st) { b = BandState(); }
        outG = 1; inG = 1; byp = 0; std::memset (dc, 0, sizeof (dc)); soloSt = SoloState(); os[0].reset(); os[1].reset(); panA = panB = 1; pol = 1;
    }
    // chiamato all'inizio di ogni blocco, dal thread audio
    void setBand (int i, const BandParams& p) {
        BandState& s = st[i]; const bool active = p.used && ! p.bypass;
        const bool sameShape = s.active && active && s.p.type == p.type && s.p.slope == p.slope && s.p.place == p.place;
        const bool changed = ! same (s.p, p) || s.scale != glob.scale;
        s.dynOn = active && p.dyn && canDyn (p.type);
        if (! active) { s.active = false; s.p = p; meterDelta[i] = 0; meterLevel[i] = -120; return; }
        if (! changed && s.active) return;
        s.p = p; s.scale = glob.scale; s.g = hasGain (p.type) ? p.gain * glob.scale : 0;
        designBand (p, s.g + (s.dynOn ? s.delta : 0), grid, s.tgt);
        BandParams dp; detShape (p, dp); designBand (dp, 0, grid, s.det);
        if (s.dynOn) { s.aA = std::exp (-1 / (std::max (0.05, p.att) * 1e-3 * fs)); s.aR = std::exp (-1 / (std::max (1.0, p.rel) * 1e-3 * fs)); }
        else s.delta = 0;
        if (! sameShape) { s.cur = s.tgt; s.fr0 = s.tgt; s.phase = 0; std::memset (s.z, 0, sizeof (s.z)); }
        s.active = true;
    }
    void setGlobal (const Global& g) { glob = g; }
    double sampleRate() const { return fs; }
    const Grid& getGrid() const { return grid; }

    // L/R in place; scL/scR = sidechain (o nullptr)
    void process (float* Lio, float* Rio, const float* scL, const float* scR, int n) {
        for (int i0 = 0; i0 < n; i0 += N) processChunk (Lio + i0, Rio + i0, scL ? scL + i0 : nullptr, scR ? scR + i0 : nullptr, std::min (N, n - i0));
    }

private:
    struct BandState {
        BandParams p; bool active = false, dynOn = false; double scale = 1, g = 0, delta = 0, aA = 0, aR = 0; int phase = 0;   // phase: posizione nel frame dinamico da 32 campioni (indipendente dal blocco della DAW)
        BandCoefs cur, tgt, det, fr0; double z[2][kMaxSecs * 2] = {}; double dz[2][2] = {}; double env[2] = { 1e-12, 1e-12 };
    };
    struct SoloState { int id = -1; BandCoefs c; double z[2][kMaxSecs * 2] = {}; };
    static bool same (const BandParams& a, const BandParams& b) {
        return a.used == b.used && a.bypass == b.bypass && a.type == b.type && a.f == b.f && a.gain == b.gain && a.q == b.q && a.slope == b.slope && a.place == b.place
            && a.dyn == b.dyn && a.thr == b.thr && a.range == b.range && a.att == b.att && a.rel == b.rel && a.knee == b.knee && a.det == b.det && a.sc == b.sc && a.gq == b.gq;
    }
    static void detShape (const BandParams& b, BandParams& d) {
        d = BandParams(); d.used = true; d.f = b.f; d.slope = 12;
        if (b.type == LowShelf) { d.type = HighCut; d.q = std::sqrt (0.5); }
        else if (b.type == HighShelf || b.type == TiltShelf) { d.type = LowCut; d.q = std::sqrt (0.5); }
        else { d.type = BandPass; d.q = std::max (0.3, b.q); }
    }
    static void run (const BandCoefs& c, double* z, double* x, int n) {
        for (int k = 0; k < c.n; ++k) {
            const Coef& q = c.c[k]; double z1 = z[2 * k], z2 = z[2 * k + 1];
            for (int i = 0; i < n; ++i) { const double xi = x[i], y = q.b0 * xi + z1; z1 = q.b1 * xi - q.a1 * y + z2; z2 = q.b2 * xi - q.a2 * y; x[i] = y; }
            z[2 * k] = z1; z[2 * k + 1] = z2;
        }
    }
    static void runLerp (const BandCoefs& c0, const BandCoefs& c1, double* z, double* x, int i0, int i1) {
        const double len = i1 - i0;
        for (int k = 0; k < c1.n; ++k) {
            const Coef& a = c0.c[k]; const Coef& b = c1.c[k]; double z1 = z[2 * k], z2 = z[2 * k + 1];
            double b0 = a.b0, b1 = a.b1, b2 = a.b2, a1 = a.a1, a2 = a.a2;
            const double db0 = (b.b0 - b0) / len, db1 = (b.b1 - b1) / len, db2 = (b.b2 - b2) / len, da1 = (b.a1 - a1) / len, da2 = (b.a2 - a2) / len;
            for (int i = i0; i < i1; ++i) {
                b0 += db0; b1 += db1; b2 += db2; a1 += da1; a2 += da2;
                const double xi = x[i], y = b0 * xi + z1; z1 = b1 * xi - a1 * y + z2; z2 = b2 * xi - a2 * y; x[i] = y;
            }
            z[2 * k] = z1; z[2 * k + 1] = z2;
        }
    }
    // filtra x[i0..i1) con coefficienti che vanno da c0 a c1 lungo un frame di F campioni; il primo campione è alla posizione 'pos' del frame
    static void runFrame (const BandCoefs& c0, const BandCoefs& c1, double* z, double* x, int i0, int i1, int pos, int F) {
        for (int k = 0; k < c1.n; ++k) {
            const Coef& a = c0.c[k]; const Coef& b = c1.c[k]; double z1 = z[2 * k], z2 = z[2 * k + 1];
            const double db0 = (b.b0 - a.b0) / F, db1 = (b.b1 - a.b1) / F, db2 = (b.b2 - a.b2) / F, da1 = (b.a1 - a.a1) / F, da2 = (b.a2 - a.a2) / F;
            double b0 = a.b0 + db0 * pos, b1 = a.b1 + db1 * pos, b2 = a.b2 + db2 * pos, a1 = a.a1 + da1 * pos, a2 = a.a2 + da2 * pos;
            for (int i = i0; i < i1; ++i) {
                b0 += db0; b1 += db1; b2 += db2; a1 += da1; a2 += da2;
                const double xi = x[i], y = b0 * xi + z1; z1 = b1 * xi - a1 * y + z2; z2 = b2 * xi - a2 * y; x[i] = y;
            }
            z[2 * k] = z1; z[2 * k + 1] = z2;
        }
    }
    static bool coefEq (const BandCoefs& a, const BandCoefs& b) { return a.n == b.n && std::memcmp (a.c, b.c, sizeof (Coef) * (size_t) a.n) == 0; }

    int chans (int place, double** ch, double** sc, bool useSC) {
        double* srcL = useSC ? sL.data() : nullptr; double* srcR = useSC ? sR.data() : nullptr;
        switch (place) {
            case Left:  ch[0] = L.data(); sc[0] = srcL; return 1;
            case Right: ch[0] = R.data(); sc[0] = srcR; return 1;
            case Mid:   ch[0] = M.data(); sc[0] = useSC ? sM.data() : nullptr; return 1;
            case Side:  ch[0] = S.data(); sc[0] = useSC ? sS.data() : nullptr; return 1;
            default:    ch[0] = L.data(); ch[1] = R.data(); sc[0] = srcL; sc[1] = srcR; return 2;
        }
    }
    void bandProcess (int bi, BandState& s, int n, bool haveSC) {
        const int place = s.p.place; const bool ms = place == Mid || place == Side;
        if (ms) for (int i = 0; i < n; ++i) { M[(size_t) i] = 0.5 * (L[(size_t) i] + R[(size_t) i]); S[(size_t) i] = 0.5 * (L[(size_t) i] - R[(size_t) i]); }
        double* ch[2]; double* sc[2]; const bool useSC = s.dynOn && s.p.sc && haveSC; const int nc = chans (place, ch, sc, useSC);
        if (! s.dynOn) {
            if (! coefEq (s.cur, s.tgt)) { for (int c = 0; c < nc; ++c) runLerp (s.cur, s.tgt, s.z[c], ch[c], 0, n); s.cur = s.tgt; }
            else for (int c = 0; c < nc; ++c) run (s.cur, s.z[c], ch[c], n);
        } else {
            const BandCoefs& dc_ = s.det; const Coef& q = dc_.c[0];   // detector a una sezione (BP/LP/HP 12 dB)
            constexpr int F = 32;                                    // frame dinamico fisso: il risultato non dipende dal blocco della DAW
            for (int i0 = 0; i0 < n; ) {
                if (s.phase == 0) s.fr0 = s.cur;                    // inizio frame: rampa da cur a tgt (tgt deciso alla fine del frame precedente)
                const int i1 = std::min (n, i0 + (F - s.phase));
                for (int c = 0; c < nc; ++c) {
                    const double* x = useSC ? sc[c] : ch[c]; double e = s.env[c], z1 = s.dz[c][0], z2 = s.dz[c][1];
                    for (int i = i0; i < i1; ++i) {
                        const double xi = x[i], y = q.b0 * xi + z1; z1 = q.b1 * xi - q.a1 * y + z2; z2 = q.b2 * xi - q.a2 * y;
                        const double p = s.p.det == DetPeak ? std::abs (y) * std::abs (y) * 2.0 : y * y;     // Peak: |y|, scalato come una sinusoide RMS
                        e = p > e ? s.aA * (e - p) + p : s.aR * (e - p) + p;
                    }
                    s.dz[c][0] = z1; s.dz[c][1] = z2; s.env[c] = e;
                }
                for (int c = 0; c < nc; ++c) runFrame (s.fr0, s.tgt, s.z[c], ch[c], i0, i1, s.phase, F);
                s.phase += i1 - i0; i0 = i1;
                if (s.phase == F) {                                   // fine frame: livello → nuovo obiettivo per il frame successivo
                    s.phase = 0; s.cur = s.tgt; double lvl = -200;
                    for (int c = 0; c < nc; ++c) lvl = std::max (lvl, 10 * std::log10 (s.env[c] + 1e-20));
                    const double delta = dynDelta (lvl, s.p) * s.scale;
                    if (std::abs (delta - s.delta) > 0.02) { s.delta = delta; designBand (s.p, s.g + delta, grid, s.tgt); }
                    meterLevel[bi].store ((float) lvl, std::memory_order_relaxed);
                }
            }
            meterDelta[bi].store ((float) s.delta, std::memory_order_relaxed);
        }
        if (ms) for (int i = 0; i < n; ++i) { L[(size_t) i] = M[(size_t) i] + S[(size_t) i]; R[(size_t) i] = M[(size_t) i] - S[(size_t) i]; }
    }
    void processChunk (float* Lio, float* Rio, const float* scL, const float* scR, int n) {
        {   // INPUT: guadagno d'ingresso con rampa di 20 ms; dL/dR restano il segnale originale (per il BYPASS)
            const double tgi = std::pow (10.0, glob.inDb / 20.0), gia = 1 - std::exp (-1 / (0.02 * fs)); double gi = inG;
            for (int i = 0; i < n; ++i) { gi += (tgi - gi) * gia; dL[(size_t) i] = Lio[i]; dR[(size_t) i] = Rio[i]; L[(size_t) i] = Lio[i] * gi; R[(size_t) i] = Rio[i] * gi; }
            inG = gi; }
        const bool haveSC = scL != nullptr && scR != nullptr;
        if (haveSC) for (int i = 0; i < n; ++i) { sL[(size_t) i] = scL[i]; sR[(size_t) i] = scR[i]; sM[(size_t) i] = 0.5 * (scL[i] + scR[i]); sS[(size_t) i] = 0.5 * (scL[i] - scR[i]); }
        if (glob.solo >= 0 && glob.solo < kMaxBands && st[glob.solo].active) {
            const BandState& b = st[glob.solo];
            if (soloSt.id != glob.solo || soloSt.c.n == 0) {
                BandParams sp; sp.used = true; sp.f = b.p.f; sp.slope = 12;
                if (b.p.type == LowCut || b.p.type == LowShelf) { sp.type = HighCut; sp.q = std::sqrt (0.5); }
                else if (b.p.type == HighCut || b.p.type == HighShelf || b.p.type == TiltShelf) { sp.type = LowCut; sp.q = std::sqrt (0.5); }
                else { sp.type = BandPass; sp.q = std::max (0.3, b.p.q); }
                designBand (sp, 0, grid, soloSt.c); soloSt.id = glob.solo; std::memset (soloSt.z, 0, sizeof (soloSt.z));
            }
            const int pl = b.p.place;
            for (int i = 0; i < n; ++i) {
                const double m = 0.5 * (L[(size_t) i] + R[(size_t) i]), sd = 0.5 * (L[(size_t) i] - R[(size_t) i]);
                if (pl == Mid) L[(size_t) i] = R[(size_t) i] = m; else if (pl == Side) L[(size_t) i] = R[(size_t) i] = sd;
                else if (pl == Left) R[(size_t) i] = 0; else if (pl == Right) L[(size_t) i] = 0;
            }
            run (soloSt.c, soloSt.z[0], L.data(), n); run (soloSt.c, soloSt.z[1], R.data(), n);
        } else {
            soloSt.id = -1;
            for (int b = 0; b < kMaxBands; ++b) if (st[b].active) bandProcess (b, st[b], n, haveSC);
        }
        // uscita: OUTPUT (−∞ sotto −60 dB) → Character (oversampling 4x) → DC → pan → polarità → bypass morbido
        const bool mute = glob.outDb <= -60; const double tg = mute ? 0.0 : std::pow (10.0, (glob.outDb + glob.autoGainDb) / 20.0), ga = 1 - std::exp (-1 / (0.02 * fs));
        const double bt = glob.bypass ? 1 : 0, ba = 1 - std::exp (-1 / (0.008 * fs)), dcA = std::exp (-2 * kPi * 5 / fs);
        // legge del pan (bilanciamento lineare, centro = 0 dB su entrambi): A = min(1, 1 − p), B = min(1, 1 + p); L/R: A→L, B→R; M/S: A→Mid, B→Side
        const double p = std::clamp (glob.pan, -1.0, 1.0), tA = std::min (1.0, 1 - p), tB = std::min (1.0, 1 + p), tP = glob.invert ? -1.0 : 1.0; const bool ms = glob.panMS;
        double g = outG, by = byp, pa = panA, pb = panB, po = pol; const int chm = glob.character;
        if (chm != lastChm) { os[0].reset(); os[1].reset(); lastChm = chm; }   // cambio di Character: stato dell'oversampling pulito
        for (int i = 0; i < n; ++i) {
            g += (tg - g) * ga; if (mute && g < 1e-7) g = 0; by += (bt - by) * ba; pa += (tA - pa) * ga; pb += (tB - pb) * ga; po += (tP - po) * ba;
            double l = L[(size_t) i] * g, r = R[(size_t) i] * g;
            if (chm) { l = os[0].run (chm, l); r = os[1].run (chm, r);
                const double yl = l - dc[0] + dcA * dc[1]; dc[0] = l; dc[1] = yl; const double yr = r - dc[2] + dcA * dc[3]; dc[2] = r; dc[3] = yr; l = yl; r = yr; }
            if (ms) { const double m = 0.5 * (l + r) * pa, sd = 0.5 * (l - r) * pb; l = m + sd; r = m - sd; } else { l *= pa; r *= pb; }
            l *= po; r *= po;
            l += (dL[(size_t) i] - l) * by; r += (dR[(size_t) i] - r) * by;
            Lio[i] = (float) l; Rio[i] = (float) r;
        }
        outG = g; byp = by; panA = pa; panB = pb; pol = po;
    }

    double fs = 48000; int N = 512; Grid grid; Global glob;
    std::vector<double> L, R, M, S, dL, dR, sL, sR, sM, sS;
    BandState st[kMaxBands]; SoloState soloSt; double outG = 1, inG = 1, byp = 0, dc[4] = {}, panA = 1, panB = 1, pol = 1; Oversampled4x os[2]; int lastChm = 0;
};

} // namespace tsrq
