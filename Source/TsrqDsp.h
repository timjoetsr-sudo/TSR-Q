// TSR Q · DSP core (C++17, header-only, real-time safe: nessuna allocazione nel process)
// Porting 1:1 del core verificato nel prototipo + All Pass, knee, detector Peak/RMS, sidechain.
#pragma once
#include <cmath>
#include <atomic>
#include <algorithm>
#include <vector>
#include <cstring>
#include <complex>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace tsrq {

constexpr double kPi = 3.14159265358979323846;
enum Type { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, FlatTilt, AllPass, NumTypes };
enum Place { Stereo = 0, Left, Right, Mid, Side };
enum DetMode { DetRMS = 0, DetPeak };
constexpr int kMaxBands = 32;
constexpr int kMaxSecs = 16;
constexpr float kMaxIn = 1000.0f;   // 0.4.8: un campione oltre +60 dBFS e un errore dell'host, non audio: diventa 0 come NaN/Inf (prima 1e30 dava 0,6 s di uscita fuori scala)

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
// sezione come SVF trapezoidale: g = tan(pi f / fs), k = 1/Q, uscita = mh*passa-alto + mb*passa-banda(g s) + ml*passa-basso (valori di default = sezione neutra)
struct Svf { double g = 1, k = 2, mh = 1, mb = 2, ml = 1; };
struct SvfSet { int n = 0; Svf s[kMaxSecs]; bool on[kMaxSecs] = {}; };   // on: sezione che in questo momento gira come SVF (le altre come biquad)
inline Coef fromSvf (const Svf& q) {   // inverso di toSvf (s = (1 - z^-1) / (1 + z^-1))
    const double g = q.g, k = q.k, n2 = q.mh, n1 = q.mb * g, n0 = q.ml * g * g, a0 = 1 + k * g + g * g;
    return Coef { (n2 + n1 + n0) / a0, 2 * (n0 - n2) / a0, (n2 - n1 + n0) / a0, 2 * (g * g - 1) / a0, (1 - k * g + g * g) / a0 };
}
inline bool firstOrd (const Coef& c) { return std::abs (c.a2) < 1e-12 && std::abs (c.b2) < 1e-12; }   // sezione del primo ordine (un polo): in SVF diventerebbe polo + zero cancellati
    // stato equivalente tra le due strutture (stessa H(z) nell'istante del passaggio): si eguagliano le prime due uscite a ingresso nullo.
// Sezione non osservabile (neutra, polo cancellato da uno zero): lo stato non conta e va a zero.
inline void svfZir (const Svf& q, double i1, double i2, double& y0, double& y1) {   // prime due uscite a ingresso nullo dallo stato (i1, i2)
    const double h1 = 1 / (1 + q.g * (q.g + q.k)), h2 = q.g * h1, h3 = q.g * h2, c1 = q.mb - q.mh * q.k, c2 = q.ml - q.mh;
    double v1 = h1 * i1 - h2 * i2, v2 = h2 * i1 + (1 - h3) * i2; y0 = c1 * v1 + c2 * v2; const double j1 = 2 * v1 - i1, j2 = 2 * v2 - i2;
    v1 = h1 * j1 - h2 * j2; v2 = h2 * j1 + (1 - h3) * j2; y1 = c1 * v1 + c2 * v2; }
inline void tdfToSvf (const Coef& c, const Svf& q, double* z) {
    const double y0 = z[0], y1 = -c.a1 * z[0] + z[1]; double p0, p1, r0, r1; svfZir (q, 1, 0, p0, p1); svfZir (q, 0, 1, r0, r1);
    // soglie lontane dai denormali: con FTZ/DAZ attivi (ScopedNoDenormals nel plugin) 1e-12 * 1e-300 diventava 0 e si divideva 0 per 0 (NaN trovato da pluginval)
    const double m = std::max ({ std::abs (p0), std::abs (p1), std::abs (r0), std::abs (r1) }), det = p0 * r1 - r0 * p1;
    if (! (m > 1e-9)) { z[0] = z[1] = 0; return; }   // l'uscita non dipende dallo stato (sezione neutra, b = a)
    if (! (std::abs (det) > 1e-9 * m * m)) {   // stato in parte non osservabile: si conserva la parte che conta (pseudo-inversa a norma minima)
        const double nn = p0 * p0 + p1 * p1 + r0 * r0 + r1 * r1; z[0] = (p0 * y0 + p1 * y1) / nn; z[1] = (r0 * y0 + r1 * y1) / nn; return; }
    z[0] = (y0 * r1 - r0 * y1) / det; z[1] = (p0 * y1 - y0 * p1) / det; }
inline void svfToTdf (const Coef& c, const Svf& q, double* z) {
    double y0, y1; svfZir (q, z[0], z[1], y0, y1); z[0] = y0; z[1] = y1 + c.a1 * y0; }
// biquad stabile -> SVF con la stessa H(z): sostituzione bilineare z = (1 + s) / (1 - s) su numeratore e denominatore
// (stabilita: D(1) > 0 e D(-1) > 0, quindi g reale e positivo)
inline Svf toSvf (const Coef& c) {
    const double A = 1 - c.a1 + c.a2, B = 2 * (1 - c.a2), C = 1 + c.a1 + c.a2, g = std::sqrt (C / A);
    return Svf { g, B / (A * g), (c.b0 - c.b1 + c.b2) / A, 2 * (c.b0 - c.b2) / (A * g), (c.b0 + c.b1 + c.b2) / (A * g * g) };
}

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
// Definizioni di forma uguali a FabFilter Pro-Q 4 (0.6.0): costanti ricavate dalle risposte di Pro-Q 4 misurate a 192 kHz sul Mac di Tim
// (docs/test_0.6.0). Valgono nell'intervallo misurato; fuori (Q < 0.3 o > 10 per shelf e tilt) sono [NON VERIFICATO].
constexpr double kPqShelfC = 0.70335, kPqShelfP = 0.46594;   // shelf e tilt a 12 dB/oct: Q effettivo = 0.70335 * Q^0.46594 (Q = 0.3 ... 10)
constexpr double kPqCutQMax = 28.2843;                        // taglio: Q della sezione piu risonante al massimo 28.28 (misurato a 72 e 96 dB/oct con Q 10)
inline double pqShelfQ (double Q) { return kPqShelfC * std::pow (Q, kPqShelfP); }
inline double pqCutZero (int slope, double Q) {               // zero aggiunto ai tagli (Pro-Q: pendenza piu dolce lontano dal taglio); 0 = nessuno
    switch (slope) { case 12: return 0.0871 / std::min (Q, 2.78); case 18: return 0.0779; case 24: return 0.0843; case 36: return 0.0838; default: return 0; } }
inline bool pqCutMeasured (int slope) { return slope == 12 || slope == 18 || slope == 24 || slope == 36 || slope == 48 || slope == 72 || slope == 96; }
inline void sectionsFor (const BandParams& b, double gainDb, SecSet& o) {
    o.n = 0; o.k = 1;
    const double f = b.f, Q = b.q, A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
    switch (b.type) {
        case Bell: { const double Qe = (b.gq ? std::min (40.0, Q * (1.0 + std::abs (gainDb) / 15.0)) : Q) / std::sqrt (2.0); push (o, f, 1, A / Qe, 1, 1, 1 / (A * Qe), 1); break; }
        case LowShelf:  if (b.slope <= 6) push (o, f, A * A, A, 0, 1, A, 0); else { const double q = pqShelfQ (Q); push (o, f, A * A, A * sA / q, A, 1, sA / q, A); } break;
        case HighShelf: if (b.slope <= 6) push (o, f, A, A * A, 0, A, 1, 0); else { const double q = pqShelfQ (Q); push (o, f, A, A * sA / q, A * A, A, sA / q, 1); } break;
        case TiltShelf:   // 12 dB/oct e oltre: high shelf di 2 x guadagno abbassato del guadagno (da -g a +g, come Pro-Q 4); 6 dB/oct: primo ordine (da -g/2 a +g/2) [forma di Pro-Q NON VERIFICATA]
            if (b.slope <= 6) push (o, f, 1, A, 0, A, 1, 0);
            else { const double T = A * A, sT = A, q = pqShelfQ (Q); push (o, f, T, T * sT / q, T * T, T, sT / q, 1); o.k = 1 / T; }
            break;
        case Notch:     push (o, f, 1, 0, 1, 1, std::sqrt (2.0) / Q, 1); break;
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
            // pendenze misurate su Pro-Q 4 (12, 18, 24, 36, 48, 72, 96): Butterworth con il Q utente sulla sezione piu risonante (Q 1 = Butterworth) e lo zero di Pro-Q;
            // 6 e 30 dB/oct: Butterworth senza Q (forma di Pro-Q NON VERIFICATA)
            const bool hp = b.type == LowCut, pq = pqCutMeasured (b.slope); const double a = pq ? pqCutZero (b.slope, Q) : 0;
            if (N % 2) { if (hp) push (o, f, 0, 1, 0, 1, 1, 0); else push (o, f, 1, 0, 0, 1, 1, 0); }
            for (int i = 0; i < nq; ++i) {
                const double qe = (pq && i == nq - 1) ? std::min (qs[i] * Q, kPqCutQMax) : qs[i], z = i == 0 ? a : 0;
                if (hp) push (o, f, 0, z, 1, 1, 1 / qe, 1); else push (o, f, 1, z, 0, 1, 1 / qe, 1); }
            break;
        }
        case FlatTilt: {
            const double S = 2 * gainDb / std::log2 (1000.0), D = 1.5, r = std::pow (10.0, S * D / 20.0);
            for (double z = 4; z < 60000; z *= std::pow (2.0, D)) push (o, z, 1, 1, 0, 1, 1 / r, 0);
            o.k = 1.0 / std::sqrt (anaMag2 (o, f));
            break;
        }
        default: break;
    }
}

// ---------- progetto digitale ----------
// Modulo del biquad in forma phi (p1 = sin^2(w/2), p0 = cos^2(w/2), p2 = 4 p0 p1 = sin^2 w): nessuna cancellazione catastrofica.
// (0.4.3 usava la forma con cos w e cos 2w: per filtri stretti sotto ~50 Hz a 96-192 kHz perdeva tutte le cifre significative e la scelta
//  del progetto migliore poteva essere sbagliata: misurato.)
struct Phi { double p0 = 1, p1 = 0, p2 = 0; };
inline Phi phiAt (double fr, double fs) { const double h = kPi * fr / fs, s = std::sin (h), c = std::cos (h); Phi q; q.p1 = s * s; q.p0 = c * c; q.p2 = 4 * q.p0 * q.p1; return q; }
inline double mag2phi (const Coef& c, const Phi& q) {
    const double s0 = c.b0 + c.b1 + c.b2, s1 = c.b0 - c.b1 + c.b2, d0 = 1 + c.a1 + c.a2, d1 = 1 - c.a1 + c.a2;
    return (s0 * s0 * q.p0 + s1 * s1 * q.p1 - 4 * c.b0 * c.b2 * q.p2) / (d0 * d0 * q.p0 + d1 * d1 * q.p1 - 4 * c.a2 * q.p2);
}
inline double coefMag2 (const Coef& c, double fr, double fs) { return mag2phi (c, phiAt (fr, fs)); }
inline double magDC (const Coef& c) { const double s0 = c.b0 + c.b1 + c.b2, d0 = 1 + c.a1 + c.a2; return (s0 * s0) / (d0 * d0); }
inline double magNy (const Coef& c) { const double s1 = c.b0 - c.b1 + c.b2, d1 = 1 - c.a1 + c.a2; return (s1 * s1) / (d1 * d1); }

// griglia di valutazione: 64 punti logaritmici nella banda udibile (20 Hz - min(20 kHz, 0.98 Nyquist)) + 5 punti sopra (peso basso nel fit)
constexpr int kEval = 64, kOut = 5, kFitMax = kEval + 17 + kOut, kNarrow = 26, kFitCap = kFitMax + kNarrow, kDense = 160;
constexpr double kOct[17] = { 0.7071067811865476, 0.7384130729697497, 0.7711054127039704, 0.8052451659746271, 0.8408964152537145, 0.8781260801866497,
                              0.9170040432046712, 0.9576032806985737, 1.0, 1.0442737824274138, 1.0905077326652577, 1.1387886347566916, 1.189207115002721,
                              1.241857812073484, 1.2968395546510096, 1.3542555469368927, 1.4142135623730951 };   // 2^(k/16), k = -8..8
struct Grid {
    double fs = 0, top = 20000; double ef[kEval], of[kOut], f32[32], df[kDense]; Phi ep[kEval], op[kOut], p32[32], dp[kDense];   // dp/df: griglia fitta per la scelta finale
    void init (double sampleRate) {
        fs = sampleRate; top = std::min (20000.0, fs / 2 * 0.98);
        for (int i = 0; i < 32; ++i) { f32[i] = 10.0 * std::pow (fs / 2 * 0.999 / 10.0, i / 31.0); p32[i] = phiAt (f32[i], fs); }
        for (int i = 0; i < kEval; ++i) { ef[i] = 20.0 * std::pow (top / 20.0, i / (double) (kEval - 1)); ep[i] = phiAt (ef[i], fs); }
        for (int i = 0; i < kOut; ++i) { of[i] = top + (fs / 2 * 0.999 - top) * (i + 1) / kOut; op[i] = phiAt (of[i], fs); }
        for (int i = 0; i < kDense; ++i) { df[i] = 20.0 * std::pow (top / 20.0, (i + 0.5) / kDense); dp[i] = phiAt (df[i], fs); }
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
inline bool fromB (double B0, double B1, double B2, double* out) {
    if (B0 < 0 && B0 > -1e-12) B0 = 0;   // zero numerico (es. passa-alto: |N(1)|^2 = 0): stesso esito in JS e C++
    if (B1 < 0 && B1 > -1e-12) B1 = 0;
    if (! (B0 >= 0) || ! (B1 >= 0)) return false;
    const double s0 = std::sqrt (B0), s1 = std::sqrt (B1), W = 0.5 * (s0 + s1), d = W * W + B2;
    if (! (d >= 0)) return false;
    const double b0 = 0.5 * (W + std::sqrt (d));
    out[0] = b0; out[1] = 0.5 * (s0 - s1); out[2] = b0 > 0 ? -B2 / (4 * b0) : 0; return true;
}
// come fromB ma proiettato sul numeratore fattorizzabile piu vicino (B0, B1 >= 0; se serve zeri sul cerchio unitario): usato dal fit minimax,
// dove il bersaglio ha un buco vicino a DC o a Nyquist (shelf con Q alto: zeri a f0/sqrt(A) oltre 20 kHz) e la soluzione lineare esce di poco dal dominio
inline void fromBp (double B0, double B1, double B2, double* out) {
    B0 = std::max (B0, 0.0); B1 = std::max (B1, 0.0);
    const double s0 = std::sqrt (B0), s1 = std::sqrt (B1), W = 0.5 * (s0 + s1); double d = W * W + B2; if (d < 0) { d = 0; B2 = -W * W; }
    const double b0 = 0.5 * (W + std::sqrt (d)); out[0] = b0; out[1] = 0.5 * (s0 - s1); out[2] = b0 > 0 ? -B2 / (4 * b0) : 0;
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
inline Coef designMZ (const Sec& sc, double fs) {   // matched-z; guadagno allineato esattamente a DC (o a Nyquist per i passa-alto)
    Coef c; const double w0 = 2 * kPi * sc.f0 / fs; poles (sc.d, w0, c.a1, c.a2);
    double z[3]; zerosPoly (sc.n, w0, z); c.b0 = z[0]; c.b1 = z[1]; c.b2 = z[2];
    const bool dc = secMag2 (sc, 0) > 1e-12; const double dg = dc ? magDC (c) : magNy (c), ag = dc ? secMag2 (sc, 0) : secMag2 (sc, fs / 2);
    const double g = dg > 0 ? std::sqrt (ag / dg) : 1;
    c.b0 *= g; c.b1 *= g; c.b2 *= g; return c;
}
inline bool solveN (double* M, double* v, double* x, int N) {   // eliminazione di Gauss con pivot parziale (N <= 5)
    for (int i = 0; i < N; ++i) {
        int p = i; for (int r = i + 1; r < N; ++r) if (std::abs (M[r * N + i]) > std::abs (M[p * N + i])) p = r;
        if (p != i) { for (int c = 0; c < N; ++c) std::swap (M[i * N + c], M[p * N + c]); std::swap (v[i], v[p]); }
        const double dd = M[i * N + i]; if (! (std::abs (dd) > 1e-300)) return false;
        for (int r = i + 1; r < N; ++r) { const double fct = M[r * N + i] / dd; for (int c = i; c < N; ++c) M[r * N + c] -= fct * M[i * N + c]; v[r] -= fct * v[i]; }
    }
    for (int i = N - 1; i >= 0; --i) { double s = v[i]; for (int c = i + 1; c < N; ++c) s -= M[i * N + c] * x[c]; x[i] = s / M[i * N + i]; }
    return true;
}
inline bool stable (const Coef& c) { return std::isfinite (c.b0 + c.b1 + c.b2 + c.a1 + c.a2) && std::abs (c.a2) < 1 && std::abs (c.a1) < 1 + c.a2; }
inline Coef withZeros (const Coef& c, const Sec& sc, double fs) {
    const double w0 = 2 * kPi * sc.f0 / fs * std::sqrt (sc.n[0] / sc.n[2]); Coef q = c; q.b0 = 1; q.b1 = -2 * std::cos (w0); q.b2 = 1;
    const double g = std::sqrt (secMag2 (sc, 0) / magDC (q)); q.b0 = g; q.b1 *= g; q.b2 = g; return q;
}
// errore massimo |dB| tra modulo digitale e analogico (rapporto massimo/minimo, un solo logaritmo)
struct ErrAcc {   // punti sopra -30 dB: errore pieno; sotto (banda attenuata): conta solo cio che supera una tolleranza di 6 dB
    double rmax = 1, rmin = 1, deep = 1; bool bad = false;
    void add (double d, double a) {
        if (! (d == d)) { bad = true; return; }
        if (a < 1e-3) { const double r = std::max (d, 1e-6) / std::max (a, 1e-6), x = (r > 1 ? r : 1 / r) / 3.981071705534973; if (x > deep) deep = x; return; }
        const double r = std::max (d, 1e-12) / a; if (! (r > 0 && r < 1e300)) { bad = true; return; } if (r > rmax) rmax = r; if (r < rmin) rmin = r; }
    double db() const { return bad ? 1e9 : 10 * std::log10 (std::max (std::max (rmax, 1 / rmin), deep)); }
};
// risonanze strette (Q > 4): 13 punti attorno alla frequenza dei poli e, se e diversa, 13 attorno a quella degli zeri, a passi di 1/(4Q) di ottava (+/- 1.5/Q ottave).
// (shelf con Q alto: il buco degli zeri sta lontano dai poli, a f0/sqrt(A) o f0*sqrt(A); prima della 0.4.4 non era coperto e l'errore li veniva sottostimato)
inline int narrowPts (const Sec& sc, double* fr) {
    const double Qd = sc.d[2] > 0 && sc.d[1] > 0 ? std::sqrt (sc.d[0] * sc.d[2]) / sc.d[1] : 0, Qn = sc.n[2] > 0 && sc.n[1] > 0 ? std::sqrt (sc.n[0] * sc.n[2]) / sc.n[1] : 0;
    const double Q = std::max (Qd, Qn), fp = sc.d[2] > 0 ? sc.f0 * std::sqrt (sc.d[0] / sc.d[2]) : 0; int c = 0;
    if (Q > 4 && sc.d[2] > 0) for (int k = -6; k <= 6; ++k) fr[c++] = fp * std::pow (2.0, k / (4 * Q));
    if (Qn > 4) { const double fz = sc.f0 * std::sqrt (sc.n[0] / sc.n[2]); if (! (fp > 0 && std::abs (fz / fp - 1) < 0.02)) for (int k = -6; k <= 6; ++k) fr[c++] = fz * std::pow (2.0, k / (4 * Qn)); }
    return c;
}
// punti di valutazione di una sezione (griglia udibile + 17 attorno a f0 + 9 stretti) con il modulo analogico: calcolati UNA volta per progetto
struct EvalSet { int n = 0; Phi q[kEval + 17 + kNarrow]; double T[kEval + 17 + kNarrow]; };
inline void buildEval (const Sec& sc, const Grid& g, EvalSet& E) {
    E.n = 0;
    for (int i = 0; i < kEval; ++i) { E.q[E.n] = g.ep[i]; E.T[E.n] = secMag2 (sc, g.ef[i]); ++E.n; }
    for (int k = 0; k < 17; ++k) { const double fr = sc.f0 * kOct[k]; if (fr > 20 && fr < g.top) { E.q[E.n] = phiAt (fr, g.fs); E.T[E.n] = secMag2 (sc, fr); ++E.n; } }
    double nf[kNarrow]; const int nn = narrowPts (sc, nf); for (int k = 0; k < nn; ++k) if (nf[k] > 20 && nf[k] < g.top) { E.q[E.n] = phiAt (nf[k], g.fs); E.T[E.n] = secMag2 (sc, nf[k]); ++E.n; }
}
inline double evalErr (const Coef& c, const EvalSet& E) { ErrAcc e; for (int i = 0; i < E.n; ++i) e.add (mag2phi (c, E.q[i]), E.T[i]); return e.db(); }
inline double secErr (const Coef& c, const Sec& sc, const Grid& g) { EvalSet E; buildEval (sc, g, E); return evalErr (c, E); }
// ---- fit minimax di un biquad su un modulo bersaglio, SOLO nella banda udibile (sopra: peso 0.02), pesi di Lawson ----
struct FitPts { int n = 0; Phi q[kFitCap]; double T[kFitCap], W0[kFitCap]; };
struct EvalSet;
// peso nel fit: banda sopra -30 dB 1; tra -30 e -40 dB 0,1 (non trascurata: fianchi dei passa-banda vicino a 20 kHz); sotto 0,001
inline double deepW (double T) { return T > 1e-3 ? 1.0 : T > 1e-4 ? 0.1 : 1e-3; }
inline void addPt (FitPts& P, const Phi& q, double T, double w) { if (P.n < kFitCap && T > 1e-6 && T < 1e12) { P.q[P.n] = q; P.T[P.n] = T; P.W0[P.n] = w; ++P.n; } }
inline void secFit (const Sec& sc, const Grid& g, const EvalSet& E, FitPts& P) {
    P.n = 0;
    for (int i = 0; i < E.n; ++i) addPt (P, E.q[i], E.T[i], deepW (E.T[i]));
    for (int i = 0; i < kOut; ++i) addPt (P, g.op[i], secMag2 (sc, g.of[i]), 0.02);
}
// griglia della 0.4.3 (24 punti fino a 0.999 Nyquist + 9 attorno a f0, tutti con peso 1): candidato "0.4.3" mantenuto, cosi il nuovo progetto non e mai peggiore
inline void secFitOld (const Sec& sc, const Grid& g, FitPts& P) {
    P.n = 0; const double ny = g.fs / 2 * 0.999;
    for (int i = 0; i < 32; ++i) if (i % 4 != 3) { if (P.n < kFitMax) { P.q[P.n] = g.p32[i]; P.T[P.n] = secMag2 (sc, g.f32[i]); P.W0[P.n] = 1; ++P.n; } }
    for (int i = -4; i <= 4; ++i) { const double fr = sc.f0 * kOct[8 + 2 * i]; if (fr > 5 && fr < ny && P.n < kFitMax) { P.q[P.n] = phiAt (fr, g.fs); P.T[P.n] = secMag2 (sc, fr); P.W0[P.n] = 1; ++P.n; } }
}
// errore per punto come rapporto (r - 1 o 1/r - 1, monotono con |dB|) + aggiornamento di Lawson; restituisce il massimo in banda
inline double lawson (const FitPts& P, const Coef& c, double* wl) {
    double e[kFitCap], emax = 0, esum = 0; int nin = 0;
    for (int i = 0; i < P.n; ++i) { const double r = mag2phi (c, P.q[i]) / P.T[i]; double ei = r >= 1 ? r - 1 : 1 / r - 1; if (! (ei >= 0 && ei < 1e300)) return 1e300; e[i] = ei; if (P.W0[i] >= 1) { if (ei > emax) emax = ei; esum += ei; ++nin; } }
    if (nin == 0) return 1e300;
    const double em = esum / nin + 1e-12; double wsum = 0;
    for (int i = 0; i < P.n; ++i) { double wn = wl[i] * (e[i] / em + 1e-3); if (P.W0[i] < 1 && wn > P.W0[i]) wn = P.W0[i]; wl[i] = wn; if (P.W0[i] >= 1) wsum += wn; }
    const double wm = wsum / nin; for (int i = 0; i < P.n; ++i) wl[i] /= wm;
    return emax;
}
inline bool fitIRLS (const FitPts& P, const Coef& init, int iters, Coef& out, double& outErr, bool law = true) {
    double a1 = init.a1, a2 = init.a2, wl[kFitCap]; for (int i = 0; i < P.n; ++i) wl[i] = P.W0[i];
    bool ok = false; outErr = 1e300; int stall = 0;
    for (int it = 0; it < iters; ++it) {
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2); if (! (A0 > 1e-14)) break;
        const double A1 = (1 - a1 + a2) * (1 - a1 + a2) / A0, A2 = -4 * a2 / A0;
        double M[25] = {}, V[5] = {}, x[5];
        for (int i = 0; i < P.n; ++i) {
            const Phi& q = P.q[i]; const double T = P.T[i], D = q.p0 + A1 * q.p1 + A2 * q.p2, td = T * D, w = wl[i] / (td * td + (law ? 1e-300 : 1e-24)), y = T * q.p0;
            const double rw[5] = { q.p0, q.p1, q.p2, -T * q.p1, -T * q.p2 };
            for (int r = 0; r < 5; ++r) { const double wr = w * rw[r]; V[r] += wr * y; for (int c = r; c < 5; ++c) M[r * 5 + c] += wr * rw[c]; }
        }
        for (int r = 1; r < 5; ++r) for (int c = 0; c < r; ++c) M[r * 5 + c] = M[c * 5 + r];
        if (! solveN (M, V, x, 5)) break;
        double den[3], num[3]; if (! fromB (1, x[3], x[4], den)) break;
        if (law) fromBp (x[0], x[1], x[2], num); else if (! fromB (x[0], x[1], x[2], num)) break;   // 0.4.3 (law = false): invariato
        const double na1 = den[1] / den[0], na2 = den[2] / den[0];
        if (! (std::abs (na2) < 0.99999 && std::abs (na1) < 1 + na2)) break;
        a1 = na1; a2 = na2; Coef c; c.b0 = num[0] / den[0]; c.b1 = num[1] / den[0]; c.b2 = num[2] / den[0]; c.a1 = a1; c.a2 = a2;
        if (! law) { out = c; ok = true; continue; }
        const double em = lawson (P, c, wl); if (! (em < 1e300)) break;
        if (em < outErr) { outErr = em; out = c; ok = true; stall = 0; } else if (++stall >= 2) break;
        if (outErr < 0.0023) break;   // gia sotto 0.01 dB
    }
    return ok;
}
// come fitIRLS ma con gli zeri fissati sul cerchio unitario (Notch): numeratore g (1, -2 cz, 1)
inline bool fitIRLSz (const FitPts& P, double cz, const Coef& init, int iters, Coef& out, double& outErr) {
    const double s0 = 2 - 2 * cz, s1 = 2 + 2 * cz; double a1 = init.a1, a2 = init.a2, wl[kFitCap]; for (int i = 0; i < P.n; ++i) wl[i] = P.W0[i];
    bool ok = false; outErr = 1e300; int stall = 0;
    for (int it = 0; it < iters; ++it) {
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2); if (! (A0 > 1e-14)) break;
        const double A1 = (1 - a1 + a2) * (1 - a1 + a2) / A0, A2 = -4 * a2 / A0;
        double M[9] = {}, V[3] = {}, x[3];
        for (int i = 0; i < P.n; ++i) {
            const Phi& q = P.q[i]; const double T = P.T[i], N0 = s0 * s0 * q.p0 + s1 * s1 * q.p1 - 4 * q.p2, D = q.p0 + A1 * q.p1 + A2 * q.p2, td = T * D, w = wl[i] / (td * td + 1e-300), y = T * q.p0;
            const double rw[3] = { N0, -T * q.p1, -T * q.p2 };
            for (int r = 0; r < 3; ++r) { const double wr = w * rw[r]; V[r] += wr * y; for (int c = 0; c < 3; ++c) M[r * 3 + c] += wr * rw[c]; }
        }
        if (! solveN (M, V, x, 3) || ! (x[0] > 0)) break;
        double den[3]; if (! fromB (1, x[1], x[2], den)) break;
        const double na1 = den[1] / den[0], na2 = den[2] / den[0];
        if (! (std::abs (na2) < 0.99999 && std::abs (na1) < 1 + na2)) break;
        a1 = na1; a2 = na2; const double gg = std::sqrt (x[0]) / den[0];
        Coef c; c.b0 = gg; c.b1 = -2 * cz * gg; c.b2 = gg; c.a1 = a1; c.a2 = a2;
        const double em = lawson (P, c, wl); if (! (em < 1e300)) break;
        if (em < outErr) { outErr = em; out = c; ok = true; stall = 0; } else if (++stall >= 2) break;
        if (outErr < 0.0023) break;
    }
    return ok;
}
// tre candidati veloci (matched-z, 3 punti) e, solo se l'errore supera 0.01 dB, il fit minimax nella banda udibile
inline Coef designSec (const Sec& sc, const Grid& g, bool precise = true) {
    const bool jw = sc.n[1] == 0 && sc.n[0] > 0 && sc.n[2] > 0;
    EvalSet E; buildEval (sc, g, E);
    Coef mz = designMZ (sc, g.fs), best = mz; double be = stable (mz) ? evalErr (mz, E) : 1e9;
    if (be < 0.01) return best;
    Coef c3 = design3pt (sc, g.fs); if (jw) c3 = withZeros (c3, sc, g.fs);
    const double e3 = stable (c3) ? evalErr (c3, E) : 1e9; if (e3 < be) { best = c3; be = e3; }
    if (be < 0.01) return best;
    if (sc.n[0] == 0 && sc.n[1] == 0) return best;   // passa-alto puro: il fit e mal condizionato (|N(1)|^2 = 0)
    FitPts P; Coef ci; double eci;
    secFitOld (sc, g, P);   // candidato della 0.4.3: IRLS fino a Nyquist, 4 iterazioni, poli iniziali dal matched-z
    if (fitIRLS (P, stable (mz) ? mz : c3, 4, ci, eci, false)) { if (jw) ci = withZeros (ci, sc, g.fs); if (stable (ci)) { const double ei = evalErr (ci, E); if (ei < be) { best = ci; be = ei; } } }
    if (! precise && be < 1.0) return best;   // veloce: qualita e costo della 0.4.3, salvo errori grossi (poli analogici oltre Nyquist)
    if (be < 0.03 || ! stable (best)) return best;   // sotto 0.03 dB il fit minimax non vale il suo costo
    secFit (sc, g, E, P);   // candidati 0.4.4: minimax nella sola banda udibile
    const double cz = jw ? std::cos (2 * kPi * sc.f0 / g.fs * std::sqrt (sc.n[0] / sc.n[2])) : 0;
    Coef cand[4]; int nc = 0; cand[nc++] = best;
    auto fit = [&] (const Coef init) { Coef c; double e; const bool ok = jw ? fitIRLSz (P, cz, init, 10, c, e) : fitIRLS (P, init, 10, c, e);
        if (ok && stable (c)) { cand[nc++] = c; const double ei = evalErr (c, E); if (ei < be) { best = c; be = ei; } } };
    fit (best);   // 1) dai poli del candidato migliore
    if (be > 0.05) { Coef z0 = best; z0.a1 = 0; z0.a2 = 0; fit (z0); }   // 2) da poli nulli (pesi iniziali uniformi): non dipende dalla mappatura dei poli,
                                                                        //    che fallisce quando i poli analogici stanno oltre Nyquist (shelf alti, Q alto: 0.4.3 fino a +19 dB a 20 kHz)
    if (nc < 2 || be < 0.03) return best;
    // scelta finale tra i candidati su E + griglia fitta di 160 punti (i punti del fit sono pochi: il massimo vero puo cadere tra due punti)
    double Td[kDense]; for (int i = 0; i < kDense; ++i) Td[i] = secMag2 (sc, g.df[i]);
    double bd = 1e300;
    for (int k = 0; k < nc; ++k) { ErrAcc e; for (int i = 0; i < E.n; ++i) e.add (mag2phi (cand[k], E.q[i]), E.T[i]); for (int i = 0; i < kDense; ++i) e.add (mag2phi (cand[k], g.dp[i]), Td[i]);
        const double ek = e.db(); if (ek < bd) { bd = ek; best = cand[k]; } }
    return best;
}
// cascate (Low/High Cut, Band Pass, Flat Tilt): conta l'errore del TOTALE, non quello di ogni sezione.
// (0.4.3 sceglieva sezione per sezione: errori di 0.5 dB ciascuna dove il totale e gia sotto -30 dB non contano, quelli in banda si sommano.)
// 1) per ogni sezione tre candidati (matched-z, 3 punti, IRLS); 2) combinazione scelta sull'errore del totale (ricerca a coordinate);
// 3) se il totale supera ancora 0.02 dB: ogni sezione non passa-alto riadattata al bersaglio totale diviso per le altre (minimax, 2 passate).
constexpr int kCasPts = 112;
inline void designCascade (const SecSet& set, const Grid& g, BandCoefs& bc, bool refine, int start = -1) {   // start >= 0: partenza forzata dal candidato start di ogni sezione (correzione)
    const int n = set.n; Coef cand[kMaxSecs][3]; bool ok[kMaxSecs][3];
    // punti: la griglia udibile + 17 punti (passo 1/16 di ottava) attorno a ogni frequenza di sezione distinta + punti stretti (Q > 4)
    Phi Ep[kCasPts]; double Et[kCasPts]; int ne = 0;
    for (int i = 0; i < kEval; ++i) { Ep[ne] = g.ep[i]; Et[ne] = anaMag2 (set, g.ef[i]); ++ne; }
    for (int k = 0; k < n && ne < kCasPts; ++k) {
        bool dup = set.s[k].d[2] == 0; for (int j = 0; j < k; ++j) if (set.s[j].f0 == set.s[k].f0) dup = true; if (dup) continue;   // sezioni del 1° ordine: larghe, bastano i 64 punti
        for (int q = 0; q < 17 && ne < kCasPts; ++q) { const double fr = set.s[k].f0 * kOct[q]; if (fr > 20 && fr < g.top) { Ep[ne] = phiAt (fr, g.fs); Et[ne] = anaMag2 (set, fr); ++ne; } }
        double nf[kNarrow]; const int nn = narrowPts (set.s[k], nf); for (int q = 0; q < nn && ne < kCasPts; ++q) if (nf[q] > 20 && nf[q] < g.top) { Ep[ne] = phiAt (nf[q], g.fs); Et[ne] = anaMag2 (set, nf[q]); ++ne; }
    }
    double To[kOut]; for (int i = 0; i < kOut; ++i) To[i] = anaMag2 (set, g.of[i]);
    static thread_local double mg[kMaxSecs][3][kCasPts];   // 43 KB fuori dallo stack del thread audio
    for (int k = 0; k < n; ++k) {
        const Sec& sc = set.s[k]; const bool hp = sc.n[0] == 0 && sc.n[1] == 0;
        cand[k][0] = designMZ (sc, g.fs); cand[k][1] = design3pt (sc, g.fs); ok[k][2] = false;
        if (! hp) { FitPts P; secFitOld (sc, g, P); Coef ci; double ei; if (fitIRLS (P, stable (cand[k][0]) ? cand[k][0] : cand[k][1], 4, ci, ei, false)) { cand[k][2] = ci; ok[k][2] = stable (ci); } }
        for (int j = 0; j < 3; ++j) {
            if (j < 2) ok[k][j] = stable (cand[k][j]);
            if (! ok[k][j]) continue;
            if (k == 0) { cand[k][j].b0 *= set.k; cand[k][j].b1 *= set.k; cand[k][j].b2 *= set.k; }
            for (int i = 0; i < ne; ++i) mg[k][j][i] = mag2phi (cand[k][j], Ep[i]);
        }
    }
    int sel[kMaxSecs];
    auto tot = [&] (const int* sl) { ErrAcc e; for (int i = 0; i < ne; ++i) { double d = 1; for (int k = 0; k < n; ++k) d *= mg[k][sl[k]][i]; e.add (d, Et[i]); } return e.db(); };
    double best = 1e300;
    for (int j = 0; j < 3; ++j) { if (start >= 0 && j != start) continue; int sl[kMaxSecs]; bool all = true; for (int k = 0; k < n; ++k) { sl[k] = ok[k][j] ? j : (ok[k][0] ? 0 : 1); all = all && (ok[k][0] || ok[k][1]); }
        if (! all) continue; const double e = tot (sl); if (e < best) { best = e; for (int k = 0; k < n; ++k) sel[k] = sl[k]; } }
    if (! (best < 1e300)) { for (int k = 0; k < n; ++k) bc.c[k] = cand[k][0]; bc.n = n; return; }
    for (int pass = 0; pass < (start >= 0 ? 0 : 2); ++pass) { bool moved = false; for (int k = 0; k < n; ++k) for (int j = 0; j < 3; ++j) {
        if (j == sel[k] || ! ok[k][j]) continue; const int keep = sel[k]; sel[k] = j; const double e = tot (sel); if (e < best) { best = e; moved = true; } else sel[k] = keep; }
        if (! moved) break; }
    bc.n = n; for (int k = 0; k < n; ++k) bc.c[k] = cand[k][sel[k]];
    if (! refine || best < 0.05) return;
    auto err = [&] (const BandCoefs& b) { ErrAcc e; for (int i = 0; i < ne; ++i) { double d = 1; for (int k = 0; k < b.n; ++k) d *= mag2phi (b.c[k], Ep[i]); e.add (d, Et[i]); } return e.db(); };
    double e0 = best;
    for (int pass = 0; pass < 2; ++pass) { if (pass == 1 && e0 < 0.2) break; for (int k = 0; k < n; ++k) {
        if (set.s[k].n[0] == 0 && set.s[k].n[1] == 0) continue;
        FitPts P; P.n = 0;
        for (int i = 0; i < ne && P.n < kFitMax + 9 - kOut; ++i) if (Et[i] > 1e-6) { double o = 1; for (int j = 0; j < n; ++j) if (j != k) o *= mag2phi (bc.c[j], Ep[i]); addPt (P, Ep[i], Et[i] / o, deepW (Et[i])); }
        for (int i = 0; i < kOut; ++i) if (To[i] > 1e-6) { double o = 1; for (int j = 0; j < n; ++j) if (j != k) o *= mag2phi (bc.c[j], g.op[i]); addPt (P, g.op[i], To[i] / o, 0.02); }
        Coef ci; double ei; if (! fitIRLS (P, bc.c[k], 8, ci, ei) || ! stable (ci)) continue;
        BandCoefs t = bc; t.c[k] = ci; const double e1 = err (t); if (e1 < e0) { bc = t; e0 = e1; }
    } }
}
// errore (dB, metrica ErrAcc) di una cascata digitale rispetto all'analogico: 64 punti udibili + 160 della griglia fitta
inline double cascadeErr (const SecSet& set, const BandCoefs& bc, const Grid& g) {
    ErrAcc e; for (int i = 0; i < kEval; ++i) { double d = 1; for (int k = 0; k < bc.n; ++k) d *= mag2phi (bc.c[k], g.ep[i]); e.add (d, anaMag2 (set, g.ef[i])); }
    for (int i = 0; i < kDense; ++i) { double d = 1; for (int k = 0; k < bc.n; ++k) d *= mag2phi (bc.c[k], g.dp[i]); e.add (d, anaMag2 (set, g.df[i])); }
    for (int j = 0; j < set.n; ++j) { double nf[kNarrow]; const int nn = narrowPts (set.s[j], nf);   // punti stretti attorno alle risonanze (Q alto): la correzione non deve peggiorarle
        for (int q = 0; q < nn; ++q) if (nf[q] > 20 && nf[q] < g.top) { const Phi ph = phiAt (nf[q], g.fs); double d = 1; for (int k = 0; k < bc.n; ++k) d *= mag2phi (bc.c[k], ph); e.add (d, anaMag2 (set, nf[q])); } }
    return e.db(); }
// 0.4.8: sezione di correzione vicino a Nyquist. Se il progetto preciso sbaglia ancora di oltre 0,25 dB (shelf e tagli sopra 15 kHz a 44,1/48 kHz, shelf con Q alto),
// si aggiunge una sezione che parte neutra (polo e zero coincidenti) e la cascata viene rifittata tutta: tenuta solo se l'errore scende di almeno 0,05 dB.
constexpr double kCorrThr = 0.25;
inline double fineErr (const SecSet& set, const BandCoefs& bc, const Grid& g) {   // 512 punti log 20 Hz - top: controllo finale della correzione
    ErrAcc e; for (int i = 0; i < 512; ++i) { const double fr = 20.0 * std::pow (g.top / 20.0, i / 511.0); const Phi ph = phiAt (fr, g.fs); double d = 1; for (int k = 0; k < bc.n; ++k) d *= mag2phi (bc.c[k], ph); e.add (d, anaMag2 (set, fr)); }
    return e.db(); }
inline void correctNyq (const SecSet& set, const Grid& g, BandCoefs& out) {
    if (set.n >= kMaxSecs) return; double fmx = 0; for (int k = 0; k < set.n; ++k) fmx = std::max (fmx, set.s[k].f0); if (fmx < g.fs * 0.1) return;   // sotto fs/10 gli errori restano sotto 0,25 dB: niente controllo (costo)
    const double e0 = cascadeErr (set, out, g); if (! (e0 > kCorrThr)) return;
    SecSet s2 = set; Sec& id = s2.s[s2.n++]; id.f0 = g.fs / 4; id.n[0] = 1; id.n[1] = 1; id.n[2] = 0; id.d[0] = 1; id.d[1] = 1; id.d[2] = 0;
    // 0.6.0: tre partenze (scelta normale, poi tutte le sezioni dal matched-z, poi dal 3 punti): il rifit parte da un minimo locale diverso.
    // (una sola partenza: High Shelf 14,8 kHz +18 dB Q analogico 0,51 a 48 kHz restava a 0,24 dB, con Q 0,50 scendeva a 0,004: misurato)
    BandCoefs best; double be = 1e300;
    for (int st = -1; st < 2; ++st) { BandCoefs t; designCascade (s2, g, t, true, st); if (t.n != s2.n) continue; bool okk = true; for (int k = 0; k < t.n; ++k) okk = okk && stable (t.c[k]); if (! okk) continue;
        const double e = cascadeErr (set, t, g); if (e < be) { be = e; best = t; } }
    if (be < e0 - 0.05 && fineErr (set, best, g) < fineErr (set, out, g) - 0.01) out = best;   // tenuta solo se migliora anche su 512 punti
}
// precise = false: progetto veloce (qualita e costo della 0.4.3), usato mentre i parametri cambiano; il motore passa al preciso dopo 50 ms fermi
// corr = false: niente sezione di correzione (bande dinamiche: la tabella dei guadagni usa una sola sezione)
inline void designBand (const BandParams& b, double gainDb, const Grid& g, BandCoefs& out, bool precise = true, bool corr = true) {
    SecSet set; sectionsFor (b, gainDb, set); out.n = set.n;
    if (b.type == AllPass) {                       // all pass: poli mappati esattamente, numeratore = denominatore invertito
        Coef c; poles (set.s[0].d, 2 * kPi * std::min (b.f, g.fs * 0.49) / g.fs, c.a1, c.a2);
        c.b0 = c.a2; c.b1 = c.a1; c.b2 = 1; out.c[0] = c; out.n = 1; return;
    }
    if (set.n > 1 && precise) designCascade (set, g, out, b.type == HighCut || b.type == BandPass);   // veloce: sezione per sezione (come 0.4.3)
    else { for (int i = 0; i < set.n; ++i) out.c[i] = designSec (set.s[i], g, precise);
           out.c[0].b0 *= set.k; out.c[0].b1 *= set.k; out.c[0].b2 *= set.k; }
    if (precise && corr) correctNyq (set, g, out);
}
inline double bandMag2 (const BandCoefs& bc, double fr, double fs) { const Phi q = phiAt (fr, fs); double m = 1; for (int i = 0; i < bc.n; ++i) m *= mag2phi (bc.c[i], q); return m; }

// ---------- dinamica ----------
inline double kneeFn (double over, double W) { if (W <= 0) return std::max (0.0, over); if (over <= -W / 2) return 0; if (over >= W / 2) return over; const double t = over + W / 2; return t * t / (2 * W); }
constexpr double kDynSlope = 0.75;    // 4:1 dentro il RANGE
constexpr int kFrame = 32;            // frame dinamico fisso (il risultato non dipende dal blocco della DAW)
constexpr int kPkHold = 256;          // Peak: tenuta del picco su almeno un periodo della banda (fino a 256 frame)
constexpr int kSettlePerChunk = 2;    // progetti precisi (assestamento) al massimo ogni kSettleTick campioni
constexpr int kAlign = 128;           // 0.4.8: elaborazione in sottoblocchi allineati alla posizione assoluta: il risultato non dipende dal blocco della DAW
constexpr int kRamp = 512;            // 0.4.8: rampa dei coefficienti di lunghezza fissa (10,7 ms a 48 kHz) per le bande statiche (prima: lunga quanto il blocco della DAW)
constexpr int kSettleTick = 512;      // 0.4.8: assestamenti decisi ogni 512 campioni assoluti (prima: a ogni blocco della DAW)
constexpr int kDynCells = 97;         // tabella dei guadagni dinamici: 96 passi tra 0 e il RANGE (minimo 0.125 dB), interpolazione lineare dei coefficienti
constexpr double kSineRmsDb = 3.0102999566398120;   // Peak: picco - 3.01 dB, una sinusoide legge il suo valore RMS
inline double dynDelta (double levelDb, const BandParams& b) {
    double amt = kneeFn (levelDb - b.thr, b.knee) * kDynSlope; const double r = std::abs (b.range);
    if (amt > r) amt = r; return b.range < 0 ? -amt : amt;
}

// ---------- oversampling 8x per il Character: 3 stadi halfband IIR polifase (2 catene di passa-tutto del 1° ordine), fase minima, nessun ritardo puro ----------
// stadio 1 (x1 <-> x2): 12 coefficienti, transizione 0.02, banda passante piatta fino a 0.23 fs2, attenuazione 123 dB
// stadio 2 (x2 <-> x4):  7 coefficienti, transizione 0.13 (basta: il segnale utile sta sotto 20 kHz), 135.8 dB
// stadio 3 (x4 <-> x8):  5 coefficienti, transizione 0.19, 120.0 dB      (0.4.3: 12 coefficienti per stadio = 84 passa-tutto per direzione, ora 46)
constexpr double kHB1a[6] = { 0.027155856726483182, 0.21303004592041422, 0.46602752012040527, 0.68259076485235193, 0.83396924102510472, 0.93680311116581549 };
constexpr double kHB1b[6] = { 0.10300238556004077, 0.33933626891036295, 0.58224701385700428, 0.76595528238687738, 0.88969265368750716, 0.97923872973422221 };
constexpr double kHB2a[4] = { 0.024598597216109117, 0.2039088165268397, 0.49912912667634296, 0.8817915443234325 };
constexpr double kHB2b[3] = { 0.0952554617272269, 0.340947798142709, 0.6769722455782793 };
constexpr double kHB3a[3] = { 0.03452867076180611, 0.2956273060299206, 0.8097876417138931 };
constexpr double kHB3b[2] = { 0.13509016177223662, 0.5151067820762019 };
template <int NA> struct AllpassChain {   // NA passa-tutto (a + z^-1)/(1 + a z^-1) alla frequenza della catena (lunghezza fissa: il compilatore srotola il ciclo)
    const double* c = nullptr; double x1[NA] = {}, y1[NA] = {};
    double run (double x) { for (int i = 0; i < NA; ++i) { const double a = c[i], y = a * (x - y1[i]) + x1[i]; x1[i] = x; y1[i] = y; x = y; } return x; }
    void reset() { for (int i = 0; i < NA; ++i) x1[i] = y1[i] = 0; }
};
template <int NA, int NB> struct HalfbandUp   { AllpassChain<NA> a; AllpassChain<NB> b; void run (double x, double& y0, double& y1) { y0 = a.run (x); y1 = b.run (x); } void reset() { a.reset(); b.reset(); } };
template <int NA, int NB> struct HalfbandDown { AllpassChain<NA> a; AllpassChain<NB> b; double prev = 0; double run (double e, double o) { const double y = 0.5 * (a.run (e) + b.run (prev)); prev = o; return y; } void reset() { a.reset(); b.reset(); prev = 0; } };
// curve del Character: identiche alla 0.4.3 fino a |x| = 1 (0 dBFS); oltre, prosecuzione monotona (stesso valore, pendenza e curvatura nel punto di giunzione).
// 0.4.3 ripiegava: Warm per picchi negativi oltre -1.06 (circa +0.5 dBFS) l'uscita tornava verso zero, Subtle oltre -2.2 / +2.5: distorsione dura.
constexpr double kSubTp = 2.9333333333333336, kSubTn = 1.8095238095238095, kWarmA = 0.02125, kWarmB = -0.09000000000000001;
inline double characterShape (int chm, double x) {
    if (chm == 1) {
        if (x > 1) return 0.97 + 0.88 * kSubTp * (1 - std::exp (-(x - 1) / kSubTp));
        if (x < -1) return -0.91 - 0.76 * kSubTn * (1 - std::exp ((x + 1) / kSubTn));
        return x + 0.03 * x * x - 0.06 * x * x * x;
    }
    const double ax = std::abs (x); double e;
    if (ax <= 1) e = 0.04 * x * x;
    else { const double u = ax - 1, ex = std::exp (-8 * u); e = 0.04 + kWarmA * (1 - ex) + kWarmB * u * ex; }
    return std::tanh (1.8 * x) / 1.8 + e;
}
struct Oversampled8x {   // x -> 8 campioni -> saturazione -> 1 campione (ogni stadio ha un solo stato e lavora sul suo flusso in sequenza)
    HalfbandUp<6, 6> u1; HalfbandUp<4, 3> u2; HalfbandUp<3, 2> u3; HalfbandDown<3, 2> d3; HalfbandDown<4, 3> d2; HalfbandDown<6, 6> d1;
    Oversampled8x() {
        u1.a.c = kHB1a; u1.b.c = kHB1b; d1.a.c = kHB1a; d1.b.c = kHB1b;
        u2.a.c = kHB2a; u2.b.c = kHB2b; d2.a.c = kHB2a; d2.b.c = kHB2b;
        u3.a.c = kHB3a; u3.b.c = kHB3b; d3.a.c = kHB3a; d3.b.c = kHB3b;
    }
    double run (int chm, double x) {
        double s2[2], s4[4], s8[8], r4[4], r2[2];
        u1.run (x, s2[0], s2[1]);
        for (int i = 0; i < 2; ++i) u2.run (s2[i], s4[2 * i], s4[2 * i + 1]);
        for (int i = 0; i < 4; ++i) u3.run (s4[i], s8[2 * i], s8[2 * i + 1]);
        for (int i = 0; i < 8; ++i) s8[i] = characterShape (chm, s8[i]);
        for (int i = 0; i < 4; ++i) r4[i] = d3.run (s8[2 * i], s8[2 * i + 1]);
        for (int i = 0; i < 2; ++i) r2[i] = d2.run (r4[2 * i], r4[2 * i + 1]);
        return d1.run (r2[0], r2[1]);
    }
    void reset() { u1.reset(); u2.reset(); u3.reset(); d3.reset(); d2.reset(); d1.reset(); }
};

// ---------- motore ----------
// dissolvenza (crossfade) di 15 ms per tutto cio che non si puo interpolare: banda attivata/disattivata, cambio di tipo/pendenza/collocazione,
// solo acceso/spento, cambio di Character. Curva a coseno rialzato (inizio e fine morbidi). Prima della prima elaborazione: nessuna dissolvenza.
inline double fadeW (double u) { return u >= 1 ? 1.0 : u <= 0 ? 0.0 : 0.5 - 0.5 * std::cos (kPi * u); }   // 0 e 1 senza coseno (prima: due coseni per campione anche a riposo, ~15 % della CPU: callgrind)
// ---------- 0.6.0: Linear Phase (bande statiche come un unico FIR a fase lineare) ----------
// Bersaglio = modulo ANALOGICO di ogni banda (nessun cramping), bande nell'ordine dell'indice; Stereo/Left/Right/Mid/Side come matrice 2x2 reale per frequenza.
// FIR di L campioni (finestra di Kaiser beta 9), simmetrico attorno a L/2; convoluzione partizionata uniforme (overlap-save, partizione B).
// Latenza = L/2 + B, costante finche non cambia la risoluzione. Ogni nuovo progetto si dissolve sul vecchio in 2 partizioni (coseno rialzato).
using Cpx = std::complex<double>;
struct Fft {
    int n = 0; std::vector<Cpx> w; std::vector<int> rv;
    void init (int N) {
        n = N; w.resize ((size_t) N / 2); for (int k = 0; k < N / 2; ++k) w[(size_t) k] = std::polar (1.0, -2 * kPi * k / N);
        rv.assign ((size_t) N, 0); int lg = 0; while ((1 << lg) < N) ++lg;
        for (int i = 0; i < N; ++i) { int r = 0; for (int b = 0; b < lg; ++b) if (i & (1 << b)) r |= 1 << (lg - 1 - b); rv[(size_t) i] = r; }
    }
    void run (Cpx* a, bool inv) const {   // senza normalizzazione (l'inversa va divisa per n)
        for (int i = 0; i < n; ++i) if (i < rv[(size_t) i]) std::swap (a[i], a[rv[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1) { const int h = len / 2, st = n / len;
            for (int i = 0; i < n; i += len) for (int j = 0; j < h; ++j) { const Cpx wj = inv ? std::conj (w[(size_t) (j * st)]) : w[(size_t) (j * st)];
                const Cpx u = a[i + j], v = a[i + j + h] * wj; a[i + j] = u + v; a[i + j + h] = u - v; } }
    }
};
inline double besselI0 (double x) { double s = 1, t = 1; for (int k = 1; k < 60; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-17 * s) break; } return s; }

class LinPhase {
public:
    static constexpr int kRes = 5, kSlots = 3;
    static int firLen (int res, double fs) {   // Low 4096 ... Maximum 65536 a 44,1/48 kHz; x2 a 88,2/96, x4 a 176,4/192, x8 a 352,8/384 (stessa risoluzione in Hz)
        int L = 4096 << std::clamp (res, 0, kRes - 1); if (fs > 60000) L *= 2; if (fs > 120000) L *= 2; if (fs > 240000) L *= 2; return std::min (L, 262144); }   // al massimo 262144: a 352,8/384 kHz Maximum = risoluzione di Very High (memoria e picco di calcolo: revisione esterna)
    static int partLen (int L) { return std::clamp (L / 64, 256, 2048); }   // al massimo 2048: picco di calcolo per blocco limitato (revisione esterna: 4096 a 192 kHz Maximum = 1,2 x la scadenza di un blocco da 64)
    static constexpr double kBeta = 3.0;   // finestra di Kaiser: beta 3 (0.6.0 prima: 9). Bell 40 Hz Q4 a L 8192: 1,50 dB contro 2,92; nessuna perdita sotto -80 dB nei casi misurati
    LinPhase() = default; LinPhase (const LinPhase&) = delete; LinPhase& operator= (const LinPhase&) = delete;
    ~LinPhase() { stopWorker(); }
    bool ready() const { return L > 0; }
    int firLength() const { return L; }
    int latency() const { return L > 0 ? L / 2 + B : 0; }
    unsigned maskNow() const { return mask[fcur]; }   // bande contenute nel FIR del blocco in uscita (thread audio)
    // alloca e avvia il thread di progetto: fuori dal thread audio
    void configure (int firL, double sampleRate) {
        stopWorker();
        L = firL; B = partLen (L); K = L / B; NF = 2 * B; fs = sampleRate; flen = std::max ({ 2 * B, 512, L / 16 }) / B * B;   // dissolvenza lunga quanto l'intervallo tra i progetti (L/16 oltre 8192): l'automazione resta a catena anche a Maximum (0.6.1: -69,5 dB)
        fL.init (L); fB.init (NF);
        win.resize ((size_t) L); const double i0 = besselI0 (kBeta); for (int i = 0; i < L; ++i) { const double t = 2.0 * i / L - 1; win[(size_t) i] = besselI0 (kBeta * std::sqrt (std::max (0.0, 1 - t * t))) / i0; }
        win[0] = 0;   // tap 0 senza simmetrico (n = L non esiste): a zero, cosi il FIR e esattamente simmetrico attorno a L/2 (con beta 3 valeva 0,2)
        tmp.assign ((size_t) L, Cpx()); mat.assign ((size_t) (L / 2 + 1) * 4, 0.0); hb.assign ((size_t) NF, Cpx());
        for (int b = 0; b < kMaxBands; ++b) { cache[b].assign ((size_t) (L / 2 + 1), 0.0f); cacheOk[b] = false; }
        for (int s = 0; s < kSlots; ++s) { Hp[s].assign ((size_t) K * NF, Cpx()); Hq[s].assign ((size_t) K * NF, Cpx()); mode[s] = 1; mask[s] = 0; }
        fdl.assign ((size_t) K * NF, Cpx()); fdr.assign ((size_t) K * NF, Cpx()); accA.assign ((size_t) NF, Cpx()); accB.assign ((size_t) NF, Cpx()); xcur.assign ((size_t) NF, Cpx());
        inL.assign ((size_t) NF, 0.0); inR.assign ((size_t) NF, 0.0); outL.assign ((size_t) B, 0.0); outR.assign ((size_t) B, 0.0); outW.assign ((size_t) B, 1.0); outFL.assign ((size_t) B, 0.0); outFR.assign ((size_t) B, 0.0);
        head = 0; pos = 0; done = 0; fcur = 0; ffrom = -1; ffpos = 0; outMc = outMf = 0; cur.store (0); fadeFrom.store (-1); readySlot.store (-1); readyInstant.store (false); readyGen.store (0);
        reqGen = 0; appliedGen = 0; tbMid.store (1); tbBack = 0; tbFront = 2;
        Req id; id.nb = 0; compute (0, id);   // identita (nessuna banda): impulso unitario al centro = solo ritardo
        quit.store (false); worker = std::thread ([this] { run(); });
    }
    void setSync (bool s) { if (s == sync.load()) return; sync.store (s); if (s) while (busy.load()) std::this_thread::yield(); }
    // richiesta (thread audio): bande del FIR nell'ordine dell'indice; idx = indice della banda (maschera e cache)
    void request (const int* idx, const BandParams* p, const double* g, int nb, bool instant) {
        Req& r = tb[tbBack]; r.nb = nb; r.instant = instant; r.gen = ++reqGen; for (int i = 0; i < nb; ++i) { r.idx[i] = idx[i]; r.p[i] = p[i]; r.g[i] = g[i]; }
        if (sync.load()) { const int sl = freeSlot(); compute (sl, r); readyInstant.store (instant); readyGen.store (r.gen); readySlot.store (sl, std::memory_order_release); return; }
        tbBack = tbMid.exchange (tbBack | 4, std::memory_order_acq_rel) & 3; cv.notify_one();
    }
    // in place, x0/x1 = L/R; ritardo L/2 + B. Per ogni campione in uscita: w = peso del FIR nuovo, mc/mf = bande nel FIR nuovo / vecchio
    void process (double* x0, double* x1, int n, double* w, unsigned* mc, unsigned* mf, double* f0, double* f1) {   // f0/f1: parte del FIR vecchio ((1 - w) * vecchio) contenuta in x0/x1
        for (int i = 0; i < n; ++i) {
            if (pos == 0 && done == 0 && ffrom < 0) { const int rs = readySlot.load (std::memory_order_acquire);   // progetto immediato (il primo) pronto prima del blocco: vale gia per questo blocco
                if (rs >= 0 && readyInstant.load() && readyGen.load() > appliedGen) { fcur = rs; appliedGen = readyGen.load(); cur.store (fcur); readySlot.store (-1, std::memory_order_release); cv.notify_one(); } }
            inL[(size_t) (B + pos)] = x0[i]; inR[(size_t) (B + pos)] = x1[i];
            x0[i] = outL[(size_t) pos]; x1[i] = outR[(size_t) pos]; w[i] = outW[(size_t) pos]; mc[i] = outMc; mf[i] = outMf; f0[i] = outFL[(size_t) pos]; f1[i] = outFR[(size_t) pos];
            ++pos; const int target = (int) ((long long) (K - 1) * pos / B);   // partizioni 1..K-1 del prossimo blocco distribuite sui campioni (nessun picco di calcolo)
            while (done < target) partial (++done);
            if (pos == B) { finish(); pos = 0; done = 0; }
        }
    }
private:
    struct Req { int nb = 0; bool instant = false; unsigned gen = 0; int idx[kMaxBands] = {}; BandParams p[kMaxBands]; double g[kMaxBands] = {}; };
    int freeSlot() const { const int c = cur.load (std::memory_order_relaxed), f = fadeFrom.load (std::memory_order_relaxed), r = readySlot.load (std::memory_order_relaxed);
        for (int s = 0; s < kSlots; ++s) if (s != c && s != f && s != r) return s; for (int s = 0; s < kSlots; ++s) if (s != c && s != f) return s; return 0; }
    static bool sameMag (const BandParams& a, double ga, const BandParams& b, double gb) { return a.type == b.type && a.f == b.f && a.q == b.q && a.slope == b.slope && a.gq == b.gq && ga == gb; }
    // Y = P Z + Q Zr, Z = X_L + j X_R, Zr[i] = conj (Z[-i]): matrice 2x2 qualsiasi con due prodotti per frequenza
    void compute (int sl, const Req& r) {
        const int nb2 = L / 2 + 1; int pl[kMaxBands]; unsigned m = 0;
        for (int b = 0; b < r.nb; ++b) { const int ix = r.idx[b]; pl[b] = r.p[b].place; m |= 1u << ix;
            if (! cacheOk[ix] || ! sameMag (cacheP[ix], cacheG[ix], r.p[b], r.g[b])) {   // modulo della banda: ricalcolato solo se la banda e cambiata
                SecSet s; sectionsFor (r.p[b], r.g[b], s); float* c = cache[ix].data(); for (int k = 0; k < nb2; ++k) c[k] = (float) std::sqrt (anaMag2 (s, (double) k * fs / L));
                cacheP[ix] = r.p[b]; cacheG[ix] = r.g[b]; cacheOk[ix] = true; } }
        bool diag = true, equal = true;
        for (int k = 0; k < nb2; ++k) {
            double a = 1, bq = 0, c = 0, d = 1;   // matrice [a b; c d] su (L, R)
            for (int b = 0; b < r.nb; ++b) { const double mg = cache[r.idx[b]][(size_t) k]; double e0, e1, e2, e3;
                switch (pl[b]) { case Left: e0 = mg; e1 = 0; e2 = 0; e3 = 1; break; case Right: e0 = 1; e1 = 0; e2 = 0; e3 = mg; break;
                    case Mid: e0 = e3 = 0.5 * (mg + 1); e1 = e2 = 0.5 * (mg - 1); break; case Side: e0 = e3 = 0.5 * (1 + mg); e1 = e2 = 0.5 * (1 - mg); break;
                    default: e0 = e3 = mg; e1 = e2 = 0; }
                const double na = e0 * a + e1 * c, nbq = e0 * bq + e1 * d, nc = e2 * a + e3 * c, nd = e2 * bq + e3 * d; a = na; bq = nbq; c = nc; d = nd; }
            double* M = &mat[(size_t) k * 4]; M[0] = a; M[1] = bq; M[2] = c; M[3] = d;
            if (bq != 0 || c != 0) diag = false; if (a != d) equal = false; }
        mode[sl] = diag && equal ? 1 : 2; mask[sl] = m;
        std::fill (Hp[sl].begin(), Hp[sl].end(), Cpx()); if (mode[sl] == 2) std::fill (Hq[sl].begin(), Hq[sl].end(), Cpx());
        for (int e = 0; e < (mode[sl] == 1 ? 1 : 4); ++e) {
            for (int k = 0; k < nb2; ++k) { const double v = mat[(size_t) k * 4 + e]; tmp[(size_t) k] = v; if (k > 0 && k < L / 2) tmp[(size_t) (L - k)] = v; }
            fL.run (tmp.data(), true);   // risposta a fase zero (reale, pari), centrata in 0
            const Cpx wp = e == 0 ? Cpx (0.5, 0) : e == 1 ? Cpx (0, -0.5) : e == 2 ? Cpx (0, 0.5) : Cpx (0.5, 0);
            const Cpx wq = e == 0 ? Cpx (0.5, 0) : e == 1 ? Cpx (0, 0.5) : e == 2 ? Cpx (0, 0.5) : Cpx (-0.5, 0);
            for (int p2 = 0; p2 < K; ++p2) { Cpx* h = hb.data();
                for (int i = 0; i < NF; ++i) h[i] = Cpx(); for (int i = 0; i < B; ++i) { const int nI = p2 * B + i; h[i] = tmp[(size_t) ((nI - L / 2 + L) % L)].real() / L * win[(size_t) nI]; }
                fB.run (h, false); Cpx* P = &Hp[sl][(size_t) p2 * NF]; Cpx* Q = &Hq[sl][(size_t) p2 * NF];
                if (mode[sl] == 1) for (int i = 0; i < NF; ++i) P[i] = h[i];
                else for (int i = 0; i < NF; ++i) { P[i] += wp * h[i]; Q[i] += wq * h[i]; } } }
    }
    void mac (int s, int p2, const Cpx* Z, const Cpx* Zr, Cpx* y) const {
        const Cpx* P = &Hp[s][(size_t) p2 * NF];
        if (mode[s] == 1) for (int i = 0; i < NF; ++i) y[i] += P[i] * Z[i];
        else { const Cpx* Q = &Hq[s][(size_t) p2 * NF]; for (int i = 0; i < NF; ++i) y[i] += P[i] * Z[i] + Q[i] * Zr[i]; } }
    void partial (int p2) {   // partizione p2 >= 1 del blocco in arrivo: ingresso passato, gia disponibile
        const size_t o = (size_t) (((head + 1 - p2 + 2 * K) % K) * NF);
        mac (fcur, p2, &fdl[o], &fdr[o], accA.data()); if (ffrom >= 0) mac (ffrom, p2, &fdl[o], &fdr[o], accB.data()); }
    void finish() {
        while (done < K - 1) partial (++done);
        head = (head + 1) % K; Cpx* X = &fdl[(size_t) head * NF]; Cpx* Xr = &fdr[(size_t) head * NF];
        for (int i = 0; i < NF; ++i) X[i] = Cpx (inL[(size_t) i], inR[(size_t) i]); fB.run (X, false);
        for (int i = 0; i < NF; ++i) Xr[i] = std::conj (X[(NF - i) % NF]);
        for (int i = 0; i < B; ++i) { inL[(size_t) i] = inL[(size_t) (B + i)]; inR[(size_t) i] = inR[(size_t) (B + i)]; }
        mac (fcur, 0, X, Xr, accA.data()); fB.run (accA.data(), true); const double sc = 1.0 / NF;
        if (ffrom >= 0) { mac (ffrom, 0, X, Xr, accB.data()); fB.run (accB.data(), true);
            for (int i = 0; i < B; ++i) { const double u = std::min (1.0, (double) (ffpos + i + 1) / flen), w = chained ? u : 0.5 - 0.5 * std::cos (kPi * u); const Cpx o = accB[(size_t) (B + i)] * sc, nw = accA[(size_t) (B + i)] * sc;   // forma: lineare se la dissolvenza segue da vicino la precedente (automazione continua: percorso dei coefficienti continuo, bande laterali -105 dB invece di -62),
                // coseno rialzato se isolata (evento singolo: pendenza nulla agli estremi, energia sopra 4 kHz -115...-124 dBFS invece di -78...-87; misurato)
                outL[(size_t) i] = o.real() + w * (nw.real() - o.real()); outR[(size_t) i] = o.imag() + w * (nw.imag() - o.imag()); outW[(size_t) i] = w; outFL[(size_t) i] = (1 - w) * o.real(); outFR[(size_t) i] = (1 - w) * o.imag(); }
            outMc = mask[fcur]; outMf = mask[ffrom]; }
        else { for (int i = 0; i < B; ++i) { const Cpx v = accA[(size_t) (B + i)] * sc; outL[(size_t) i] = v.real(); outR[(size_t) i] = v.imag(); outW[(size_t) i] = 1.0; outFL[(size_t) i] = outFR[(size_t) i] = 0.0; } outMc = outMf = mask[fcur]; }
        std::fill (accA.begin(), accA.end(), Cpx()); std::fill (accB.begin(), accB.end(), Cpx());
        // piano del prossimo blocco: dissolvenza in corso o progetto nuovo pronto (i risultati piu vecchi dell'ultimo applicato si scartano)
        if (ffrom >= 0) { ffpos += B; if (ffpos >= flen) { ffrom = -1; sinceEnd = 0; } } else if (sinceEnd < 1000000) sinceEnd += B;
        if (ffrom < 0) { const int rs = readySlot.load (std::memory_order_acquire);
            if (rs >= 0) { const unsigned gn = readyGen.load();
                if (gn > appliedGen) { if (readyInstant.load()) fcur = rs; else { ffrom = fcur; fcur = rs; ffpos = 0; chained = sinceEnd <= std::max (2048, 4 * B) + flen; } appliedGen = gn; }
                cur.store (fcur); fadeFrom.store (ffrom);   // prima gli slot in uso, poi lo slot pronto si libera: il thread di progetto non deve mai vedere lo slot appena preso come libero
                readySlot.store (-1, std::memory_order_release); cv.notify_one(); } }   // (0.6.1 li pubblicava dopo: corsa trovata da ThreadSanitizer, salti fino a 3,5 dB nell'automazione)
        cur.store (fcur); fadeFrom.store (ffrom);
    }
    void run() {   // thread di progetto: prende l'ultima richiesta, progetta nello slot libero, la consegna al thread audio
        std::unique_lock<std::mutex> lk (mx);
        while (! quit.load()) {
            cv.wait_for (lk, std::chrono::milliseconds (20));
            if (quit.load()) break;
            if (! (tbMid.load (std::memory_order_acquire) & 4) || readySlot.load (std::memory_order_acquire) >= 0) continue;
            busy.store (true); if (sync.load()) { busy.store (false); continue; }
            tbFront = tbMid.exchange (tbFront, std::memory_order_acq_rel) & 3; const Req& r = tb[tbFront];
            const int sl = freeSlot(); compute (sl, r); readyInstant.store (r.instant); readyGen.store (r.gen); readySlot.store (sl, std::memory_order_release); busy.store (false);
        }
    }
    void stopWorker() { if (worker.joinable()) { quit.store (true); cv.notify_one(); worker.join(); } }
    std::atomic<bool> sync { true }, busy { false }, readyInstant { false }; std::atomic<unsigned> readyGen { 0 };
    bool chained = false; int sinceEnd = 1000000, L = 0, B = 0, K = 0, NF = 0, head = 0, pos = 0, done = 0, fcur = 0, ffrom = -1, ffpos = 0, flen = 512, mode[kSlots] = { 1, 1, 1 }; unsigned mask[kSlots] = {}, outMc = 0, outMf = 0, reqGen = 0, appliedGen = 0; double fs = 48000;
    std::atomic<int> cur { 0 }, fadeFrom { -1 }, readySlot { -1 }, tbMid { 1 }; int tbBack = 0, tbFront = 2; std::atomic<bool> quit { false };
    std::thread worker; std::mutex mx; std::condition_variable cv; Req tb[3];
    std::vector<float> cache[kMaxBands]; BandParams cacheP[kMaxBands]; double cacheG[kMaxBands] = {}; bool cacheOk[kMaxBands] = {};
    Fft fL, fB; std::vector<double> win, mat, inL, inR, outL, outR, outW, outFL, outFR; std::vector<Cpx> tmp, fdl, fdr, accA, accB, xcur, hb; std::vector<Cpx> Hp[kSlots], Hq[kSlots];
};

// ritardo stereo di n campioni (anello), in place
struct Delay2 { std::vector<double> a, b; int n = 0, w = 0;
    void setup (int len) { n = len; a.assign ((size_t) std::max (1, len), 0.0); b.assign ((size_t) std::max (1, len), 0.0); w = 0; }
    void process (double* x, double* y, int m) { if (n <= 0) return; for (int i = 0; i < m; ++i) { const double ta = a[(size_t) w], tb = b[(size_t) w]; a[(size_t) w] = x[i]; b[(size_t) w] = y[i]; x[i] = ta; y[i] = tb; if (++w == n) w = 0; } } };

class Engine {
public:
    struct Global { double inDb = 0, outDb = 0, autoGainDb = 0, scale = 1; int character = 0; bool bypass = false; int solo = -1;
                    double pan = 0; bool panMS = false; bool invert = false; bool gq = false; };   // pan -1..+1 (L/R o M/S), polarità invertita, Gain-Q
    std::atomic<float> meterDelta[kMaxBands], meterLevel[kMaxBands];

    Engine() { for (int i = 0; i < kMaxBands; ++i) { meterDelta[i] = 0; meterLevel[i] = -120; } }
    void prepare (double sampleRate, int maxBlock) {
        fs = sampleRate; grid.init (fs); N = std::max (maxBlock, 32); absPos = 0;
        for (auto* v : { &L, &R, &M, &S, &dL, &dR, &sL, &sR, &sM, &sS, &xL, &xR, &oL, &oR, &pL, &pR }) v->assign ((size_t) N, 0.0);
        for (auto& b : st) { b = BandState(); } for (auto& b : old) { b = BandState(); }
        fadeLen = std::max (1, (int) std::lround (0.015 * fs)); settleLen = std::max (1, (int) std::lround (0.05 * fs)); primed = false; aParam = 1 - std::exp (-kFrame / (0.01 * fs)); soloW = 0; chPrev = 0; chPos = 2 * fadeLen;
        for (auto& m : osm) for (auto& o : m) o.reset(); std::memset (dcm, 0, sizeof (dcm));
        outG = outG1 = 1; inG = inG1 = 1; byp = 0; soloSt = SoloState(); lastChm = 0; panA = panB = panA1 = panB1 = 1; pol = 0;
        rL.assign ((size_t) N, 0.0); rR.assign ((size_t) N, 0.0); configurePhase (phaseMode, phaseRes);
    }
    // 0.6.0: modo di elaborazione. 0 = Zero Latency (biquad), 1 = Linear Phase (bande statiche in un FIR a fase lineare; le dinamiche restano biquad).
    // Alloca: va chiamato fuori dal thread audio (il processore sospende l'elaborazione). Dopo il cambio l'uscita riparte da 0 con la rampa di 20 ms.
    void configurePhase (int mode, int res) {
        phaseMode = mode == 1 ? 1 : 0; phaseRes = std::clamp (res, 0, LinPhase::kRes - 1);
        lin.reset();   // ferma il thread di progetto e libera la memoria del modo precedente
        if (phaseMode == 1 && fs > 0) { lin = std::make_unique<LinPhase>(); lin->configure (LinPhase::firLen (phaseRes, fs), fs); lin->setSync (linSync); const int la = lin->latency(); dlyDry.setup (la); dlyRaw.setup (la); dlySc.setup (la); }
        else { dlyDry.setup (0); dlyRaw.setup (0); dlySc.setup (0); }
        linDirty = true; linOnce = false; outG = outG1 = 0; for (auto& b : st) { std::memset (b.z, 0, sizeof (b.z)); std::memset (b.sz, 0, sizeof (b.sz)); std::memset (b.sdz, 0, sizeof (b.sdz)); b.flen = 0; b.lpFromDyn = false; b.linTail = false; }
        for (auto& o : old) o.active = false;   // nessuna copia in dissolvenza sopravvive al cambio di modo (revisione esterna: +10 dB al ritorno)
        kW.assign ((size_t) N, 1.0); kMc.assign ((size_t) N, 0u); kMf.assign ((size_t) N, 0u); sxL.assign ((size_t) N, 0.0); sxR.assign ((size_t) N, 0.0); sxM.assign ((size_t) N, 0.0); sxS.assign ((size_t) N, 0.0); kk.assign ((size_t) N, 0.0); kxL.assign ((size_t) N, 0.0); kxR.assign ((size_t) N, 0.0); fxL.assign ((size_t) N, 0.0); fxR.assign ((size_t) N, 0.0); tgL.assign ((size_t) N, 0.0); tgR.assign ((size_t) N, 0.0);
    }
    void setPhaseConfig (int mode, int res) { phaseMode = mode == 1 ? 1 : 0; phaseRes = std::clamp (res, 0, LinPhase::kRes - 1); }   // solo memorizza: prepare() poi alloca
    int latencySamples() const { return phaseMode == 1 && lin ? lin->latency() : 0; }
    // true: i progetti Linear Phase avvengono nel thread audio (risultato deterministico: test, rendering offline); false: thread di progetto (tempo reale)
    void setLinSync (bool s) { linSync = s; if (lin) lin->setSync (s); }
    int phaseModeNow() const { return phaseMode; }
    // chiamato all'inizio di ogni blocco, dal thread audio
    void setBand (int i, const BandParams& p) {
        BandState& s = st[i]; const bool active = p.used && ! p.bypass;
        if (! active) { if (s.active) { fadeOut (i); linDirty = true; } s.active = false; s.dynOn = false; s.p = p; meterDelta[i] = 0; meterLevel[i] = -120; return; }
        const bool changed = ! same (s.p, p) || s.scale != glob.scale;
        if (! changed && s.active) return;
        linDirty = true;
        if (s.active && sameFilter (s.p, p) && s.scale == glob.scale && s.p.dyn == p.dyn && s.p.range == p.range) {   // cambiano solo soglia/attacco/rilascio/knee/detector: il filtro resta
            s.p = p; if (s.dynOn) { s.aA = 1 - std::exp (-kFrame / (std::max (0.05, p.att) * 1e-3 * fs)); s.aR = 1 - std::exp (-kFrame / (std::max (1.0, p.rel) * 1e-3 * fs)); }
            return;
        }
        const bool dynOn = p.dyn && canDyn (p.type); const double g = hasGain (p.type) ? p.gain * glob.scale : 0;
        BandCoefs nc; designBand (p, g, grid, nc, ! primed, ! dynOn); s.precise = ! primed; s.quiet = 0;
        // stessa forma = stesso tipo, stessa collocazione, stesso numero di sezioni: si interpola. Altrimenti dissolvenza vecchia -> nuova.
        bool sameShape = s.active && s.p.type == p.type && s.p.slope == p.slope && s.p.place == p.place && (nc.n == s.tgt.n || (! dynOn && ! s.dynOn && std::abs (nc.n - s.tgt.n) == 1)) && s.dynOn == dynOn;
        if (sameShape && ! dynOn && primed && bigJump (s.p, s.g, p, g)) sameShape = false;
        if (sameShape && nc.n != s.tgt.n) matchSecs (s, nc);   // 0.4.8: con o senza sezione di correzione si interpola lo stesso   // dinamica on/off: dissolvenza (il salto di guadagno non si interpola bene)
        if (s.active && ! sameShape) fadeOut (i);
        const bool fresh = ! sameShape;
        { const bool wasIir = s.active && iirOnly (s); BandState tmpS = s; tmpS.dynOn = dynOn; tmpS.p = p; if (wasIir && ! iirOnly (tmpS) && linActive()) s.lpFromDyn = true; }   // Linear Phase: da biquad a FIR, il biquad resta finche il FIR non la contiene
        s.dynOn = dynOn; s.p = p; s.scale = glob.scale; s.g = g; if (! fresh && ! dynOn) startRamp (s, nc); else s.tgt = nc;
        if (dynOn) { BandParams dp; detShape (p, dp); designBand (dp, 0, grid, s.det, false); }   // filtro del detector solo per le bande dinamiche (prima: progettato a ogni cambio anche quando non serviva)
        if (s.dynOn) {   // costanti a frequenza di frame: attacco/rilascio sul livello in dB, media RMS su 8 periodi della banda (10-80 ms)
            s.aA = 1 - std::exp (-kFrame / (std::max (0.05, p.att) * 1e-3 * fs)); s.aR = 1 - std::exp (-kFrame / (std::max (1.0, p.rel) * 1e-3 * fs));
            s.aRms = 1 - std::exp (-kFrame / (std::clamp (8.0 / p.f, 0.01, 0.08) * fs));   // finestra RMS: 8 periodi della banda, tra 10 e 80 ms
            s.pkLen = std::clamp ((int) std::ceil (fs / (p.f * kFrame)), 1, kPkHold);       // Peak: un periodo della banda (prima: un frame da 32 campioni, a 100 Hz leggeva 0,6-0,9 dB in meno)
            const double R = p.range * glob.scale; s.dynSign = R >= 0 ? 1 : -1; s.dynCell = std::max (0.125, std::abs (R) / (kDynCells - 1)); ++s.dynGen;
            s.dynC[0] = nc.c[0]; s.dynValid[0] = s.dynGen;
        }
        else s.delta = 0;
        if (fresh) { s.delta = 0; s.cur = s.tgt; s.fr0 = s.tgt; s.rfrom = s.tgt; s.rc = s.tgt; s.svf = false; for (int k = 0; k < kMaxSecs; ++k) s.rcS.on[k] = false; s.rpos = kRamp; s.phase = 0; std::memset (s.z, 0, sizeof (s.z)); std::memset (s.dz, 0, sizeof (s.dz));
                     std::memset (s.acc2, 0, sizeof (s.acc2)); std::memset (s.pk, 0, sizeof (s.pk)); std::memset (s.pkRing, 0, sizeof (s.pkRing)); s.pkPos = 0; std::memset (s.ms, 0, sizeof (s.ms)); s.lvl = -120; s.detInit = true; s.thrS = p.thr;
                     s.fpos = 0; s.flen = primed ? fadeLen : 0; }
        else if (s.dynOn) dynTarget (s, s.delta, s.tgt);
        s.active = true;
    }
    void setGlobal (const Global& g) { glob = g; }
    double sampleRate() const { return fs; }
    const Grid& getGrid() const { return grid; }

    // L/R in place; scL/scR = sidechain (o nullptr)
    void process (float* Lio, float* Rio, const float* scL, const float* scR, int n) {
        if (! primed) snapGlobals();   // primo blocco dopo prepare: guadagni, bypass, polarita, pan e Character partono gia al valore impostato (prima: rampa da 0 dB / attivo / Clean)
        primed = true;
        for (int i0 = 0; i0 < n; ) {   // sottoblocchi che non attraversano mai un multiplo di kAlign (contato dall'inizio): rampe e assestamenti identici con qualsiasi blocco
            const int len = std::min ({ N, n - i0, kAlign - (int) (absPos % (unsigned long long) kAlign) });
            processChunk (Lio + i0, Rio + i0, scL ? scL + i0 : nullptr, scR ? scR + i0 : nullptr, len); absPos += (unsigned long long) len; i0 += len; }
    }

private:
    struct BandState {
        BandParams p; bool active = false, dynOn = false; double scale = 1, g = 0, delta = 0, aA = 0, aR = 0; int phase = 0;   // phase: posizione nel frame dinamico da 32 campioni (indipendente dal blocco della DAW)
        BandCoefs cur, tgt, det, fr0; double z[2][kMaxSecs * 2] = {}; double dz[2][2] = {};
        double pkRing[kPkHold] = {}; int pkPos = 0, pkLen = 1;   // Peak: picchi degli ultimi frame; il livello e il massimo sugli ultimi pkLen (>= un periodo della banda)
        double acc2[2] = {}, pk[2] = {}, ms[2] = {}, lvl = -120, aRms = 0;   // detector: somma dei quadrati e picco del frame, media RMS, livello (dB) con attacco/rilascio
        double dynCell = 0.125; int dynSign = -1; unsigned dynGen = 1; unsigned dynValid[kDynCells] = {}; Coef dynC[kDynCells];
        bool precise = true; int quiet = 0;   // progetto preciso dopo 50 ms senza cambi di parametri (prima: veloce)
        BandCoefs rfrom; int rpos = kRamp;     // bande statiche: rampa da rfrom a tgt, posizione rpos su kRamp campioni
        BandCoefs rc, rd;                     // rampa: coefficienti dell'ultimo campione e incremento per campione (sommati in sequenza: identici con qualsiasi divisione in blocchi)
        SvfSet rcS, rdS; bool svf = false;    // 0.5.0: durante i salti grandi la banda gira come SVF (parametri e incrementi qui), poi torna biquad
        bool detInit = false;                 // primo frame dopo l'attivazione: il detector parte dal livello misurato (niente salita da -120 dB)
        double thrS = -24;                    // soglia levigata (10 ms): automazione della soglia senza gradini di guadagno
        double sz[2][kMaxSecs * 2] = {};      // Linear Phase: stato della banda statica nella catena del detector
        bool linTail = false;                 // Linear Phase: la banda ha suonato come biquad; la sua coda va finita prima di fermarla
        bool lpFromDyn = false; double sdz[2][kMaxSecs * 2] = {};   // stato della banda dinamica nella catena del detector (Linear Phase)               // Linear Phase: banda diventata statica da dinamica (o da All Pass): suona come biquad finche il FIR non la contiene
        int fpos = 0, flen = 0; double w0 = 1;   // dissolvenza: in entrata (banda nuova) o in uscita (copia in old[])
    };
    struct SoloState { int id = -1; BandParams src; BandCoefs c, tgt; double z[2][kMaxSecs * 2] = {}; int place = 0; };
    static bool sameFilter (const BandParams& a, const BandParams& b) {   // parametri che determinano i coefficienti del filtro (non la dinamica)
        return a.used == b.used && a.bypass == b.bypass && a.type == b.type && a.f == b.f && a.gain == b.gain && a.q == b.q && a.slope == b.slope && a.place == b.place && a.gq == b.gq;
    }
    static bool same (const BandParams& a, const BandParams& b) {
        return a.used == b.used && a.bypass == b.bypass && a.type == b.type && a.f == b.f && a.gain == b.gain && a.q == b.q && a.slope == b.slope && a.place == b.place
            && a.dyn == b.dyn && a.thr == b.thr && a.range == b.range && a.att == b.att && a.rel == b.rel && a.knee == b.knee && a.det == b.det && a.sc == b.sc && a.gq == b.gq;
    }
    static void detShape (const BandParams& b, BandParams& d) {
        d = BandParams(); d.used = true; d.f = b.f; d.slope = 12;
        if (b.type == LowShelf) { d.type = HighCut; d.q = std::sqrt (0.5); }
        else if (b.type == HighShelf || b.type == TiltShelf) { d.type = LowCut; d.q = std::sqrt (0.5); }
        else { d.type = BandPass; d.q = std::max (0.3, b.q); d.slope = 6; }   // Bell: passa-banda a UNA sezione (2° ordine, 0 dB al centro, Q della banda).
    }                                                                       // 0.4.3 usava slope 12 = due sezioni, ma il detector ne esegue una sola: era un passa-alto a 0.62 f (+0.59 dB al centro)
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
            for (int i = i0; i < i1; ++i) {   // coefficienti dalla posizione assoluta nel frame: stesso risultato comunque il frame sia diviso tra blocchi
                const double t = (double) (pos + (i - i0) + 1), b0 = a.b0 + db0 * t, b1 = a.b1 + db1 * t, b2 = a.b2 + db2 * t, a1 = a.a1 + da1 * t, a2 = a.a2 + da2 * t;
                const double xi = x[i], y = b0 * xi + z1; z1 = b1 * xi - a1 * y + z2; z2 = b2 * xi - a2 * y; x[i] = y;
            }
            z[2 * k] = z1; z[2 * k + 1] = z2;
        }
    }
    // stesse operazioni, due canali nello stesso ciclo (due catene di dipendenza indipendenti: circa il doppio del throughput, risultato identico bit per bit)
    static void run2 (const BandCoefs& c, double* zl, double* zr, double* xl, double* xr, int n) {
#if defined(__GNUC__) || defined(__clang__)
        // 0.6.0: L e R in un vettore di due double (SSE2 / NEON): stesse operazioni, stesso ordine, nessuna FMA (-ffp-contract=off) -> risultato identico al bit, meta delle istruzioni
        typedef double V2 __attribute__ ((vector_size (16)));
        for (int k = 0; k < c.n; ++k) {
            const Coef& q = c.c[k]; const V2 b0 = { q.b0, q.b0 }, b1 = { q.b1, q.b1 }, b2 = { q.b2, q.b2 }, a1 = { q.a1, q.a1 }, a2 = { q.a2, q.a2 };
            V2 z1 = { zl[2 * k], zr[2 * k] }, z2 = { zl[2 * k + 1], zr[2 * k + 1] };
            for (int i = 0; i < n; ++i) {
                const V2 x = { xl[i], xr[i] }, y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; xl[i] = y[0]; xr[i] = y[1];
            }
            zl[2 * k] = z1[0]; zl[2 * k + 1] = z2[0]; zr[2 * k] = z1[1]; zr[2 * k + 1] = z2[1];
        }
#else
        for (int k = 0; k < c.n; ++k) {
            const Coef& q = c.c[k]; const double b0 = q.b0, b1 = q.b1, b2 = q.b2, a1 = q.a1, a2 = q.a2;
            double l1 = zl[2 * k], l2 = zl[2 * k + 1], r1 = zr[2 * k], r2 = zr[2 * k + 1];
            for (int i = 0; i < n; ++i) {
                const double xa = xl[i], xb = xr[i], ya = b0 * xa + l1, yb = b0 * xb + r1;
                l1 = b1 * xa - a1 * ya + l2; r1 = b1 * xb - a1 * yb + r2; l2 = b2 * xa - a2 * ya; r2 = b2 * xb - a2 * yb; xl[i] = ya; xr[i] = yb;
            }
            zl[2 * k] = l1; zl[2 * k + 1] = l2; zr[2 * k] = r1; zr[2 * k + 1] = r2;
        }
#endif
    }
    bool simpleStereo (int b) const { const BandState& s = st[b]; return s.active && ! s.dynOn && s.flen == 0 && ! old[b].active && s.rpos >= kRamp && ! s.svf && s.p.place == Stereo && s.tgt.n > 0; }
    static constexpr int kCasMax = 64; const Coef* csC[kCasMax] = {}; double* csZl[kCasMax] = {}; double* csZr[kCasMax] = {};
    // cascata a fronte d'onda: al passo t la sezione j elabora il campione t - j. Le sezioni di un passo sono indipendenti (parallelismo per la CPU);
    // ogni sezione fa le stesse operazioni nello stesso ordine di run2: risultato identico al bit
    void runCascade2 (int m, double* xl, double* xr, int n) {
        if (m == 0) return;
#if defined(__GNUC__) || defined(__clang__)
        typedef double V2 __attribute__ ((vector_size (16)));
        V2 B0[kCasMax], B1[kCasMax], B2[kCasMax], A1[kCasMax], A2[kCasMax], Z1[kCasMax], Z2[kCasMax], Cr[kCasMax];
        for (int j = 0; j < m; ++j) { const Coef& q = *csC[j]; B0[j] = V2 { q.b0, q.b0 }; B1[j] = V2 { q.b1, q.b1 }; B2[j] = V2 { q.b2, q.b2 }; A1[j] = V2 { q.a1, q.a1 }; A2[j] = V2 { q.a2, q.a2 };
            Z1[j] = V2 { csZl[j][0], csZr[j][0] }; Z2[j] = V2 { csZl[j][1], csZr[j][1] }; Cr[j] = V2 { 0, 0 }; }
        for (int t = 0; t < n + m - 1; ++t) {
            const int jHi = std::min (m - 1, t), jLo = std::max (0, t - n + 1);
            for (int j = jHi; j >= jLo; --j) { const int i = t - j; const V2 x = j == 0 ? V2 { xl[i], xr[i] } : Cr[j];
                const V2 y = B0[j] * x + Z1[j]; Z1[j] = B1[j] * x - A1[j] * y + Z2[j]; Z2[j] = B2[j] * x - A2[j] * y;
                if (j == m - 1) { xl[i] = y[0]; xr[i] = y[1]; } else Cr[j + 1] = y; } }
        for (int j = 0; j < m; ++j) { csZl[j][0] = Z1[j][0]; csZr[j][0] = Z1[j][1]; csZl[j][1] = Z2[j][0]; csZr[j][1] = Z2[j][1]; }
#else
        for (int j = 0; j < m; ++j) { const Coef& q = *csC[j]; double l1 = csZl[j][0], l2 = csZl[j][1], r1 = csZr[j][0], r2 = csZr[j][1];
            for (int i = 0; i < n; ++i) { const double xa = xl[i], xb = xr[i], ya = q.b0 * xa + l1, yb = q.b0 * xb + r1;
                l1 = q.b1 * xa - q.a1 * ya + l2; r1 = q.b1 * xb - q.a1 * yb + r2; l2 = q.b2 * xa - q.a2 * ya; r2 = q.b2 * xb - q.a2 * yb; xl[i] = ya; xr[i] = yb; }
            csZl[j][0] = l1; csZl[j][1] = l2; csZr[j][0] = r1; csZr[j][1] = r2; }
#endif
    }
    static void runLerp2 (const BandCoefs& c0, const BandCoefs& c1, double* zl, double* zr, double* xl, double* xr, int i0, int i1) {
        const double len = i1 - i0;
        for (int k = 0; k < c1.n; ++k) {
            const Coef& a = c0.c[k]; const Coef& b = c1.c[k]; double l1 = zl[2 * k], l2 = zl[2 * k + 1], r1 = zr[2 * k], r2 = zr[2 * k + 1];
            double b0 = a.b0, b1 = a.b1, b2 = a.b2, a1 = a.a1, a2 = a.a2;
            const double db0 = (b.b0 - b0) / len, db1 = (b.b1 - b1) / len, db2 = (b.b2 - b2) / len, da1 = (b.a1 - a1) / len, da2 = (b.a2 - a2) / len;
            for (int i = i0; i < i1; ++i) {
                b0 += db0; b1 += db1; b2 += db2; a1 += da1; a2 += da2;
                const double xa = xl[i], xb = xr[i], ya = b0 * xa + l1, yb = b0 * xb + r1;
                l1 = b1 * xa - a1 * ya + l2; r1 = b1 * xb - a1 * yb + r2; l2 = b2 * xa - a2 * ya; r2 = b2 * xb - a2 * yb; xl[i] = ya; xr[i] = yb;
            }
            zl[2 * k] = l1; zl[2 * k + 1] = l2; zr[2 * k] = r1; zr[2 * k + 1] = r2;
        }
    }
    static void runFrame2 (const BandCoefs& c0, const BandCoefs& c1, double* zl, double* zr, double* xl, double* xr, int i0, int i1, int pos, int F) {
        for (int k = 0; k < c1.n; ++k) {
            const Coef& a = c0.c[k]; const Coef& b = c1.c[k]; double l1 = zl[2 * k], l2 = zl[2 * k + 1], r1 = zr[2 * k], r2 = zr[2 * k + 1];
            const double db0 = (b.b0 - a.b0) / F, db1 = (b.b1 - a.b1) / F, db2 = (b.b2 - a.b2) / F, da1 = (b.a1 - a.a1) / F, da2 = (b.a2 - a.a2) / F;
            for (int i = i0; i < i1; ++i) {
                const double t = (double) (pos + (i - i0) + 1), b0 = a.b0 + db0 * t, b1 = a.b1 + db1 * t, b2 = a.b2 + db2 * t, a1 = a.a1 + da1 * t, a2 = a.a2 + da2 * t;
                const double xa = xl[i], xb = xr[i], ya = b0 * xa + l1, yb = b0 * xb + r1;
                l1 = b1 * xa - a1 * ya + l2; r1 = b1 * xb - a1 * yb + r2; l2 = b2 * xa - a2 * ya; r2 = b2 * xb - a2 * yb; xl[i] = ya; xr[i] = yb;
            }
            zl[2 * k] = l1; zl[2 * k + 1] = l2; zr[2 * k] = r1; zr[2 * k + 1] = r2;
        }
    }
    // rampa incrementale: c += d a ogni campione, c resta nello stato (stesse somme nello stesso ordine comunque il blocco sia diviso)
    static void runInc (BandCoefs& c, const BandCoefs& d, double* z, double* x, int n) {
        for (int k = 0; k < c.n; ++k) {
            const Coef& q = d.c[k]; double b0 = c.c[k].b0, b1 = c.c[k].b1, b2 = c.c[k].b2, a1 = c.c[k].a1, a2 = c.c[k].a2, z1 = z[2 * k], z2 = z[2 * k + 1];
            for (int i = 0; i < n; ++i) { b0 += q.b0; b1 += q.b1; b2 += q.b2; a1 += q.a1; a2 += q.a2;
                const double xi = x[i], y = b0 * xi + z1; z1 = b1 * xi - a1 * y + z2; z2 = b2 * xi - a2 * y; x[i] = y; }
            c.c[k] = Coef { b0, b1, b2, a1, a2 }; z[2 * k] = z1; z[2 * k + 1] = z2;
        }
    }
    static void runInc2 (BandCoefs& c, const BandCoefs& d, double* zl, double* zr, double* xl, double* xr, int n) {
        for (int k = 0; k < c.n; ++k) {
            const Coef& q = d.c[k]; double b0 = c.c[k].b0, b1 = c.c[k].b1, b2 = c.c[k].b2, a1 = c.c[k].a1, a2 = c.c[k].a2;
            double l1 = zl[2 * k], l2 = zl[2 * k + 1], r1 = zr[2 * k], r2 = zr[2 * k + 1];
            for (int i = 0; i < n; ++i) { b0 += q.b0; b1 += q.b1; b2 += q.b2; a1 += q.a1; a2 += q.a2;
                const double xa = xl[i], xb = xr[i], ya = b0 * xa + l1, yb = b0 * xb + r1;
                l1 = b1 * xa - a1 * ya + l2; r1 = b1 * xb - a1 * yb + r2; l2 = b2 * xa - a2 * ya; r2 = b2 * xb - a2 * yb; xl[i] = ya; xr[i] = yb; }
            c.c[k] = Coef { b0, b1, b2, a1, a2 }; zl[2 * k] = l1; zl[2 * k + 1] = l2; zr[2 * k] = r1; zr[2 * k + 1] = r2;
        }
    }
    // parametri che cambiano a ogni campione: q += d (incrementale, nello stato) oppure q = a + d * t (posizione assoluta nel frame)
    template <bool Inc> static void svfMod (Svf& q, const Svf& d, const Svf& a, int pos, double* z, double* x, int n) {
        double i1 = z[0], i2 = z[1];
        for (int i = 0; i < n; ++i) { if (Inc) { q.g += d.g; q.k += d.k; q.mh += d.mh; q.mb += d.mb; q.ml += d.ml; } else { const double t = (double) (pos + i + 1); q = Svf { a.g + d.g * t, a.k + d.k * t, a.mh + d.mh * t, a.mb + d.mb * t, a.ml + d.ml * t }; }
            const double h1 = 1 / (1 + q.g * (q.g + q.k)), h2 = q.g * h1, h3 = q.g * h2, v0 = x[i], v3 = v0 - i2, v1 = h1 * i1 + h2 * v3, v2 = i2 + h2 * i1 + h3 * v3;
            i1 = 2 * v1 - i1; i2 = 2 * v2 - i2; x[i] = q.mh * (v0 - q.k * v1 - v2) + q.mb * v1 + q.ml * v2; }
        z[0] = i1; z[1] = i2;
    }
    template <bool Inc> static void svfMod2 (Svf& q, const Svf& d, const Svf& a, int pos, double* zl, double* zr, double* xl, double* xr, int n) {
        double l1 = zl[0], l2 = zl[1], r1 = zr[0], r2 = zr[1];
        for (int i = 0; i < n; ++i) { if (Inc) { q.g += d.g; q.k += d.k; q.mh += d.mh; q.mb += d.mb; q.ml += d.ml; } else { const double t = (double) (pos + i + 1); q = Svf { a.g + d.g * t, a.k + d.k * t, a.mh + d.mh * t, a.mb + d.mb * t, a.ml + d.ml * t }; }
            const double h1 = 1 / (1 + q.g * (q.g + q.k)), h2 = q.g * h1, h3 = q.g * h2, a0 = xl[i], b0 = xr[i], a3 = a0 - l2, b3 = b0 - r2;
            const double a1 = h1 * l1 + h2 * a3, b1 = h1 * r1 + h2 * b3, a2 = l2 + h2 * l1 + h3 * a3, b2 = r2 + h2 * r1 + h3 * b3;
            l1 = 2 * a1 - l1; r1 = 2 * b1 - r1; l2 = 2 * a2 - l2; r2 = 2 * b2 - r2;
            xl[i] = q.mh * (a0 - q.k * a1 - a2) + q.mb * a1 + q.ml * a2; xr[i] = q.mh * (b0 - q.k * b1 - b2) + q.mb * b1 + q.ml * b2; }
        zl[0] = l1; zl[1] = l2; zr[0] = r1; zr[1] = r2;
    }
    static Svf svfDelta (const Svf& a, const Svf& b, double len) { return Svf { (b.g - a.g) / len, (b.k - a.k) / len, (b.mh - a.mh) / len, (b.mb - a.mb) / len, (b.ml - a.ml) / len }; }
    static void incSec (Coef& c, const Coef& q, double* z, double* x, int n) {   // una sezione biquad con coefficienti incrementali
        double b0 = c.b0, b1 = c.b1, b2 = c.b2, a1 = c.a1, a2 = c.a2, z1 = z[0], z2 = z[1];
        for (int i = 0; i < n; ++i) { b0 += q.b0; b1 += q.b1; b2 += q.b2; a1 += q.a1; a2 += q.a2; const double xi = x[i], y = b0 * xi + z1; z1 = b1 * xi - a1 * y + z2; z2 = b2 * xi - a2 * y; x[i] = y; }
        c = Coef { b0, b1, b2, a1, a2 }; z[0] = z1; z[1] = z2; }
    static void runIncS (SvfSet& c, const SvfSet& d, BandCoefs& rc, const BandCoefs& rd, double* z, double* x, int n) {
        for (int k = 0; k < c.n; ++k) { if (c.on[k]) svfMod<true> (c.s[k], d.s[k], c.s[k], 0, z + 2 * k, x, n); else incSec (rc.c[k], rd.c[k], z + 2 * k, x, n); } }
    static void runIncS2 (SvfSet& c, const SvfSet& d, BandCoefs& rc, const BandCoefs& rd, double* zl, double* zr, double* xl, double* xr, int n) {
        for (int k = 0; k < c.n; ++k) { if (c.on[k]) svfMod2<true> (c.s[k], d.s[k], c.s[k], 0, zl + 2 * k, zr + 2 * k, xl, xr, n);
            else { Coef c2 = rc.c[k]; incSec (rc.c[k], rd.c[k], zl + 2 * k, xl, n); incSec (c2, rd.c[k], zr + 2 * k, xr, n); } } }
    // stesso filtro con una sezione in piu o in meno (correzione vicino a Nyquist): la cascata piu corta si completa con sezioni neutre (b0 = 1),
    // le sezioni nuove partono da stato zero; l'interpolazione dei coefficienti resta stabile (triangolo di stabilita convesso)
    static void matchSecs (BandState& s, BandCoefs& nc) {
        const int m = std::max (nc.n, s.tgt.n);
        for (int k = nc.n; k < m; ++k) nc.c[k] = Coef();
        for (int k = s.tgt.n; k < m; ++k) { s.tgt.c[k] = Coef(); s.cur.c[k] = Coef(); s.fr0.c[k] = Coef(); s.rfrom.c[k] = Coef(); s.rc.c[k] = Coef(); s.rd.c[k] = Coef { 0, 0, 0, 0, 0 }; s.rcS.s[k] = Svf(); s.rcS.on[k] = false; s.rdS.s[k] = Svf { 0, 0, 0, 0, 0 }; for (int c = 0; c < 2; ++c) { s.z[c][2 * k] = 0; s.z[c][2 * k + 1] = 0; } }
        nc.n = m; s.tgt.n = m; s.cur.n = std::max (s.cur.n, m); s.fr0.n = std::max (s.fr0.n, m); s.rfrom.n = std::max (s.rfrom.n, m); s.rc.n = std::max (s.rc.n, m); s.rd.n = std::max (s.rd.n, m); s.rcS.n = std::max (s.rcS.n, m); s.rdS.n = std::max (s.rdS.n, m);
    }
    // coefficienti istantanei di una banda statica (a meta rampa: interpolati come nel filtro) e avvio di una rampa nuova verso nc
    // 0.5.0: tagli, shelf, passa-banda e tilt girano come SVF trapezoidale mentre i coefficienti si muovono, poi tornano biquad (stato equivalente,
    // uscita continua). Il biquad TDF-II con coefficienti interpolati passava per filtri intermedi sbagliati e accumulava energia: salto High Cut 48 dB
    // 20 kHz -> 200 Hz +11,5 dB su un tono (+34,8 dB su rumore), spazzata in 20 ms +4,7 dB (misurati). Bell, Notch e All Pass (una sezione:
    // interpolazione innocua, misurata 0,0 dB, e residuo di modulazione piu basso in biquad) restano biquad.
    static bool svfRamp (int type) { return ! (type == Bell || type == Notch || type == AllPass); }
    void startRamp (BandState& s, const BandCoefs& nc) {
        const bool mid = s.rpos < kRamp;   // rampa in corso: si riparte dai coefficienti dell'ultimo campione (continuita esatta)
        if (! mid) { s.rfrom = s.cur; s.rfrom.n = s.tgt.n; if (coefEq (s.rfrom, nc)) { s.tgt = nc; s.cur = nc; return; } s.rc = s.rfrom; for (int k = 0; k < kMaxSecs; ++k) s.rcS.on[k] = false; }
        s.tgt = nc; s.rpos = 0;
        for (int k = s.rc.n; k < s.tgt.n; ++k) { s.rc.c[k] = Coef(); s.rcS.on[k] = false; }
        s.rc.n = s.rd.n = s.rcS.n = s.rdS.n = s.tgt.n; s.svf = false;
        for (int k = 0; k < s.tgt.n; ++k) {   // per sezione: SVF se il tipo lo prevede e nessuno dei due estremi e del primo ordine; il passaggio conserva lo stato equivalente
            bool& on = s.rcS.on[k]; const Coef now = on ? fromSvf (s.rcS.s[k]) : s.rc.c[k];
            const bool want = svfRamp (s.p.type) && ! firstOrd (now) && ! firstOrd (nc.c[k]);
            if (want && ! on) { s.rcS.s[k] = toSvf (now); for (int c = 0; c < 2; ++c) tdfToSvf (now, s.rcS.s[k], &s.z[c][2 * k]); on = true; }
            else if (! want && on) { for (int c = 0; c < 2; ++c) svfToTdf (now, s.rcS.s[k], &s.z[c][2 * k]); s.rc.c[k] = now; on = false; }
            if (on) { s.rdS.s[k] = svfDelta (s.rcS.s[k], toSvf (nc.c[k]), kRamp); s.svf = true; }
            else { const Coef& p = s.rc.c[k]; const Coef& q = nc.c[k]; s.rd.c[k] = Coef { (q.b0 - p.b0) / kRamp, (q.b1 - p.b1) / kRamp, (q.b2 - p.b2) / kRamp, (q.a1 - p.a1) / kRamp, (q.a2 - p.a2) / kRamp }; }
        }
    }
    // salto grande in un solo cambio (oltre un'ottava, Q x2 o 12 dB) per tagli, shelf, passa-banda e tilt: dissolvenza tra vecchio e nuovo filtro invece della rampa
    // (rampa su un salto grande: si attraversano filtri intermedi lontani da entrambi; Low Shelf Q40 +30 -> -30 dB 100 Hz -> 5 kHz: tono +34 dB, misurato)
    static bool bigJump (const BandParams& a, double ga, const BandParams& b, double gb) {
        return svfRamp (b.type) && (std::abs (std::log2 (b.f / a.f)) > 1 || std::abs (std::log2 (b.q / a.q)) > 1 || std::abs (gb - ga) > 12); }
    static bool coefEq (const BandCoefs& a, const BandCoefs& b) { return a.n == b.n && std::memcmp (a.c, b.c, sizeof (Coef) * (size_t) a.n) == 0; }
    // coefficienti per il guadagno dinamico delta: tabella a passi fissi (progettati solo quando servono) + interpolazione lineare.
    // 0.4.3 riprogettava la banda intera a ogni frame in cui delta cambiava di oltre 0.02 dB (16 bande in movimento: 7 % di un core).
    void dynTarget (BandState& s, double delta, BandCoefs& out) {
        const double ad = std::abs (delta) / s.dynCell; int k = (int) ad; if (k > kDynCells - 2) k = kDynCells - 2; double fr = ad - k; if (fr > 1) fr = 1;
        // celle >= 1 con il progetto veloce (con ripiego preciso sopra 1 dB): costo limitato anche quando tutte le bande saltano nello stesso frame
        // (con il progetto preciso: 32 bande dinamiche, raffica di rumore dopo il silenzio, picco 1,2 ms in un blocco da 64 campioni: misurato)
        for (int j = k; j <= k + 1; ++j) if (s.dynValid[j] != s.dynGen) { BandCoefs t; designBand (s.p, s.g + s.dynSign * j * s.dynCell, grid, t, false); s.dynC[j] = t.c[0]; s.dynValid[j] = s.dynGen; }
        const Coef& c0 = s.dynC[k]; const Coef& c1 = s.dynC[k + 1]; Coef& c = out.c[0]; out.n = 1;
        c.b0 = c0.b0 + (c1.b0 - c0.b0) * fr; c.b1 = c0.b1 + (c1.b1 - c0.b1) * fr; c.b2 = c0.b2 + (c1.b2 - c0.b2) * fr; c.a1 = c0.a1 + (c1.a1 - c0.a1) * fr; c.a2 = c0.a2 + (c1.a2 - c0.a2) * fr;
    }

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
    void fadeOut (int i) {   // la banda attuale continua a suonare in old[i] e sfuma a zero in 15 ms
        if (! primed) { old[i].active = false; return; }
        if (linActive() && ! iirOnly (st[i]) && (lin->maskNow() >> i & 1u)) { old[i].active = false; return; }   // Linear Phase: banda statica nel FIR, la dissolvenza la fa il FIR
        const BandState& s = st[i]; const double w = s.flen > 0 ? fadeW ((double) s.fpos / s.flen) : 1.0;
        old[i] = s; old[i].w0 = w; old[i].fpos = 0; old[i].flen = fadeLen; old[i].active = true;
    }
    void bandStep (int bi, int n, bool haveSC) {
        BandState& s = st[bi]; BandState& o = old[bi];
        const bool fin = s.active && s.flen > 0, fout = o.active;
        if (! fin && ! fout) { if (s.active) bandProcess (bi, s, n, haveSC); return; }
        for (int i = 0; i < n; ++i) { xL[(size_t) i] = L[(size_t) i]; xR[(size_t) i] = R[(size_t) i]; }
        if (fout) { bandProcess (-1, o, n, haveSC); for (int i = 0; i < n; ++i) { oL[(size_t) i] = L[(size_t) i]; oR[(size_t) i] = R[(size_t) i]; L[(size_t) i] = xL[(size_t) i]; R[(size_t) i] = xR[(size_t) i]; } }
        if (s.active) bandProcess (bi, s, n, haveSC);
        for (int i = 0; i < n; ++i) {
            const double wi = s.active ? (s.flen > 0 ? fadeW ((double) (s.fpos + i + 1) / s.flen) : 1.0) : 0.0;
            const double wo = fout ? o.w0 * (1 - fadeW ((double) (o.fpos + i + 1) / o.flen)) : 0.0;
            const double xl = xL[(size_t) i], xr = xR[(size_t) i];
            L[(size_t) i] = (wi == 1.0 ? L[(size_t) i] : xl + wi * (L[(size_t) i] - xl)) + (fout && wo != 0.0 ? wo * (oL[(size_t) i] - xl) : 0.0);   // pesi 1/0: nessun arrotondamento (uscita identica con qualsiasi blocco)
            R[(size_t) i] = (wi == 1.0 ? R[(size_t) i] : xr + wi * (R[(size_t) i] - xr)) + (fout && wo != 0.0 ? wo * (oR[(size_t) i] - xr) : 0.0);
        }
        if (fin) { s.fpos += n; if (s.fpos >= s.flen) { s.fpos = 0; s.flen = 0; } }
        if (fout) { o.fpos += n; if (o.fpos >= o.flen) o.active = false; }
    }
    // msReady: M/S gia validi e L/R da ricostruire dopo (bande Mid/Side consecutive condividono una sola conversione)
    void bandProcess (int bi, BandState& s, int n, bool haveSC, bool msReady = false) {
        const int place = s.p.place; const bool ms = (place == Mid || place == Side) && ! msReady;
        if (ms) for (int i = 0; i < n; ++i) { M[(size_t) i] = 0.5 * (L[(size_t) i] + R[(size_t) i]); S[(size_t) i] = 0.5 * (L[(size_t) i] - R[(size_t) i]); }
        double* ch[2]; double* sc[2]; const bool useSC = s.dynOn && s.p.sc && haveSC; const int nc = chans (place, ch, sc, useSC);
        double* sd[2] = { nullptr, nullptr }; const bool sideRun = s.dynOn && sideOn && bi >= 0, useSide = sideRun && ! useSC;   // Linear Phase: detector sulla catena delle bande precedenti
        if (sideRun) { switch (place) { case Left: sd[0] = sxL.data(); break; case Right: sd[0] = sxR.data(); break;
                case Mid: case Side: for (int i = 0; i < n; ++i) { sxM[(size_t) i] = 0.5 * (sxL[(size_t) i] + sxR[(size_t) i]); sxS[(size_t) i] = 0.5 * (sxL[(size_t) i] - sxR[(size_t) i]); } sd[0] = place == Mid ? sxM.data() : sxS.data(); break;
                default: sd[0] = sxL.data(); sd[1] = sxR.data(); } }
        if (! s.dynOn) {
            int m = 0;   // rampa di lunghezza fissa (indipendente dal blocco), poi coefficienti fermi
            if (s.rpos < kRamp) { const int k = std::min (n, kRamp - s.rpos);
                if (s.svf) { if (nc == 2) runIncS2 (s.rcS, s.rdS, s.rc, s.rd, s.z[0], s.z[1], ch[0] + m, ch[1] + m, k); else runIncS (s.rcS, s.rdS, s.rc, s.rd, s.z[0], ch[0] + m, k); }
                else if (nc == 2) runInc2 (s.rc, s.rd, s.z[0], s.z[1], ch[0] + m, ch[1] + m, k); else runInc (s.rc, s.rd, s.z[0], ch[0] + m, k);
                s.rpos += k; m += k;
                if (s.rpos >= kRamp) { s.cur = s.tgt;   // fine della rampa: si torna biquad (stato equivalente con i coefficienti finali)
                    if (s.svf) { for (int k2 = 0; k2 < s.tgt.n; ++k2) if (s.rcS.on[k2]) { for (int c = 0; c < 2; ++c) svfToTdf (s.tgt.c[k2], s.rcS.s[k2], &s.z[c][2 * k2]); s.rcS.on[k2] = false; } s.svf = false; } s.rc = s.tgt; } }
            if (m < n) { if (nc == 2) run2 (s.tgt, s.z[0], s.z[1], ch[0] + m, ch[1] + m, n - m); else run (s.tgt, s.z[0], ch[0] + m, n - m); }
        } else {
            const Coef& q = s.det.c[0];   // detector a una sezione (BP/LP/HP 12 dB)
            constexpr int F = kFrame;
            for (int i0 = 0; i0 < n; ) {
                if (s.phase == 0) s.fr0 = s.cur;                    // inizio frame: rampa da cur a tgt (tgt deciso alla fine del frame precedente)
                const int i1 = std::min (n, i0 + (F - s.phase));
                for (int c = 0; c < nc; ++c) {                     // detector: somma dei quadrati e picco del frame
                    const double* x = useSC ? sc[c] : useSide ? sd[c] : ch[c]; double z1 = s.dz[c][0], z2 = s.dz[c][1], a2s = s.acc2[c], pk = s.pk[c];
                    for (int i = i0; i < i1; ++i) { const double xi = x[i], y = q.b0 * xi + z1; z1 = q.b1 * xi - q.a1 * y + z2; z2 = q.b2 * xi - q.a2 * y; a2s += y * y; const double ay = std::abs (y); if (ay > pk) pk = ay; }
                    s.dz[c][0] = z1; s.dz[c][1] = z2; s.acc2[c] = a2s; s.pk[c] = pk;
                }
                if (nc == 2) runFrame2 (s.fr0, s.tgt, s.z[0], s.z[1], ch[0], ch[1], i0, i1, s.phase, F); else runFrame (s.fr0, s.tgt, s.z[0], ch[0], i0, i1, s.phase, F);
                if (sideRun) { if (nc == 2) runFrame2 (s.fr0, s.tgt, s.sdz[0], s.sdz[1], sd[0], sd[1], i0, i1, s.phase, F); else runFrame (s.fr0, s.tgt, s.sdz[0], sd[0], i0, i1, s.phase, F); }   // la catena del detector passa anche dalle bande dinamiche (come in Zero Latency)
                s.phase += i1 - i0; i0 = i1;
                if (s.phase == F) {   // fine frame: livello (RMS vero o picco - 3.01 dB) -> attacco/rilascio in dB -> guadagno -> coefficienti dalla tabella
                    s.phase = 0; s.cur = s.tgt; double inst, fpk = 0;
                    for (int c = 0; c < nc; ++c) if (s.pk[c] > fpk) fpk = s.pk[c];
                    s.pkRing[s.pkPos] = fpk; s.pkPos = (s.pkPos + 1) % kPkHold;   // storia dei picchi di frame (scritta anche in RMS: passando a Peak e gia pronta)
                    if (s.p.det == DetPeak) { double pk = 0; for (int k = 1; k <= s.pkLen; ++k) { const double v = s.pkRing[(s.pkPos - k + kPkHold) % kPkHold]; if (v > pk) pk = v; }
                        for (int c = 0; c < nc; ++c) { s.pk[c] = 0; s.acc2[c] = 0; } inst = 20 * std::log10 (pk + 1e-10) - kSineRmsDb; }
                    else { double m = 0; for (int c = 0; c < nc; ++c) { if (s.detInit) s.ms[c] = s.acc2[c] / F; else s.ms[c] += (s.acc2[c] / F - s.ms[c]) * s.aRms; if (s.ms[c] > m) m = s.ms[c]; s.acc2[c] = 0; s.pk[c] = 0; } inst = 10 * std::log10 (m + 1e-20); }
                    if (s.detInit) { s.lvl = inst; s.detInit = false; } else s.lvl += (inst - s.lvl) * (inst > s.lvl ? s.aA : s.aR);
                    s.thrS += (s.p.thr - s.thrS) * aParam; BandParams dp = s.p; dp.thr = s.thrS;
                    s.delta = dynDelta (s.lvl, dp) * s.scale; dynTarget (s, s.delta, s.tgt);
                    if (bi >= 0) meterLevel[bi].store ((float) s.lvl, std::memory_order_relaxed);
                }
            }
            if (bi >= 0) meterDelta[bi].store ((float) s.delta, std::memory_order_relaxed);
            if (sideRun && (place == Mid || place == Side)) for (int i = 0; i < n; ++i) { sxL[(size_t) i] = sxM[(size_t) i] + sxS[(size_t) i]; sxR[(size_t) i] = sxM[(size_t) i] - sxS[(size_t) i]; }
        }
        if (ms) for (int i = 0; i < n; ++i) { L[(size_t) i] = M[(size_t) i] + S[(size_t) i]; R[(size_t) i] = M[(size_t) i] - S[(size_t) i]; }
    }
    void processChunk (float* Lio, float* Rio, const float* scL, const float* scR, int n) {
        {   // INPUT: guadagno d'ingresso con rampa di 20 ms; dL/dR restano il segnale originale (per il BYPASS)
            const double tgi = std::pow (10.0, glob.inDb / 20.0), gia = 1 - std::exp (-1 / (0.01 * fs)); double gi = inG, gi1 = inG1;
            for (int i = 0; i < n; ++i) {   // un campione NaN/Inf o oltre +60 dBFS in ingresso diventa 0 (0.4.3: un solo NaN rendeva l'uscita NaN per sempre)
                float l = Lio[i], r = Rio[i]; if (! (std::abs (l) <= kMaxIn)) l = 0; if (! (std::abs (r) <= kMaxIn)) r = 0;
                gi1 += (tgi - gi1) * gia; gi += (gi1 - gi) * gia; dL[(size_t) i] = l; dR[(size_t) i] = r; L[(size_t) i] = l * gi; R[(size_t) i] = r * gi; }
            inG = gi; inG1 = gi1; }
        const bool linOn = phaseMode == 1 && lin && lin->ready();
        if (linOn) {   // 0.6.0 Linear Phase: dry (bypass) e copia per il solo ritardati come il FIR; le bande statiche sono nel FIR
            if (linDirty && (absPos % (unsigned long long) linInterval() == 0 || ! linOnce)) { linDesign (! linOnce); linDirty = false; linOnce = true; }
            dlyDry.process (dL.data(), dR.data(), n);
            for (int i = 0; i < n; ++i) { rL[(size_t) i] = L[(size_t) i]; rR[(size_t) i] = R[(size_t) i]; } dlyRaw.process (rL.data(), rR.data(), n);
            lin->process (L.data(), R.data(), n, kW.data(), kMc.data(), kMf.data(), fxL.data(), fxR.data()); }
        const bool haveSC = scL != nullptr && scR != nullptr;
        if (haveSC) { for (int i = 0; i < n; ++i) { double l = scL[i], r = scR[i]; if (! (std::abs (l) <= kMaxIn)) l = 0; if (! (std::abs (r) <= kMaxIn)) r = 0; sL[(size_t) i] = l; sR[(size_t) i] = r; }
            if (linOn) dlySc.process (sL.data(), sR.data(), n);
            for (int i = 0; i < n; ++i) { sM[(size_t) i] = 0.5 * (sL[(size_t) i] + sR[(size_t) i]); sS[(size_t) i] = 0.5 * (sL[(size_t) i] - sR[(size_t) i]); } }
        // SOLO: il percorso solo lavora su una copia del segnale; le bande continuano a girare sotto (nessun salto all'uscita dal solo).
        // Accensione/spegnimento con dissolvenza di 15 ms; filtro del solo interpolato quando la banda si muove o si allarga.
        const bool soloOn = glob.solo >= 0 && glob.solo < kMaxBands && st[glob.solo].active;
        const bool soloRun = soloOn || soloW > 0;
        if (soloRun) {
            if (soloOn) {
                const BandState& b = st[glob.solo];
                const bool newId = soloSt.id != glob.solo || soloSt.c.n == 0;
                if (newId || ! same (soloSt.src, b.p)) {
                    BandParams sp; sp.used = true; sp.f = b.p.f; sp.slope = 12;
                    if (b.p.type == LowCut || b.p.type == LowShelf) { sp.type = HighCut; sp.q = std::sqrt (0.5); }
                    else if (b.p.type == HighCut || b.p.type == HighShelf || b.p.type == TiltShelf) { sp.type = LowCut; sp.q = std::sqrt (0.5); }
                    else { sp.type = BandPass; sp.q = std::max (0.3, b.p.q); }
                    designBand (sp, 0, grid, soloSt.tgt, false); soloSt.src = b.p; soloSt.place = b.p.place;   // filtro d'ascolto: progetto veloce (si ridisegna a ogni blocco mentre la banda si muove)
                    if (newId || soloSt.tgt.n != soloSt.c.n) { soloSt.c = soloSt.tgt; std::memset (soloSt.z, 0, sizeof (soloSt.z)); }
                    soloSt.id = glob.solo;
                }
            }
            const int pl = soloSt.place;
            for (int i = 0; i < n; ++i) {
                const double l = linOn ? rL[(size_t) i] : L[(size_t) i], r = linOn ? rR[(size_t) i] : R[(size_t) i], m = 0.5 * (l + r), sd = 0.5 * (l - r);
                pL[(size_t) i] = l; pR[(size_t) i] = r;
                if (pl == Mid) pL[(size_t) i] = pR[(size_t) i] = m; else if (pl == Side) pL[(size_t) i] = pR[(size_t) i] = sd;
                else if (pl == Left) pR[(size_t) i] = 0; else if (pl == Right) pL[(size_t) i] = 0;
            }
            if (! coefEq (soloSt.c, soloSt.tgt)) { runLerp (soloSt.c, soloSt.tgt, soloSt.z[0], pL.data(), 0, n); runLerp (soloSt.c, soloSt.tgt, soloSt.z[1], pR.data(), 0, n); soloSt.c = soloSt.tgt; }
            else run2 (soloSt.c, soloSt.z[0], soloSt.z[1], pL.data(), pR.data(), n);
        }
        int settleBudget = absPos % (unsigned long long) kSettleTick == 0 ? kSettlePerChunk : 0;   // al massimo 2 progetti precisi ogni 512 campioni: dopo un cambio di preset le 32 bande si assestano in 16 passi, non tutte insieme
        for (int b = 0; b < kMaxBands; ++b) {   // bande ferme da 50 ms: progetto preciso (interpolato dal veloce lungo il blocco)
            BandState& s = st[b]; if (! s.active || s.precise || (linOn && ! iirOnly (s))) continue;
            s.quiet += n; if (s.quiet < settleLen || settleBudget <= 0) continue; --settleBudget;
            BandCoefs nc; designBand (s.p, s.g, grid, nc, true, ! s.dynOn); s.precise = true;
            if (s.dynOn) { ++s.dynGen; s.dynC[0] = nc.c[0]; s.dynValid[0] = s.dynGen; dynTarget (s, s.delta, s.tgt); } else { if (nc.n != s.tgt.n) matchSecs (s, nc); startRamp (s, nc); }
        }
        bool msLive = false;   // bande Mid/Side consecutive (senza dissolvenze in corso): una sola conversione L/R -> M/S e ritorno
        if (linOn) { linBands (n, haveSC); goto bandsDone; }
        for (int b = 0; b < kMaxBands; ++b) {
            if (! st[b].active && ! old[b].active) continue;
            if (simpleStereo (b)) {   // 0.6.0: bande statiche Stereo ferme consecutive -> una sola cascata a fronte d'onda (sezioni indipendenti in parallelo, stesso risultato al bit)
                if (msLive) { for (int i = 0; i < n; ++i) { L[(size_t) i] = M[(size_t) i] + S[(size_t) i]; R[(size_t) i] = M[(size_t) i] - S[(size_t) i]; } msLive = false; }
                int m = 0; int b2 = b;
                for (; b2 < kMaxBands; ++b2) { if (! st[b2].active && ! old[b2].active) continue; if (! simpleStereo (b2)) break;
                    BandState& q = st[b2]; for (int k = 0; k < q.tgt.n && m < kCasMax; ++k) { csC[m] = &q.tgt.c[k]; csZl[m] = &q.z[0][2 * k]; csZr[m] = &q.z[1][2 * k]; ++m; } if (m >= kCasMax - kMaxSecs) { ++b2; break; } }
                runCascade2 (m, L.data(), R.data(), n); b = b2 - 1; continue; }
            const BandState& s = st[b]; const bool msBand = s.active && s.flen == 0 && ! old[b].active && (s.p.place == Mid || s.p.place == Side);
            if (msBand) {
                if (! msLive) { for (int i = 0; i < n; ++i) { M[(size_t) i] = 0.5 * (L[(size_t) i] + R[(size_t) i]); S[(size_t) i] = 0.5 * (L[(size_t) i] - R[(size_t) i]); } msLive = true; }
                bandProcess (b, st[b], n, haveSC, true);
            } else {
                if (msLive) { for (int i = 0; i < n; ++i) { L[(size_t) i] = M[(size_t) i] + S[(size_t) i]; R[(size_t) i] = M[(size_t) i] - S[(size_t) i]; } msLive = false; }
                bandStep (b, n, haveSC);
            }
        }
        if (msLive) for (int i = 0; i < n; ++i) { L[(size_t) i] = M[(size_t) i] + S[(size_t) i]; R[(size_t) i] = M[(size_t) i] - S[(size_t) i]; }
      bandsDone:
        if (soloRun) {
            const double tgt = soloOn ? 1.0 : 0.0, stp = primed ? 1.0 / fadeLen : 1.0; double w = soloW;
            for (int i = 0; i < n; ++i) {
                w = tgt > w ? std::min (tgt, w + stp) : std::max (tgt, w - stp); const double k = fadeW (w);
                L[(size_t) i] += k * (pL[(size_t) i] - L[(size_t) i]); R[(size_t) i] += k * (pR[(size_t) i] - R[(size_t) i]);
            }
            soloW = w; if (! soloOn && soloW <= 0) soloSt.id = -1;
        }
        // uscita: OUTPUT (−∞ sotto −60 dB) → Character (oversampling 8x) → DC → pan → polarità → bypass morbido
        const bool mute = glob.outDb <= -60; const double tg = mute ? 0.0 : std::pow (10.0, (glob.outDb + glob.autoGainDb) / 20.0), ga = 1 - std::exp (-1 / (0.01 * fs));   // smoother a due poli (10 + 10 ms)
        const double bt = glob.bypass ? 1 : 0, dcA = std::exp (-2 * kPi * 5 / fs), stp = primed ? 1.0 / fadeLen : 1.0;   // bypass e polarita: coseno rialzato in 15 ms
        // legge del pan (bilanciamento lineare, centro = 0 dB su entrambi): A = min(1, 1 − p), B = min(1, 1 + p); L/R: A→L, B→R; M/S: A→Mid, B→Side
        const double p = std::clamp (glob.pan, -1.0, 1.0), tA = std::min (1.0, 1 - p), tB = std::min (1.0, 1 + p), tP = glob.invert ? 1.0 : 0.0; const bool ms = glob.panMS;
        double g = outG, g1 = outG1, by = byp, pa = panA, pb = panB, pa1 = panA1, pb1 = panB1, po = pol; const int chm = glob.character;
        if (chm != lastChm) {   // cambio di Character: il nuovo modo parte pulito, gira 15 ms a vuoto (i filtri dell'oversampling si caricano: senza, transiente a -62 dB misurato) e poi si dissolve sul vecchio in 15 ms
            if (chm) { osm[chm][0].reset(); osm[chm][1].reset(); std::memset (dcm[chm], 0, sizeof (dcm[chm])); }
            chPrev = lastChm; chPos = primed ? 0 : 2 * fadeLen; lastChm = chm; }
        if (g == tg && g1 == tg && pa == tA && pa1 == tA && pb == tB && pb1 == tB && pa == 1.0 && pb == 1.0 && by == 0.0 && bt == 0.0 && po == 0.0 && tP == 0.0 && chm == 0 && chPos >= 2 * fadeLen) {
            for (int i = 0; i < n; ++i) { Lio[i] = (float) (L[(size_t) i] * g); Rio[i] = (float) (R[(size_t) i] * g); }   // 0.6.0: tutto a regime (stesso risultato al bit del ciclo completo, senza i suoi passi)
            return; }
        for (int i = 0; i < n; ++i) {
            g1 += (tg - g1) * ga; g += (g1 - g) * ga; if (mute && g < 1e-7) g = g1 = 0; pa1 += (tA - pa1) * ga; pa += (pa1 - pa) * ga; pb1 += (tB - pb1) * ga; pb += (pb1 - pb) * ga;
            by = bt > by ? std::min (bt, by + stp) : std::max (bt, by - stp); po = tP > po ? std::min (tP, po + stp) : std::max (tP, po - stp);
            double l = L[(size_t) i] * g, r = R[(size_t) i] * g;
            if (chm || chPos < 2 * fadeLen) {
                double nl, nr; chPath (chm, l, r, nl, nr, dcA);
                if (chPos < 2 * fadeLen) { double ol, orr; chPath (chPrev, l, r, ol, orr, dcA); ++chPos; const double k = chPos <= fadeLen ? 0.0 : fadeW ((double) (chPos - fadeLen) / fadeLen); nl = ol + k * (nl - ol); nr = orr + k * (nr - orr); }
                l = nl; r = nr; }
            if (ms) { const double m = 0.5 * (l + r) * pa, sd = 0.5 * (l - r) * pb; l = m + sd; r = m - sd; } else { l *= pa; r *= pb; }
            const double kp = 1 - 2 * fadeW (po), kb = fadeW (by); l *= kp; r *= kp;
            l += (dL[(size_t) i] - l) * kb; r += (dR[(size_t) i] - r) * kb;
            Lio[i] = (float) l; Rio[i] = (float) r;
        }
        outG = g; outG1 = g1; byp = by; panA = pa; panB = pb; panA1 = pa1; panB1 = pb1; pol = po;
    }

    void snapGlobals() {
        const bool mute = glob.outDb <= -60; outG = outG1 = mute ? 0.0 : std::pow (10.0, (glob.outDb + glob.autoGainDb) / 20.0);
        inG = inG1 = std::pow (10.0, glob.inDb / 20.0); byp = glob.bypass ? 1 : 0; pol = glob.invert ? 1 : 0;
        const double p = std::clamp (glob.pan, -1.0, 1.0); panA = panA1 = std::min (1.0, 1 - p); panB = panB1 = std::min (1.0, 1 + p);
        lastChm = chPrev = glob.character; chPos = 2 * fadeLen;
    }
    void chPath (int m, double l, double r, double& ol, double& orr, double dcA) {   // Character del modo m (0 = Clean, passa invariato)
        if (! m) { ol = l; orr = r; return; }
        double* d = dcm[m]; l = osm[m][0].run (m, l); r = osm[m][1].run (m, r);
        const double yl = l - d[0] + dcA * d[1]; d[0] = l; d[1] = yl; const double yr = r - d[2] + dcA * d[3]; d[2] = r; d[3] = yr; ol = yl; orr = yr;
    }
    double fs = 48000; int N = 512; Grid grid; Global glob;
    std::vector<double> L, R, M, S, dL, dR, sL, sR, sM, sS, xL, xR, oL, oR, pL, pR;
    static bool iirOnly (const BandState& s) { return s.dynOn || s.p.type == AllPass; }   // in Linear Phase restano biquad: dinamiche e All Pass (fase, non modulo)
    // intervallo tra due progetti Linear Phase: 512 campioni fino a FIR di 8192, poi L/16 (circa 85 ms a Maximum a ogni fs). In offline ogni progetto costa fino a 0,1 s
    // a 192 kHz Maximum: ogni 512 campioni il rendering era 34 volte piu lento del tempo reale (revisione esterna); ora tempo reale e offline seguono l'automazione allo stesso passo
    int linInterval() const { return lin ? std::max (kSettleTick, lin->firLength() / 16) : kSettleTick; }
    bool linActive() const { return phaseMode == 1 && lin && lin->ready(); }
    void linDesign (bool instant) {   // bande attive statiche, nell'ordine dell'indice
        BandParams p[kMaxBands]; double g[kMaxBands]; int ix[kMaxBands], nb = 0;
        for (int b = 0; b < kMaxBands; ++b) { const BandState& s = st[b]; if (! s.active || iirOnly (s)) continue; ix[nb] = b; p[nb] = s.p; g[nb] = s.g; ++nb; }
        lin->request (ix, p, g, nb, instant); }
    // banda statica applicata a due segnali secondo la collocazione, con il suo stato (catena del detector in Linear Phase)
    void staticOn (BandState& s, double* a, double* b, int n) {
        switch (s.p.place) {
            case Left: run (s.tgt, s.sz[0], a, n); break; case Right: run (s.tgt, s.sz[1], b, n); break;
            case Mid: case Side: { for (int i = 0; i < n; ++i) { sxM[(size_t) i] = 0.5 * (a[i] + b[i]); sxS[(size_t) i] = 0.5 * (a[i] - b[i]); }
                run (s.tgt, s.sz[0], s.p.place == Mid ? sxM.data() : sxS.data(), n); for (int i = 0; i < n; ++i) { a[i] = sxM[(size_t) i] + sxS[(size_t) i]; b[i] = sxM[(size_t) i] - sxS[(size_t) i]; } break; }
            default: run2 (s.tgt, s.sz[0], s.sz[1], a, b, n); }
    }
    // Linear Phase: il FIR ha gia applicato le bande statiche. Qui le bande biquad (dinamiche, All Pass) e, mentre il FIR non le contiene ancora o non piu,
    // le statiche nuove o in transizione. Peso di ogni banda per campione: k = w * (non nel FIR nuovo) + (1 - w) * (non nel FIR vecchio), w = dissolvenza del FIR.
    // Il detector di una banda dinamica legge l'ingresso (ritardato) passato dalle sole bande statiche di indice minore, come in Zero Latency.
    void linBands (int n, bool haveSC) {
        // l'uscita del FIR e x = c + f: c = w * FIR nuovo, f = (1 - w) * FIR vecchio (f = 0 fuori dalle dissolvenze).
        // Ogni banda agisce solo sulla parte che non la contiene gia: bersaglio t = (non nel nuovo ? c : 0) + (non nel vecchio ? f : 0); x <- x - t + banda(t).
        // (prima: la banda agiva su tutto x con un peso, e nel passaggio statica <-> dinamica veniva applicata due volte: +12 dB, revisione esterna; poi +3,9 dB)
        bool needSide = false; for (int b = 0; b < kMaxBands; ++b) if (st[b].active && st[b].dynOn && ! (st[b].p.sc && haveSC)) needSide = true;
        if (needSide) for (int i = 0; i < n; ++i) { sxL[(size_t) i] = rL[(size_t) i]; sxR[(size_t) i] = rR[(size_t) i]; }
        sideOn = needSide;
        for (int b = 0; b < kMaxBands; ++b) {
            BandState& s = st[b]; BandState& o = old[b];
            if (! s.active && ! o.active) continue;
            const unsigned bit = 1u << b; bool any = false, whole = true;
            for (int i = 0; i < n; ++i) { const bool lc = ! (kMc[(size_t) i] & bit), lf = ! (kMf[(size_t) i] & bit);
                const double fl = fxL[(size_t) i], fr = fxR[(size_t) i], cl = L[(size_t) i] - fl, cr = R[(size_t) i] - fr;
                tgL[(size_t) i] = (lc ? cl : 0.0) + (lf ? fl : 0.0); tgR[(size_t) i] = (lc ? cr : 0.0) + (lf ? fr : 0.0);
                if (lc || lf) any = true; if (! (lc && lf)) whole = false; }
            bool inBoth = true; for (int i = 0; i < n; ++i) if (! ((kMc[(size_t) i] & kMf[(size_t) i]) & bit)) { inBoth = false; break; }
            if (inBoth) s.lpFromDyn = false;
            // banda statica che entra o esce: solo il FIR (dissolvenza tra due FIR). Il biquad a fase minima insieme al FIR a fase lineare si cancellava vicino ai tagli ripidi
            // (Low Cut 96 dB/oct: buco di -20 dB, revisione esterna). Il biquad serve solo ai passaggi dinamica <-> statica.
            if (s.active && ! iirOnly (s) && ! s.lpFromDyn && ! (o.active && iirOnly (o))) any = false;
            const bool runIt = any || iirOnly (s) || (o.active && iirOnly (o));
            if (! runIt && s.linTail) {   // la banda e appena entrata nel FIR: la sua coda (stato del biquad) continua a suonare con ingresso nullo, poi si spegne
                double mx = 0; for (int i = 0; i < n; ++i) { kxL[(size_t) i] = L[(size_t) i]; kxR[(size_t) i] = R[(size_t) i]; L[(size_t) i] = R[(size_t) i] = 0; }
                bandStep (b, n, haveSC);
                for (int i = 0; i < n; ++i) { mx = std::max ({ mx, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]) }); L[(size_t) i] += kxL[(size_t) i]; R[(size_t) i] += kxR[(size_t) i]; }
                if (mx < 1e-10) { s.linTail = false; std::memset (s.z, 0, sizeof (s.z)); } }
            if (runIt) { s.linTail = true;
                if (whole) bandStep (b, n, haveSC);   // la banda non e in nessuno dei due FIR: agisce su tutto
                else { for (int i = 0; i < n; ++i) { kxL[(size_t) i] = L[(size_t) i] - tgL[(size_t) i]; kxR[(size_t) i] = R[(size_t) i] - tgR[(size_t) i]; L[(size_t) i] = tgL[(size_t) i]; R[(size_t) i] = tgR[(size_t) i]; }
                    bandStep (b, n, haveSC);
                    for (int i = 0; i < n; ++i) { const bool lc = ! (kMc[(size_t) i] & bit), lf = ! (kMf[(size_t) i] & bit);
                        if (lf && ! lc) { fxL[(size_t) i] = L[(size_t) i]; fxR[(size_t) i] = R[(size_t) i]; }   // la banda ha agito solo sulla parte vecchia
                        L[(size_t) i] += kxL[(size_t) i]; R[(size_t) i] += kxR[(size_t) i]; } }
            }
            if (needSide && s.active && ! iirOnly (s)) staticOn (s, sxL.data(), sxR.data(), n);   // la catena del detector avanza dopo la banda
        }
        sideOn = false;
    }
    std::unique_ptr<LinPhase> lin; bool linSync = true, sideOn = false; std::vector<double> kW, sxL, sxR, sxM, sxS, kk, kxL, kxR, fxL, fxR, tgL, tgR; std::vector<unsigned> kMc, kMf; Delay2 dlyDry, dlyRaw, dlySc; std::vector<double> rL, rR; int phaseMode = 0, phaseRes = 1; bool linDirty = true, linOnce = false;
    BandState st[kMaxBands], old[kMaxBands]; SoloState soloSt; double outG = 1, outG1 = 1, inG = 1, inG1 = 1, byp = 0, panA = 1, panB = 1, panA1 = 1, panB1 = 1, pol = 0, soloW = 0;
    Oversampled8x osm[3][2]; double dcm[3][4] = {}, aParam = 0.0645; int lastChm = 0, chPrev = 0, chPos = 1440, fadeLen = 720, settleLen = 2400; bool primed = false; unsigned long long absPos = 0;
};

} // namespace tsrq
