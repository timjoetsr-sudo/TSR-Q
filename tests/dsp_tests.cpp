// TSR Q · test del core DSP (stessa batteria del prototipo, + All Pass e 32 bande)
#include "../Source/TsrqDsp.h"
#include <cstdio>
#include <random>
#include <memory>
#include <chrono>
#include <complex>
using namespace tsrq;
static int fails = 0;
static void ok (bool c, const char* fmt, double v1 = 0, double v2 = 0) { std::printf ("%s ", c ? "PASS" : "FAIL"); std::printf (fmt, v1, v2); std::printf ("\n"); if (! c) ++fails; }
static double db (double x) { return 10 * std::log10 (std::max (x, 1e-30)); }

static double maxErr (const BandParams& b, double fs, const Grid& g, double floorDb = -40) {
    SecSet set; sectionsFor (b, hasGain (b.type) ? b.gain : 0, set); BandCoefs c; designBand (b, hasGain (b.type) ? b.gain : 0, g, c);
    double m = 0;
    for (int i = 0; i <= 600; ++i) { const double f = 20 * std::pow (1000.0, i / 600.0); if (f >= fs / 2 * 0.999) break;
        const double A = std::max (db (anaMag2 (set, f)), floorDb), D = std::max (db (bandMag2 (c, f, fs)), floorDb); m = std::max (m, std::abs (A - D)); }
    return m;
}
static void fftMag (std::vector<double>& x, std::vector<double>& mag) {
    const size_t N = x.size(); std::vector<std::complex<double>> a (N); for (size_t i = 0; i < N; ++i) a[i] = x[i];
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= N; len <<= 1) { const double ang = -2 * kPi / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < N; i += len) { std::complex<double> w (1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } } }
    mag.resize (N / 2); for (size_t k = 0; k < N / 2; ++k) mag[k] = std::norm (a[k]);
}
static BandParams band (int type, double f, double g, double q, int slope = 12, int place = Stereo) { BandParams b; b.used = true; b.type = type; b.f = f; b.gain = g; b.q = q; b.slope = slope; b.place = place; return b; }

int main() {
    std::printf ("== 1. PRECISIONE vs prototipo analogico (20 Hz-20 kHz, pavimento -40 dB) ==\n");
    std::vector<double> lo, lo8, hi; double worst8 = 0; char worstTxt[160] = "";
    for (double fs : { 44100.0, 48000.0, 96000.0 }) { Grid g; g.init (fs);
        for (int t : { Bell, LowShelf, HighShelf, TiltShelf, Notch, BandPass, LowCut, HighCut, FlatTilt })
            for (double f : { 30.0, 250.0, 1000.0, 4000.0, 8000.0, 11000.0, 12000.0, 16000.0 })
                for (double gn : { -12.0, 6.0, 18.0 }) for (double q : { 0.5, 1.0, 4.0 }) {
                    std::vector<int> slopes = (t == LowCut || t == HighCut) ? std::vector<int> { 6, 12, 24, 48, 96 } : (t == LowShelf || t == HighShelf) ? std::vector<int> { 6, 12 } : t == BandPass ? std::vector<int> { 6 } : std::vector<int> { 12 };   // Band Pass 6 dB/oct = una sezione (come prima)
                    for (int sl : slopes) { const double e = maxErr (band (t, f, gn, q, sl), fs, g); if (f <= fs / 4) lo.push_back (e); else hi.push_back (e); if (f <= fs / 8) { lo8.push_back (e); if (e > worst8) { worst8 = e; std::snprintf (worstTxt, sizeof worstTxt, "tipo %d f %.0f g %.0f q %.2f pendenza %d fs %.0f", t, f, gn, q, sl, fs); } } }
                } }
    auto pct = [] (std::vector<double> v, double p) { std::sort (v.begin(), v.end()); return v[(size_t) ((v.size() - 1) * p)]; };
    std::printf ("casi f0<=fs/4: %zu | mediana %.3f | 95%% %.3f | max %.3f dB\n", lo.size(), pct (lo, .5), pct (lo, .95), pct (lo, 1));
    std::printf ("casi f0>fs/4 : %zu | mediana %.3f | 95%% %.3f | max %.3f dB\n", hi.size(), pct (hi, .5), pct (hi, .95), pct (hi, 1));
    ok (pct (lo, .95) < 1.0, "f0 <= fs/4: 95 percentile %.3f dB < 1.0 dB", pct (lo, .95));
    std::printf ("caso peggiore f0<=fs/8: %s -> %.3f dB\n", worstTxt, worst8);
    ok (pct (lo8, 1) < 1.0, "f0 <= fs/8: errore massimo %.3f dB < 1.0 dB", pct (lo8, 1));

    std::printf ("== 1b. Band Pass 12/24/36/48/72/96 dB/oct: precisione nella banda utile (sopra -20 dB) ==\n");
    { double m = 0, mAll = 0; for (double fs : { 44100.0, 48000.0, 96000.0 }) { Grid g; g.init (fs);
        for (double f : { 30.0, 250.0, 1000.0, 4000.0, 8000.0 }) for (double q : { 0.5, 1.0, 4.0 }) for (int sl : { 12, 24, 36, 48, 72, 96 }) {
            if (f > fs / 8) continue; m = std::max (m, maxErr (band (BandPass, f, 0, q, sl), fs, g, -20)); mAll = std::max (mAll, maxErr (band (BandPass, f, 0, q, sl), fs, g)); } }
      std::printf ("errore massimo sopra -20 dB: %.3f dB | fino a -40 dB (vicino a Nyquist, il digitale scende un po' di piu): %.3f dB\n", m, mAll);
      ok (m < 1.0, "Band Pass multi-sezione, f0 <= fs/8: errore massimo sopra -20 dB %.3f dB < 1.0 dB", m); }

    std::printf ("== 2. STABILITA, 20000 bande casuali (10 tipi) ==\n");
    { std::mt19937 rng (7); std::uniform_real_distribution<double> U (0, 1); int bad = 0; Grid gs[4]; double fss[4] = { 44100, 48000, 88200, 96000 }; for (int i = 0; i < 4; ++i) gs[i].init (fss[i]);
      for (int i = 0; i < 20000; ++i) { const Grid& g = gs[i % 4]; BandParams b = band (i % NumTypes, 10 * std::pow (3000.0, U (rng)), -30 + 60 * U (rng), 0.025 * std::pow (1600.0, U (rng)), 6 * (1 + (int) (U (rng) * 16)));
        BandCoefs c; designBand (b, b.gain, g, c); for (int k = 0; k < c.n; ++k) if (! stable (c.c[k])) ++bad; }
      ok (bad == 0, "sezioni instabili o non finite: %.0f", bad); }

    std::printf ("== 3. COERENZA motore: risposta all'impulso (FFT) == curva disegnata ==\n");
    { const double fs = 48000; const int N = 65536; Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl);
      BandParams bs[5] = { band (LowCut, 30, 0, .71, 48), band (Bell, 250, -6, 2), band (HighShelf, 9000, 4, .7), band (Notch, 3150, 0, 8), band (FlatTilt, 1000, -6, 1) };
      for (int i = 0; i < 5; ++i) e.setBand (i, bs[i]);
      std::vector<float> L (N, 0), R (N, 0); L[0] = R[0] = 1; e.process (L.data(), R.data(), nullptr, nullptr, N);
      std::vector<double> x (L.begin(), L.end()), H; fftMag (x, H); double m = 0;
      for (int k = 3; k < N / 2; ++k) { const double f = k * fs / N; if (f < 20 || f > 20000) continue; double p = 1;
        for (auto& b : bs) { BandCoefs c; designBand (b, hasGain (b.type) ? b.gain : 0, e.getGrid(), c); p *= bandMag2 (c, f, fs); }
        if (db (p) < -60) continue; m = std::max (m, std::abs (db (H[(size_t) k]) - db (p))); }
      ok (m < 0.01, "differenza max impulso-FFT vs curva = %.2e dB", m); }

    std::printf ("== 4. NULL TEST bande a 0 dB, ALL PASS = modulo piatto ==\n");
    { const double fs = 48000; const int n = 48000; Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl);
      e.setBand (0, band (Bell, 1000, 0, 1)); e.setBand (1, band (LowShelf, 100, 0, .7, 12, Mid));
      std::mt19937 rng (1); std::uniform_real_distribution<float> U (-1, 1); std::vector<float> L (n), R (n), L0, R0; for (int i = 0; i < n; ++i) { L[i] = U (rng); R[i] = U (rng); } L0 = L; R0 = R;
      e.process (L.data(), R.data(), nullptr, nullptr, n); double m = 0; for (int i = 0; i < n; ++i) m = std::max ({ m, (double) std::abs (L[i] - L0[i]), (double) std::abs (R[i] - R0[i]) });
      ok (db (m * m) < -120, "residuo bande a 0 dB = %.1f dBFS", db (m * m));
      Grid g; g.init (fs); double am = 0; for (double f : { 50.0, 500.0, 2000.0, 9000.0 }) for (double q : { .3, 1.0, 5.0 }) { BandCoefs c; designBand (band (AllPass, f, 0, q), 0, g, c);
        for (int i = 0; i < 200; ++i) am = std::max (am, std::abs (db (bandMag2 (c, 20 * std::pow (1000.0, i / 199.0), fs)))); }
      ok (am < 1e-6, "ALL PASS: scarto dal modulo piatto = %.1e dB (tolleranza 1e-6: arrotondamento double)", am); }

    std::printf ("== 5. STEREO: MID non tocca il SIDE, LEFT non tocca il RIGHT ==\n");
    for (int place : { Mid, Left }) { const double fs = 48000; const int n = 48000; Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); e.setBand (0, band (Bell, 1000, 12, 1, 12, place));
      std::mt19937 rng (3); std::uniform_real_distribution<float> U (-.1f, .1f); std::vector<float> L (n), R (n); for (int i = 0; i < n; ++i) { L[i] = (float) (.25 * std::sin (2 * kPi * 1000 * i / fs)) + U (rng); R[i] = U (rng) * 2; }
      auto L0 = L, R0 = R; e.process (L.data(), R.data(), nullptr, nullptr, n); double m = 0, chg = 0;
      for (int i = 0; i < n; ++i) { if (place == Mid) m = std::max (m, std::abs (.5 * (L[i] - R[i]) - .5 * (L0[i] - R0[i]))); else m = std::max (m, (double) std::abs (R[i] - R0[i])); if (i > 1000) chg = std::max (chg, (double) std::abs (L[i] - L0[i])); }
      ok (db (m * m) < -120 && chg > .1, place == Mid ? "MID: residuo sul SIDE %.1f dBFS, canale trattato cambiato di %.3f" : "LEFT: residuo sul RIGHT %.1f dBFS, canale trattato cambiato di %.3f", db (m * m), chg); }

    std::printf ("== 6. DINAMICA: sopra/sotto soglia, espansione, knee, Peak, sidechain ==\n");
    auto dynRun = [] (double amp, double thr, double range, int det, bool sc, double scAmp) {
        const double fs = 48000; const int n = 96000; Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl);
        BandParams b = band (Bell, 1000, 0, 1); b.dyn = true; b.thr = thr; b.range = range; b.att = 5; b.rel = 100; b.det = det; b.sc = sc; e.setBand (0, b);
        std::vector<float> L (n), R (n), sL (n), sR (n); for (int i = 0; i < n; ++i) { L[i] = R[i] = (float) (amp * std::sin (2 * kPi * 1000 * i / fs)); sL[i] = sR[i] = (float) (scAmp * std::sin (2 * kPi * 1000 * i / fs)); }
        e.process (L.data(), R.data(), sc ? sL.data() : nullptr, sc ? sR.data() : nullptr, n);
        double pk = 0; for (int i = n - 4800; i < n; ++i) pk = std::max (pk, (double) std::abs (L[i]));
        const double lvl = e.meterLevel[0].load(); return std::make_pair (20 * std::log10 (pk / amp), dynDelta (lvl, b)); };
    { auto a = dynRun (.5, -30, -12, DetRMS, false, 0); ok (std::abs (a.first - a.second) < .3 && a.first < -11, "sopra soglia: gain %.2f dB, atteso %.2f dB", a.first, a.second);
      auto b = dynRun (.001, -30, -12, DetRMS, false, 0); ok (std::abs (b.first) < .05, "sotto soglia: gain %.3f dB (atteso 0)", b.first);
      auto c = dynRun (.5, -30, 6, DetRMS, false, 0); ok (std::abs (c.first - 6) < .3, "espansione: gain %.2f dB, atteso +6", c.first);
      auto d = dynRun (.5, -30, -12, DetPeak, false, 0); ok (std::abs (d.first - d.second) < .3, "detector PEAK: gain %.2f dB, atteso %.2f dB", d.first, d.second);
      auto s1 = dynRun (.001, -30, -12, DetRMS, true, .5); ok (s1.first < -11, "SIDECHAIN forte, segnale debole: gain %.2f dB (attesa riduzione)", s1.first);
      auto s2 = dynRun (.5, -30, -12, DetRMS, true, 0); ok (std::abs (s2.first) < .05, "SIDECHAIN muto, segnale forte: gain %.3f dB (atteso 0)", s2.first);
      BandParams k = band (Bell, 1000, 0, 1); k.thr = -20; k.range = -24; k.knee = 12; const double k0 = dynDelta (-20, k), k1 = dynDelta (-26.5, k);
      ok (k0 < 0 && k0 > -3 && k1 == 0, "knee 12 dB: a soglia %.2f dB, 6,5 dB sotto %.2f dB", k0, k1); }

    std::printf ("== 7. PENDENZE: attenuazione a fc/2 vs Butterworth analogico ==\n");
    { Grid g; g.init (96000); double w = 0; for (int sl : { 6, 12, 18, 24, 36, 48, 72, 96 }) { BandCoefs c; designBand (band (LowCut, 1000, 0, std::sqrt (.5), sl), 0, g, c);
        const double a = -db (bandMag2 (c, 500, 96000)), ex = 10 * std::log10 (1 + std::pow (4.0, sl / 6)); w = std::max (w, std::abs (a - ex)); }
      ok (w < 0.1, "scarto max 6..96 dB/oct = %.4f dB", w); }

    std::printf ("== 8. PRESTAZIONI: 32 bande (28 dinamiche + 4 tagli 96 dB/oct), stereo, 10 s @48k, blocchi 128 ==\n");
    { const double fs = 48000; Engine e; e.prepare (fs, 128); Engine::Global gl; gl.character = 2; e.setGlobal (gl);
      for (int i = 0; i < 28; ++i) { BandParams b = band (i % 4 == 0 ? Bell : i % 4 == 1 ? LowShelf : i % 4 == 2 ? HighShelf : TiltShelf, 40 * std::pow (1.2, i), 3, 1.5, 12, i % 5); b.dyn = true; b.thr = -30; b.range = -6; e.setBand (i, b); }
      for (int i = 0; i < 4; ++i) e.setBand (28 + i, band (i % 2 ? HighCut : LowCut, i % 2 ? 18000 : 25, 0, .71, 96));
      std::mt19937 rng (5); std::uniform_real_distribution<float> U (-.5f, .5f); std::vector<float> L (128), R (128); const int blocks = (int) (fs * 10 / 128);
      auto t0 = std::chrono::steady_clock::now(); for (int k = 0; k < blocks; ++k) { for (int i = 0; i < 128; ++i) { L[i] = U (rng); R[i] = U (rng); } e.process (L.data(), R.data(), nullptr, nullptr, 128); }
      const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
      std::printf ("   tempo per 10 s di audio: %.0f ms -> %.1f %% di un core\n", ms, ms / 100);
      ok (ms < 10000, "piu veloce del tempo reale (x%.1f)", 10000 / ms); }

    // ---------- funzioni del master ----------
    auto runSine = [] (Engine& e, double fs, double f, double amp, double phR, int secs, std::vector<double>& outL, std::vector<double>& outR, const float* inR = nullptr) {
        const int n = (int) (fs * secs); outL.assign ((size_t) n, 0); outR.assign ((size_t) n, 0); std::vector<float> L (256), R (256);
        for (int i0 = 0; i0 < n; i0 += 256) { for (int i = 0; i < 256; ++i) { const double t = (i0 + i) / fs; L[i] = (float) (amp * std::sin (2 * kPi * f * t)); R[i] = (float) (amp * phR * std::sin (2 * kPi * f * t)); }
            e.process (L.data(), R.data(), nullptr, nullptr, 256); for (int i = 0; i < 256 && i0 + i < n; ++i) { outL[(size_t) (i0 + i)] = L[i]; outR[(size_t) (i0 + i)] = R[i]; } } };
    auto rmsTail = [] (const std::vector<double>& x) { double s = 0; const size_t a = x.size() / 2; for (size_t i = a; i < x.size(); ++i) s += x[i] * x[i]; return std::sqrt (s / (double) (x.size() - a)); };
    std::printf ("== 9. USCITA: OUTPUT -inf/+36, pan L/R e M/S (bilanciamento lineare, centro 0 dB), polarita ==\n");
    { const double fs = 48000; std::vector<double> l, r; const double ref = 0.5 / std::sqrt (2.0);
      auto mk = [&] (double out, double pan, bool ms, bool inv, double phR) { Engine e; e.prepare (fs, 256); Engine::Global g; g.outDb = out; g.pan = pan; g.panMS = ms; g.invert = inv; e.setGlobal (g); runSine (e, fs, 1000, .5, phR, 1, l, r); };
      mk (-60, 0, false, false, 1); ok (rmsTail (l) == 0 && rmsTail (r) == 0, "OUTPUT a -60 = -inf: uscita %.1e / %.1e (attesa zero esatto)", rmsTail (l), rmsTail (r));
      mk (36, 0, false, false, 1); ok (std::abs (20 * std::log10 (rmsTail (l) / ref) - 36) < 0.01, "OUTPUT +36 dB: misurato %.3f dB", 20 * std::log10 (rmsTail (l) / ref));
      mk (0, 0.5, false, false, 1); ok (std::abs (20 * std::log10 (rmsTail (l) / ref) + 6.0206) < 0.01 && std::abs (20 * std::log10 (rmsTail (r) / ref)) < 0.01, "pan L/R +50%%: L %.3f dB (atteso -6.021), R %.3f dB (atteso 0)", 20 * std::log10 (rmsTail (l) / ref), 20 * std::log10 (rmsTail (r) / ref));
      mk (0, -1, false, false, 1); ok (rmsTail (r) < 1e-9 && std::abs (20 * std::log10 (rmsTail (l) / ref)) < 0.01, "pan L/R -100%%: R %.1e (atteso 0), L %.3f dB", rmsTail (r), 20 * std::log10 (rmsTail (l) / ref));
      mk (0, -1, true, false, -1); ok (rmsTail (l) < 1e-9 && rmsTail (r) < 1e-9, "pan M/S -100%% (solo Mid) su segnale tutto Side (R = -L): uscita %.1e / %.1e (attesa zero)", rmsTail (l), rmsTail (r));
      mk (0, 1, true, false, 1); ok (rmsTail (l) < 1e-9, "pan M/S +100%% (solo Side) su segnale tutto Mid (R = L): uscita %.1e (attesa zero)", rmsTail (l));
      mk (0, 0, true, false, 0); { const double a = rmsTail (l), b = rmsTail (r); ok (std::abs (20 * std::log10 (a / ref)) < 0.01 && b < 1e-9, "pan M/S al centro: neutro (L %.3f dB, R %.1e)", 20 * std::log10 (a / ref), b); }
      { Engine e; e.prepare (fs, 256); Engine::Global g; g.invert = true; e.setGlobal (g); runSine (e, fs, 1000, .5, 1, 1, l, r); double m = 0; for (size_t i = l.size() / 2; i < l.size(); ++i) m = std::max (m, std::abs (l[i] + .5 * std::sin (2 * kPi * 1000 * (double) i / fs)));
        ok (m < 1e-6, "polarita invertita: uscita = -ingresso, scarto %.1e", m); } }
    std::printf ("== 10. GAIN-Q INTERACTION (Bell): Q effettivo = Q * (1 + |dB|/15), max 40 ==\n");
    { Grid g; g.init (48000); double worst = 0;
      for (double gd : { -30.0, -12.0, -3.0, 3.0, 12.0, 30.0 }) for (double q : { 0.3, 1.0, 4.0, 40.0 }) { BandParams b = band (Bell, 1000, gd, q); b.gq = true; BandCoefs c; designBand (b, gd, g, c);
          BandParams r = band (Bell, 1000, gd, std::min (40.0, q * (1 + std::abs (gd) / 15))); SecSet ss; sectionsFor (r, gd, ss);
          for (double f : { 250.0, 700.0, 1000.0, 1400.0, 4000.0 }) worst = std::max (worst, std::abs (db (bandMag2 (c, f, 48000)) - db (anaMag2 (ss, f)))); 
          for (int k = 0; k < c.n; ++k) if (! stable (c.c[k])) worst = 99; }
      ok (worst < 0.5, "filtro reale = Bell con Q effettivo (24 casi fino a +/-30 dB e Q 40): scarto max %.3f dB, tutti stabili", worst);
      BandParams a = band (Bell, 1000, 12, 1), b2 = a; b2.gq = true; BandCoefs ca, cb; designBand (a, 12, g, ca); designBand (b2, 12, g, cb);
      const double wa = db (bandMag2 (ca, 2000, 48000)), wb = db (bandMag2 (cb, 2000, 48000));
      ok (wb < wa - 1, "+12 dB: con Gain-Q la campana e piu stretta (a 2 kHz %.2f dB invece di %.2f dB)", wb, wa);
      BandParams z = band (Bell, 1000, 0, 2); z.gq = true; BandCoefs cz; designBand (z, 0, g, cz); ok (std::abs (db (bandMag2 (cz, 1000, 48000))) < 1e-9, "0 dB con Gain-Q: piatto (%.1e dB)", db (bandMag2 (cz, 1000, 48000))); }
    std::printf ("== 11. CHARACTER: aliasing misurato senza e con oversampling 8x (sinusoide alta, vari livelli e fs) ==\n");
    { auto measure = [&] (double fs, double f0, double ampDb, int chm, bool os, double& alias, double& fund) {
          const int n = 1 << 15, skip = 4096; std::vector<double> x ((size_t) n); Oversampled8x o; double dcs[4] = {}; const double dcA = std::exp (-2 * kPi * 5 / fs), a = std::pow (10.0, ampDb / 20);
          for (int i = 0; i < n + skip; ++i) { const double s = a * std::sin (2 * kPi * f0 * i / fs); double y = os ? o.run (chm, s) : characterShape (chm, s); const double yy = y - dcs[0] + dcA * dcs[1]; dcs[0] = y; dcs[1] = yy; if (i >= skip) x[(size_t) (i - skip)] = yy; }
          std::vector<double> w (x); for (int i = 0; i < n; ++i) w[(size_t) i] *= 0.35875 - 0.48829 * std::cos (2 * kPi * i / n) + 0.14128 * std::cos (4 * kPi * i / n) - 0.01168 * std::cos (6 * kPi * i / n);
          std::vector<double> m; fftMag (w, m); const int k0 = (int) std::lround (f0 / fs * n); fund = 0; for (int k = k0 - 6; k <= k0 + 6; ++k) fund = std::max (fund, m[(size_t) k]);
          alias = 0; for (int k = 20; k < n / 2 - 4; ++k) { const double fk = (double) k * fs / n; bool harm = false; for (int h = 1; h * f0 < fs / 2; ++h) if (std::abs (fk - h * f0) < 8 * fs / n) harm = true; if (! harm && fk < 20000) alias = std::max (alias, m[(size_t) k]); } };
      double worstOld = -999, worstNew = -999;
      for (double fs : { 44100.0, 48000.0, 96000.0 }) for (double lv : { -12.0, -6.0, 0.0 }) for (int chm : { 1, 2 }) {
          const double fc = std::round (15000.0 * 32768 / fs) * fs / 32768;   /* tono centrato su un bin (campionamento coerente): senza, a 44.1k la dispersione della finestra (-94 dB) mascherava la misura */
          double a0, f0a, a1, f1a; measure (fs, fc, lv, chm, false, a0, f0a); measure (fs, fc, lv, chm, true, a1, f1a);
          const double ro = db (a0 / f0a), rn = db (a1 / f1a);   /* fftMag restituisce POTENZA: prima il rapporto veniva elevato al quadrato e i dB risultavano raddoppiati */ worstOld = std::max (worstOld, ro); worstNew = std::max (worstNew, rn);
          std::printf ("   fs %.0f, 15 kHz %+.0f dBFS, %s: alias sotto 20 kHz  senza OS %.1f dB  |  con OS 8x %.1f dB (rispetto alla fondamentale)\n", fs, lv, chm == 1 ? "Subtle" : "Warm  ", ro, rn); }
      ok (worstNew < worstOld - 20, "oversampling 8x: alias peggiore %.1f dB invece di %.1f dB (miglioramento > 20 dB)", worstNew, worstOld);
      ok (worstNew < -100, "alias peggiore con oversampling 8x sotto -100 dB rispetto alla fondamentale (%.1f dB)", worstNew);
      // banda passante e ritardo del filtro di oversampling (segnale piccolo: la saturazione e praticamente lineare)
      auto resp = [&] (double fs, double f, double& magDb, double& delay) { Oversampled8x o; const int n = (int) fs; double sr = 0, si = 0, sa = 0; const double a = 1e-4;
          for (int i = 0; i < n; ++i) { const double s = a * std::sin (2 * kPi * f * i / fs), y = o.run (1, s) / a; if (i > n / 2) { sr += y * std::sin (2 * kPi * f * i / fs); si += y * std::cos (2 * kPi * f * i / fs); sa += 0.5; } }
          magDb = 20 * std::log10 (std::hypot (sr, si) / sa); const double ph = std::atan2 (-si, sr); delay = -ph / (2 * kPi * f) * fs; };
      auto gdel = [&] (double f) { double ma, pa, mb, pb; resp (44100, f, ma, pa); resp (44100, f + 20, mb, pb); double dp = (pb - pa) / 44100 * (2 * kPi * f);   // pa/pb = -fase/(2 pi f) * fs
          const double ph1 = -pa * 2 * kPi * f / 44100, ph2 = -pb * 2 * kPi * (f + 20) / 44100; double d = ph2 - ph1; while (d > kPi) d -= 2 * kPi; while (d < -kPi) d += 2 * kPi; (void) dp; return std::abs (d / (2 * kPi * 20) * 44100); };
      double m1, d1, m10, d10, m18, d18, m20, d20; resp (44100, 1000, m1, d1); resp (44100, 10000, m10, d10); resp (44100, 18000, m18, d18); resp (44100, 20000, m20, d20);
      std::printf ("   filtro OS @44.1k: modulo 1 kHz %.3f dB, 10 kHz %.3f dB, 18 kHz %.3f dB, 20 kHz %.3f dB | ritardo di gruppo 1 kHz %.2f, 10 kHz %.2f, 18 kHz %.2f campioni\n", m1, m10, m18, m20, gdel (1000), gdel (10000), gdel (18000));
      ok (std::abs (m1) < 0.01 && std::abs (m10) < 0.01 && std::abs (m18) < 0.05, "banda passante piatta fino a 18 kHz @44.1k (scarto max %.3f dB)", std::max ({ std::abs (m1), std::abs (m10), std::abs (m18) }));
      std::printf ("   nota: con Character attivo il suono passa da questi filtri (ritardo di fase indicato sopra); con Clean nessun oversampling e nessun ritardo\n");
      { const double fs = 48000; double t[2]; for (int chm : { 0, 2 }) { Engine e; e.prepare (fs, 128); Engine::Global gl; gl.character = chm; e.setGlobal (gl); std::vector<float> L (128, .1f), R (128, .1f);
          auto t0 = std::chrono::steady_clock::now(); for (int k = 0; k < (int) (fs * 10 / 128); ++k) { for (int i = 0; i < 128; ++i) { L[i] = .3f * (float) std::sin (k * 128 + i); R[i] = L[i]; } e.process (L.data(), R.data(), nullptr, nullptr, 128); }
          t[chm ? 1 : 0] = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count(); }
        std::printf ("   CPU, 10 s stereo @48k senza bande: Clean %.0f ms, Warm con OS 8x %.0f ms\n", t[0], t[1]); ok (t[1] < 2000, "costo del Character con oversampling 8x: %.0f ms per 10 s (%.1f %% di un core)", t[1], t[1] / 100); } }

    std::printf ("== 12. SOLO: allargando o spostando la banda durante il solo cambia cio che si ascolta ==\n");
    { const double fs = 48000; auto lvl = [&] (double q, double f0) { Engine e; e.prepare (fs, 256); Engine::Global g; g.solo = 0; e.setGlobal (g);
          std::vector<float> L (256), R (256); double s = 0; int cnt = 0; BandParams b = band (Bell, 1000, 6, 4);
          for (int k = 0; k < 400; ++k) { if (k == 200) { b.q = q; b.f = f0; } e.setBand (0, b);   // a meta: banda allargata/spostata, solo sempre acceso
              for (int i = 0; i < 256; ++i) { L[i] = R[i] = (float) (0.5 * std::sin (2 * kPi * 2500 * (k * 256 + i) / fs)); } e.process (L.data(), R.data(), nullptr, nullptr, 256);
              if (k > 300) for (int i = 0; i < 256; ++i) { s += L[i] * L[i]; ++cnt; } }
          return 10 * std::log10 (s / cnt / 0.125); };
      const double narrow = lvl (4, 1000), wide = lvl (0.3, 1000), moved = lvl (4, 2500);
      ok (wide > narrow + 6, "solo su 1 kHz, tono a 2,5 kHz: Q 4 -> %.1f dB, allargata a Q 0.3 -> %.1f dB (si sente di piu)", narrow, wide);
      ok (moved > narrow + 6, "spostata a 2,5 kHz durante il solo: %.1f dB (prima %.1f dB)", moved, narrow); }

    std::printf ("== 13. CLICK: ogni cambio di stato misurato come energia sopra 6 kHz su un segnale senza contenuto sopra 1,5 kHz (blocchi da 256) ==\n");
    { const double fs = 48000; const int n = 48000 * 6, bl = 256;
      auto scen = [&] (const std::string& sc) {
          std::vector<float> L ((size_t) n), R ((size_t) n);
          for (int i = 0; i < n; ++i) { const double t = i / fs, env = 0.6 + 0.4 * std::sin (2 * kPi * 0.5 * t);
              const double v = env * (0.3 * std::sin (2 * kPi * 110 * t) + 0.25 * std::sin (2 * kPi * 220 * t + 1) + 0.2 * std::sin (2 * kPi * 330 * t + 2) + 0.15 * std::sin (2 * kPi * 440 * t)
                                   + 0.1 * std::sin (2 * kPi * 660 * t) + 0.08 * std::sin (2 * kPi * 880 * t) + 0.05 * std::sin (2 * kPi * 1320 * t)); L[(size_t) i] = (float) v; R[(size_t) i] = (float) (0.8 * v); }
          Engine e; e.prepare (fs, 4096); Engine::Global g; BandParams b = band (Bell, 500, 6, 2); if (sc == "slope") b = band (LowCut, 300, 0, .71, 12);
          for (int i0 = 0; i0 < n; i0 += bl) { const double u = (double) i0 / n; const int ph = (i0 / 12000) % 2, k = i0 / 12000;
              if (sc == "drag" || sc == "solo_drag") b.f = 200 * std::pow (10.0, u);
              if (sc == "solo_q") b.q = 0.3 * std::pow (10 / 0.3, u);
              g.solo = sc.rfind ("solo", 0) == 0 ? 0 : -1; if (sc == "solo_toggle") g.solo = ph ? 0 : -1;
              if (sc == "bypass_band") b.bypass = ph; if (sc == "used") b.used = ! ph;
              if (sc == "type") { const int t[4] = { Bell, LowShelf, HighShelf, Notch }; b.type = t[k % 4]; b.f = 800; }
              if (sc == "slope") { const int sl[3] = { 12, 24, 48 }; b.slope = sl[k % 3]; }
              if (sc == "place") { const int pl[3] = { Stereo, Mid, Side }; b.place = pl[k % 3]; }
              if (sc == "dyn") { b.dyn = ph; b.thr = -40; b.range = -1.5; }
              if (sc == "gbypass") g.bypass = ph; if (sc == "subtle") g.character = ph ? 1 : 0; if (sc == "outstep") g.outDb = ph ? -12 : 0; if (sc == "instep") g.inDb = ph ? -12 : 0;
              if (sc == "polarity") g.invert = ph; if (sc == "pan") g.pan = ph ? -1 : 0;
              e.setGlobal (g); e.setBand (0, b); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); }
          // misura: passa-alto 6 kHz 96 dB/oct (il motore stesso, fermo), picco RMS su finestre da 64 campioni, rispetto all'RMS del segnale
          Engine hp; hp.prepare (fs, 4096); Engine::Global gh; hp.setGlobal (gh); hp.setBand (0, band (LowCut, 6000, 0, .71, 96)); hp.process (L.data(), R.data(), nullptr, nullptr, n);
          double pk = 0; for (int i0 = 4800; i0 + 64 <= n; i0 += 64) { double s = 0; for (int i = i0; i < i0 + 64; ++i) s += L[(size_t) i] * L[(size_t) i]; pk = std::max (pk, s / 64); }
          return 10 * std::log10 (pk / 0.0338 + 1e-30); };   // 0.0338 = potenza media del segnale di prova (misurata)
      const char* names[] = { "static", "drag", "solo_static", "solo_drag", "solo_q", "solo_toggle", "bypass_band", "used", "type", "slope", "place", "dyn", "gbypass", "subtle", "outstep", "instep", "polarity", "pan" };
      const char* desc[] = { "parametri fermi", "frequenza 200 Hz -> 2 kHz", "solo fermo", "solo mentre la banda si sposta", "solo mentre la banda si allarga", "solo acceso/spento", "banda bypass on/off", "banda attivata/disattivata",
                             "cambio di tipo Bell/LowShelf/HighShelf/Notch", "cambio di pendenza 12/24/48", "cambio di collocazione Stereo/Mid/Side", "dinamica on/off", "bypass globale on/off", "Character Clean/Subtle",
                             "OUTPUT a gradino 0/-12 dB", "INPUT a gradino 0/-12 dB", "inversione di polarita", "pan a gradino" };
      double worst = -999; const double flo = scen ("static");
      for (size_t i = 0; i < sizeof (names) / sizeof (*names); ++i) { const double v = scen (names[i]); worst = std::max (worst, v); std::printf ("   %-14s %7.1f dB  (%s)\n", names[i], v, desc[i]); }
      std::printf ("   pavimento (parametri fermi) %.1f dB\n", flo);
      ok (worst < -90, "nessun click: energia sopra 6 kHz al massimo %.1f dB rispetto al segnale in tutti gli scenari (< -90 dB)", worst); }

    // ======================= 0.4.4: difetti trovati nell'analisi della 0.4.3 (ogni test fallisce sul motore 0.4.3 e passa sul nuovo) =======================
    auto dmag = [] (const BandCoefs& c, double f, double fs) {   // modulo digitale in long double, indipendente dal motore
        const std::complex<long double> z = std::polar ((long double) 1, (long double) (-2 * kPi * f / fs)), z2 = z * z; long double m = 1;
        for (int k = 0; k < c.n; ++k) { const auto& q = c.c[k]; m *= std::norm (((long double) q.b0 + (long double) q.b1 * z + (long double) q.b2 * z2) / ((long double) 1 + (long double) q.a1 * z + (long double) q.a2 * z2)); }
        return (double) m; };

    std::printf ("== 14. NaN / Inf IN INGRESSO: il motore si riprende subito (0.4.3: un solo NaN rendeva l'uscita NaN per sempre) ==\n");
    { const double fs = 48000; const int n = 48000 * 3;
      auto run = [&] (bool bad) { std::vector<float> L ((size_t) n), R ((size_t) n);
          for (int i = 0; i < n; ++i) { const double t = i / fs; L[(size_t) i] = (float) (0.3 * std::sin (2 * kPi * 220 * t) + 0.1 * std::sin (2 * kPi * 3100 * t)); R[(size_t) i] = (float) (0.25 * std::sin (2 * kPi * 330 * t)); }
          const float v[4] = { bad ? std::nanf ("") : 0.f, bad ? INFINITY : 0.f, bad ? -INFINITY : 0.f, bad ? std::nanf ("") : 0.f }; L[48000] = v[0]; R[60000] = v[1]; L[72000] = v[2]; R[72001] = v[3];
          Engine e; e.prepare (fs, 512); Engine::Global g; g.character = 2; e.setGlobal (g);
          BandParams d = band (Bell, 220, 3, 1); d.dyn = true; d.thr = -30; d.range = -6; e.setBand (0, d); e.setBand (1, band (LowCut, 40, 0, .71, 24)); e.setBand (2, band (HighShelf, 8000, 2, .71));
          for (int i0 = 0; i0 < n; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0));
          std::vector<float> o (L); o.insert (o.end(), R.begin(), R.end()); return o; };
      const auto a = run (true), b = run (false); int nonfin = 0; double md = 0;
      for (size_t i = 0; i < a.size(); ++i) { if (! std::isfinite (a[i])) ++nonfin; else md = std::max (md, (double) std::abs (a[i] - b[i])); }
      ok (nonfin == 0, "NaN, +Inf, -Inf in 4 punti (banda dinamica, Low Cut, shelf, Character Warm): campioni d'uscita non finiti = %.0f", nonfin);
      ok (nonfin == 0 && md == 0, "uscita identica al bit a quella con 0 al posto dei valori non validi (scarto %.1e)", md); }

    std::printf ("== 15. DETECTOR CALIBRATO: sinusoide a -20 dBFS RMS letta -20 dB in RMS e in Peak (picco - 3,01 dB) (0.4.3: RMS +2,9 dB, Peak +6 dB) ==\n");
    { double wr = 0, wp = 0;
      for (double fs : { 44100.0, 48000.0, 96000.0 }) for (double f : { 100.0, 1000.0, 5000.0 }) for (int det : { (int) DetRMS, (int) DetPeak }) {
          const int n = (int) (fs * 2); std::vector<float> L ((size_t) n), R ((size_t) n); const double amp = 0.1 * std::sqrt (2.0);
          for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * f * i / fs));
          Engine e; e.prepare (fs, 512); Engine::Global g; e.setGlobal (g); BandParams b = band (Bell, f, 0, 1); b.dyn = true; b.thr = -60; b.range = -1.5; b.att = 5; b.rel = 80; b.det = det; e.setBand (0, b);
          double mn = 1e9, mx = -1e9; for (int i0 = 0; i0 < n; i0 += 512) { e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0)); if (i0 > n / 2) { const double v = e.meterLevel[0].load(); mn = std::min (mn, v); mx = std::max (mx, v); } }
          const double err = std::max (std::abs (mn + 20), std::abs (mx + 20)); if (det == DetRMS) wr = std::max (wr, err); else wp = std::max (wp, err); }
      ok (wr < 0.1, "RMS: scarto massimo dal livello vero %.3f dB (44,1/48/96 kHz; 100 Hz, 1 kHz, 5 kHz)", wr);
      ok (wp < 0.15, "Peak: scarto massimo dal livello vero %.3f dB", wp); }

    std::printf ("== 16. DETECTOR DEL BELL LIMITATO ALLA BANDA (0.4.3: era un passa-alto: un tono a 5 kHz comprimeva un Bell a 200 Hz di tutto il RANGE) ==\n");
    { auto red = [] (double ft) { const double fs = 48000; const int n = (int) fs * 2; std::vector<float> L ((size_t) n), R ((size_t) n);
          for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.316 * std::sqrt (2.0) * std::sin (2 * kPi * ft * i / fs));
          Engine e; e.prepare (fs, 512); Engine::Global g; e.setGlobal (g); BandParams b = band (Bell, 200, 0, 1); b.dyn = true; b.thr = -40; b.knee = 6; b.range = -6; e.setBand (0, b);
          for (int i0 = 0; i0 < n; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0));
          return (double) e.meterDelta[0].load(); };
      const double in = red (200), o5 = red (5000), o10 = red (10000);
      ok (in < -5.9, "tono a 200 Hz a -10 dBFS (in banda): riduzione %.2f dB (atteso tutto il RANGE, -6)", in);
      ok (o5 > -2.0 && o10 > -0.5, "tono a 5 kHz a -10 dBFS (fuori banda): riduzione %.2f dB; a 10 kHz %.2f dB (detector passa-banda 2o ordine: -28 dB a 5 kHz)", o5, o10); }

    std::printf ("== 17. CHARACTER OLTRE 0 dBFS: curva monotona (0.4.3: Warm ripiegava sotto -1,06 di ingresso, Subtle oltre -2,2/+2,5) ==\n");
    { double worst = 0; char wtxt[160] = "";
      // curva monotona: nel semiperiodo positivo l'uscita al picco d'ingresso non e piu bassa di quella dove l'ingresso, scendendo, ripassa per il 90 %
      // (simmetrico nel semiperiodo negativo). Il punto al 90 % e preso DOPO il picco: l'abbassamento del filtro anti-DC (5 Hz) va a favore, non contro.
      for (int chm : { 1, 2 }) for (double amp : { 1.5, 2.5, 4.0, 8.0 }) {
          const double fs = 48000; const int n = 48000, P = 480, dly = 7; std::vector<float> L ((size_t) n), R ((size_t) n);
          for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * 100 * i / fs));
          Engine e; e.prepare (fs, 512); Engine::Global g; g.character = chm; e.setGlobal (g); for (int i0 = 0; i0 < n; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0));
          auto at = [&] (int i) { double m = 0; for (int k = -2; k <= 2; ++k) m += L[(size_t) (i + dly + k)]; return m / 5; };
          const int a90 = (int) std::lround (P * (0.5 - std::asin (0.9) / kPi) / 2 + P / 4.0) - P / 4;   // campioni dal picco al 90 % in discesa
          double span = 0; for (int i = n / 2; i < n; ++i) span = std::max (span, (double) std::abs (L[(size_t) i]));
          for (int c0 = n / 2; c0 + P + 64 <= n; c0 += P) { const int pp = c0 + P / 4, pn = c0 + 3 * P / 4;   // picco positivo e negativo dell'ingresso (fase 0 a c0)
              const double vpos = at (pp + a90) - at (pp), vneg = at (pn) - at (pn + a90), v = std::max (vpos, vneg) / span;
              if (v > worst) { worst = v; std::snprintf (wtxt, sizeof wtxt, "%s ingresso x%.1f", chm == 1 ? "Subtle" : "Warm", amp); } } }
      ok (worst < 0.005, "uscita al picco d'ingresso mai 'dentro' quella al 90 %% (Subtle e Warm, 100 Hz, da +3,5 a +18 dBFS): violazione peggiore %.2f%% del picco d'uscita", worst * 100);
      if (worst >= 0.005) std::printf ("   caso peggiore: %s\n", wtxt); }

    std::printf ("== 18. PRECISIONE VICINO A NYQUIST (44,1 e 48 kHz, f0 10-20 kHz, scarto max 20 Hz-20 kHz, pavimento -30 dB) ==\n");
    { auto err = [&] (const BandParams& b, double fs) { Grid g; g.init (fs); SecSet set; sectionsFor (b, hasGain (b.type) ? b.gain : 0, set); BandCoefs c; designBand (b, hasGain (b.type) ? b.gain : 0, g, c);
          double m = 0; for (int i = 0; i < 1200; ++i) { const double f = 20 * std::pow (1000.0, i / 1199.0); if (f > 20000 || f > fs / 2 * 0.999) break;
              const double A = std::max (db (anaMag2 (set, f)), -30.0), D = std::max (db (dmag (c, f, fs)), -30.0); m = std::max (m, std::abs (A - D)); } return m; };
      double w[4] = {}; const char* nm[4] = { "Bell", "Notch", "High Cut", "High Shelf" };
      for (double fs : { 44100.0, 48000.0 }) for (double f : { 10000.0, 12000.0, 14000.0, 16000.0, 18000.0, 19000.0, 20000.0 }) {
          for (double q : { 0.5, 1.0, 2.0, 4.0, 8.0, 20.0 }) { for (double gd : { -18.0, -9.0, 9.0, 18.0 }) { w[0] = std::max (w[0], err (band (Bell, f, gd, q), fs)); w[3] = std::max (w[3], err (band (HighShelf, f, gd, q), fs)); }
              w[1] = std::max (w[1], err (band (Notch, f, 0, q), fs)); }
          for (int sl : { 12, 24, 48 }) w[2] = std::max (w[2], err (band (HighCut, f, 0, .71, sl), fs)); }
      for (int t = 0; t < 3; ++t) ok (w[t] < 0.5, t == 0 ? "Bell (Q 0,5-20, +/-9 e 18 dB): scarto max %.3f dB (0.4.3: 4,2 dB)" : t == 1 ? "Notch (Q 0,5-20): scarto max %.3f dB (0.4.3: 7,1 dB)" : "High Cut 12/24/48 dB/oct: scarto max %.3f dB (0.4.3: 1,5 dB)", w[t]);
      ok (w[3] < 3.0, "High Shelf (Q 0,5-20, +/-9 e 18 dB): scarto max %.3f dB (0.4.3: 27,6 dB; resta sopra 1 dB solo con Q 20 e taglio)", w[3]);
      const double hs = err (band (HighShelf, 16000, 18, 20), 44100);
      ok (hs < 0.5, "High Shelf 16 kHz +18 dB Q 20 a 44,1 kHz: scarto %.3f dB (0.4.3: +19 dB in piu a 20 kHz, poli analogici oltre Nyquist)", hs); }

    std::printf ("== 19. INVARIANZA AL BLOCCO con dinamica RMS e Peak, Mid/Side, Character e solo: blocchi 1/7/64/333/4096/casuali = blocchi da 512, al bit ==\n");
    { const double fs = 48000; const int n = 48000 * 4;
      std::vector<float> L0 ((size_t) n), R0 ((size_t) n); std::mt19937 rng (3); std::uniform_real_distribution<double> U (-1, 1);
      for (int i = 0; i < n; ++i) { const double t = i / fs, env = (i / 6000) % 3 == 0 ? 1.0 : 0.2; L0[(size_t) i] = (float) (env * (0.3 * std::sin (2 * kPi * 220 * t) + 0.05 * U (rng))); R0[(size_t) i] = (float) (env * (0.2 * std::sin (2 * kPi * 3300 * t) + 0.05 * U (rng))); }
      for (int solo : { -1, 1 }) {
          auto rend = [&] (int bl) { std::vector<float> L (L0), R (R0); Engine e; e.prepare (fs, 4096); Engine::Global g; g.character = 1; g.solo = solo; std::mt19937 r2 (11);
              BandParams d = band (Bell, 220, 0, 1.2); d.dyn = true; d.thr = -30; d.range = -6; d.att = 1; d.rel = 40;
              BandParams p = band (HighShelf, 3000, 2, .71); p.dyn = true; p.thr = -35; p.range = -4; p.det = DetPeak;
              const BandParams bs[6] = { band (LowCut, 35, 0, .71, 24), d, p, band (Bell, 1000, -3, 2, 12, Mid), band (Bell, 6000, 2, 1, 12, Side), band (BandPass, 2000, 0, 1, 48) };
              for (int i0 = 0; i0 < n; ) { const int m = std::min (bl > 0 ? bl : 1 + (int) (r2() % 1500), n - i0); e.setGlobal (g); for (int k = 0; k < 5; ++k) e.setBand (k, bs[k]); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, m); i0 += m; }
              L.insert (L.end(), R.begin(), R.end()); return L; };
          const auto ref = rend (512); double m = 0; for (int bl : { 1, 7, 64, 333, 4096, -1 }) { const auto o = rend (bl); for (size_t i = 0; i < o.size(); ++i) m = std::max (m, (double) std::abs (o[i] - ref[i])); }
          ok (m == 0, solo < 0 ? "senza solo: scarto massimo %.1e (atteso 0: identico al bit)" : "con solo sulla banda dinamica: scarto massimo %.1e (atteso 0)", m); } }

    std::printf ("== 20. BANDA SPOSTATA E FERMATA: dopo 50 ms il filtro raggiunge la precisione piena, senza click ==\n");
    { const double fs = 44100; const int n = (int) fs * 2;
      // a) precisione: Bell 15 kHz Q 4 +9 dB trascinata da 6 kHz e fermata; guadagno misurato con due toni (15 e 19 kHz) dopo l'assestamento
      { std::vector<float> L ((size_t) n), R ((size_t) n); const double fa = 15000, fb = 19000;
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.05 * std::sin (2 * kPi * fa * i / fs) + 0.05 * std::sin (2 * kPi * fb * i / fs));
        std::vector<float> X (L); Engine e; e.prepare (fs, 4096); Engine::Global g; e.setGlobal (g);
        for (int i0 = 0; i0 < n; i0 += 256) { const double u = std::min (1.0, i0 / (0.5 * fs)); e.setBand (0, band (Bell, 6000 * std::pow (2.5, u), 9, 4)); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (256, n - i0)); }
        auto amp = [&] (const std::vector<float>& x, double f) { double c = 0, s2 = 0; const int a = n - 22050; for (int i = a; i < n; ++i) { c += x[(size_t) i] * std::cos (2 * kPi * f * i / fs); s2 += x[(size_t) i] * std::sin (2 * kPi * f * i / fs); } return std::sqrt (c * c + s2 * s2); };
        SecSet set; sectionsFor (band (Bell, 15000, 9, 4), 9, set); double w = 0;
        for (double f : { fa, fb }) w = std::max (w, std::abs (20 * std::log10 (amp (L, f) / amp (X, f)) - db (anaMag2 (set, f))));
        ok (w < 0.3, "guadagno misurato a 15 e 19 kHz dopo l'assestamento: scarto dall'analogico %.3f dB", w); }
      // b) click: segnale senza contenuto sopra 1,5 kHz, Bell a 4-9 kHz Q 8 trascinata su e giu e fermata 100 ms in cima e in fondo (ogni fermata fa scattare il progetto preciso)
      { std::vector<float> L ((size_t) n), R ((size_t) n); for (int i = 0; i < n; ++i) { const double t = i / fs; L[(size_t) i] = R[(size_t) i] = (float) (0.4 * std::sin (2 * kPi * 110 * t) + 0.2 * std::sin (2 * kPi * 440 * t) + 0.1 * std::sin (2 * kPi * 1320 * t)); }
        double sig = 0; for (int i = 0; i < n; ++i) sig += L[(size_t) i] * (double) L[(size_t) i]; sig /= n;
        Engine e; e.prepare (fs, 4096); Engine::Global g; e.setGlobal (g);
        for (int i0 = 0; i0 < n; i0 += 256) { const double ph = std::fmod (i0 / (0.4 * fs), 1.0); const double u = ph < 0.25 ? ph / 0.25 : ph < 0.5 ? 1.0 : ph < 0.75 ? 1.0 - (ph - 0.5) / 0.25 : 0.0; e.setBand (0, band (Bell, 4000 * std::pow (2.25, u), 9, 8)); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (256, n - i0)); }
        Engine hp; hp.prepare (fs, 4096); Engine::Global gh; hp.setGlobal (gh); hp.setBand (0, band (LowCut, 6000, 0, .71, 96)); hp.process (L.data(), R.data(), nullptr, nullptr, n);
        double pk = 0; for (int i0 = 4410; i0 + 64 <= n; i0 += 64) { double s2 = 0; for (int i = i0; i < i0 + 64; ++i) s2 += L[(size_t) i] * (double) L[(size_t) i]; pk = std::max (pk, s2 / 64); }
        ok (10 * std::log10 (pk / sig + 1e-30) < -90, "energia sopra 6 kHz al massimo %.1f dB rispetto al segnale (< -90 dB)", 10 * std::log10 (pk / sig + 1e-30)); } }

    std::printf ("== 21. DINAMICA ESATTA: guadagno a regime = curva di soglia/knee/range applicata al livello VERO (162 casi; 0.4.3: fino a 4,4 dB di scarto) ==\n");
    { const double fs = 48000; double worst = 0; int cases = 0;
      for (double thr : { -40.0, -24.0, -12.0 }) for (double knee : { 0.0, 6.0, 12.0 }) for (double range : { -1.5, -6.0, 6.0 }) for (double lvl : { -30.0, -20.0, -10.0 }) for (int det : { (int) DetRMS, (int) DetPeak }) {
          const int n = (int) fs * 2; std::vector<float> L ((size_t) n), R ((size_t) n); const double amp = std::pow (10.0, lvl / 20) * std::sqrt (2.0);
          for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * 1000 * i / fs));
          Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); BandParams b = band (Bell, 1000, 0, 1); b.dyn = true; b.thr = thr; b.knee = knee; b.range = range; b.att = 5; b.rel = 80; b.det = det; e.setBand (0, b);
          e.process (L.data(), R.data(), nullptr, nullptr, n);
          double s2 = 0; for (int i = n - 9600; i < n; ++i) s2 += (double) L[(size_t) i] * L[(size_t) i]; const double outDb = 10 * std::log10 (s2 / 9600) - lvl;
          worst = std::max (worst, std::abs (outDb - dynDelta (lvl, b))); ++cases; }
      ok (worst < 0.05, "scarto massimo tra guadagno misurato e atteso: %.4f dB su 162 casi (RMS e Peak)", worst); }

    std::printf ("== 22. CORREZIONE VICINO A NYQUIST: shelf a 14,8-20 kHz (Q 0,5, +-18 dB) e Low Cut a 20 kHz, 44,1 e 48 kHz (0.4.7: fino a 2,94 dB) ==\n");
    { double worst = 0; int nc2 = 0;
      for (double fs : { 44100.0, 48000.0 }) { Grid g; g.init (fs);
        for (double f : { 14811.369, 20000.0 }) for (int t : { (int) LowShelf, (int) HighShelf }) for (double gn : { -18.0, 18.0 }) { BandParams b = band (t, f, gn, 0.5); worst = std::max (worst, maxErr (b, fs, g, -30)); BandCoefs bc; designBand (b, gn, g, bc); nc2 += bc.n == 2; }
        BandParams lc = band (LowCut, 20000, 0, .71, 12); worst = std::max (worst, maxErr (lc, fs, g, -30)); }
      ok (worst < 0.1, "errore massimo dall'analogico (20 Hz-20 kHz, sopra -30 dB) %.3f dB (< 0,1 dB)", worst);
      ok (nc2 >= 13, "progetti con la sezione di correzione: %.0f su 16 (i casi che sbagliavano oltre 1 dB sono 13)", nc2);
      // nel motore: banda ferma (precisa, 2 sezioni) -> trascinata (veloce, 1 sezione + neutra) -> ferma (precisa): uscita finita, invariante al blocco, guadagno giusto a 15 e 19 kHz
      const double fs = 44100; const int n = (int) fs * 2; auto runB = [&] (int bl) { std::vector<float> L ((size_t) n), R ((size_t) n); for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (2 * kPi * 15000 * i / fs) + 0.1 * std::sin (2 * kPi * 19000 * i / fs));
          Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl);
          for (int j0 = 0; j0 < n; j0 += 1024) { const double t = j0 / fs, gn = t < 0.3 ? 12.0 : t < 0.6 ? 12.0 + 6 * (t - 0.3) / 0.3 : 18.0; e.setBand (0, band (HighShelf, 20000, gn, 0.5));   // parametri ogni 1024 campioni in tutti e due i casi
            for (int i0 = j0; i0 < std::min (n, j0 + 1024); i0 += bl) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min ({ bl, n - i0, j0 + 1024 - i0 })); } return L; };
      const auto A = runB (64), B = runB (1024); double mx = 0; bool fin = true; for (int i = 0; i < n; ++i) { fin = fin && std::isfinite (A[(size_t) i]); mx = std::max (mx, (double) std::abs (A[(size_t) i] - B[(size_t) i])); }
      ok (fin && mx == 0, "veloce -> preciso con una sezione in piu: uscita finita e identica al bit con blocchi da 64 e da 1024 (scarto %.1f dBFS)", mx > 0 ? 20 * std::log10 (mx) : -300.0);
      SecSet set; sectionsFor (band (HighShelf, 20000, 18, 0.5), 18, set); double w = 0; const int s0 = n - 16384;
      auto amp = [&] (const std::vector<float>& x, double f) { double re = 0, im = 0; for (int i = s0; i < n; ++i) { const double win = 0.5 - 0.5 * std::cos (2 * kPi * (i - s0) / 16384.0); re += x[(size_t) i] * win * std::cos (2 * kPi * f * i / fs); im += x[(size_t) i] * win * std::sin (2 * kPi * f * i / fs); } return std::hypot (re, im); };
      std::vector<float> X ((size_t) n); for (int i = 0; i < n; ++i) X[(size_t) i] = (float) (0.1 * std::sin (2 * kPi * 15000 * i / fs) + 0.1 * std::sin (2 * kPi * 19000 * i / fs));
      for (double f : { 15000.0, 19000.0 }) w = std::max (w, std::abs (20 * std::log10 (amp (A, f) / amp (X, f)) - db (anaMag2 (set, f))));
      ok (w < 0.1, "nel motore, dopo l'assestamento: guadagno a 15 e 19 kHz scarto dall'analogico %.3f dB (< 0,1 dB)", w); }

    std::printf ("== 23. INGRESSO ASSURDO: un campione 1e30 / 1e10 / 1e5 (0.4.7: fino a 1,6 s di uscita fuori scala) ==\n");
    { const double fs = 48000; int worstBad = 0; double keep = 0;
      for (double big : { 1e30, 1e10, 1e5, 999.0 }) { Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); e.setBand (0, band (Bell, 1000, 4, 1)); e.setBand (1, band (LowCut, 40, 0, .71, 24));
        const int n = (int) fs * 4; std::vector<float> L ((size_t) n), R ((size_t) n); for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.1 * std::sin (2 * kPi * 440 * i / fs)); L[48000] = R[48000] = (float) big;
        for (int i0 = 0; i0 < n; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, 512);
        int bad = 0; for (int i = 0; i < n; ++i) if (! std::isfinite (L[(size_t) i]) || std::abs (L[(size_t) i]) > 4) ++bad;
        if (big > 1000) worstBad = std::max (worstBad, bad); else keep = std::abs (L[48000]); }
      ok (worstBad == 0, "campioni d'uscita non finiti o oltre +12 dBFS: %.0f (atteso 0)", worstBad);
      ok (keep > 500, "un campione da 999 (+60 dBFS, ancora valido) passa: uscita %.0f", keep); }

    std::printf ("== 24. AVVIO DOPO PREPARE: il primo blocco esce gia con OUTPUT/INPUT/bypass/polarita/pan/Character impostati (0.4.8: rampa da 0 dB per 160 ms) ==\n");
    { const double fs = 48000; const char* nm[] = { "OUTPUT -12 dB", "INPUT -12 dB", "OUTPUT -inf", "BYPASS con EQ", "polarita invertita", "pan +50", "Character Warm" };
      for (int c = 0; c < 7; ++c) { auto cfg = [&] (Engine& e) { Engine::Global gl; switch (c) { case 0: gl.outDb = -12; break; case 1: gl.inDb = -12; break; case 2: gl.outDb = -60; break; case 3: gl.bypass = true; break; case 4: gl.invert = true; break; case 5: gl.pan = .5; break; default: gl.character = 2; }
          e.setGlobal (gl); e.setBand (0, band (Bell, 1000, 4, 1)); e.setBand (1, band (LowCut, 40, 0, .71, 24)); };
        const int n = (int) fs / 2; std::vector<float> L ((size_t) n), R ((size_t) n); std::mt19937 rg (8); std::uniform_real_distribution<float> u (-.5f, .5f); for (int i = 0; i < n; ++i) { L[(size_t) i] = u (rg); R[(size_t) i] = u (rg); } auto L2 = L, R2 = R;
        auto pa = std::make_unique<Engine>(), pb = std::make_unique<Engine>(); Engine& a = *pa; Engine& b = *pb;   // nello heap: due motori in piu sullo stack di main lo facevano traboccare
        a.prepare (fs, 512); cfg (a); for (int i0 = 0; i0 < n; i0 += 512) a.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0));
        b.prepare (fs, 512); cfg (b); { std::vector<float> z ((size_t) fs, 0.f), z2 = z; for (int i0 = 0; i0 < (int) fs; i0 += 512) b.process (z.data() + i0, z2.data() + i0, nullptr, nullptr, std::min (512, (int) fs - i0)); }
        cfg (b); for (int i0 = 0; i0 < n; i0 += 512) b.process (L2.data() + i0, R2.data() + i0, nullptr, nullptr, std::min (512, n - i0));
        double mx = 0; for (int i = 0; i < n; ++i) mx = std::max ({ mx, (double) std::abs (L[(size_t) i] - L2[(size_t) i]), (double) std::abs (R[(size_t) i] - R2[(size_t) i]) });
        std::printf ("  %s\n", nm[c]); ok (mx == 0, "motore appena preparato = motore a regime, scarto %.1f dBFS (atteso nessuno)", mx > 0 ? 20 * std::log10 (mx) : -300.0); } }

    std::printf ("== 25. MOVIMENTO: salti e spazzate di tagli e passa-banda senza picchi (0.4.9: High Cut 48 dB 20 kHz -> 200 Hz a 192 kHz +11,5 dB su un tono 100 Hz) ==\n");
    { auto mov = [&] (double fs, BandParams a, double fB, double tf, double T) {   // picco del tono oltre il massimo del filtro lungo il percorso (dB)
          auto pe = std::make_unique<Engine>(); Engine& e = *pe; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl);
          const int n = (int) (fs * (1.2 + T)), j = (int) fs; std::vector<float> L ((size_t) n), R ((size_t) n); for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (2 * kPi * tf * i / fs));
          for (int i = 0; i < n; i += 64) { BandParams b = a; if (i >= j) b.f = a.f * std::pow (fB / a.f, T > 0 ? std::min (1.0, (i - j) / (T * fs)) : 1.0); e.setBand (0, b); e.process (L.data() + i, R.data() + i, nullptr, nullptr, std::min (64, n - i)); }
          double pk = 0; for (int i = j; i < n; ++i) pk = std::max (pk, (double) std::abs (L[(size_t) i]));
          Grid g; g.init (fs); double m = 0; for (int k = 0; k <= 200; ++k) { BandParams b = a; b.f = a.f * std::pow (fB / a.f, k / 200.0); BandCoefs c; designBand (b, b.gain, g, c, true); m = std::max (m, bandMag2 (c, tf, fs)); }
          return 20 * std::log10 (pk / (0.25 * std::sqrt (m))); };
      double w = -99; const double cases[][5] = { { 192000, HighCut, 20000, 200, 100 }, { 48000, HighCut, 20000, 200, 100 }, { 48000, LowCut, 30, 300, 2000 }, { 48000, BandPass, 100, 10000, 1000 }, { 48000, HighShelf, 1000, 15000, 6000 } };
      for (const auto& c : cases) for (double T : { 0.0, 0.02, 0.1 }) { BandParams a = band ((int) c[1], c[2], c[1] == HighShelf ? 12 : 0, c[1] == BandPass ? 10 : c[1] == HighShelf ? 2 : .71, c[1] == HighCut || c[1] == LowCut ? 48 : 12); w = std::max (w, mov (c[0], a, c[3], c[4], T)); }
      ok (w < 0.5, "salti e spazzate (0, 20, 100 ms) di High/Low Cut 48 dB, Band Pass Q10, High Shelf: picco oltre il percorso %.2f dB (< 0,5 dB)", w); }

    std::printf ("== 26. MOVIMENTO, revisione esterna: sezioni del primo ordine senza click, salti grandi in dissolvenza ==\n");
    { auto run1 = [&] (BandParams a, BandParams b, double tf, double amp, std::vector<float>& L) { auto pe = std::make_unique<Engine>(); Engine& e = *pe; e.prepare (48000, 512); Engine::Global gl; e.setGlobal (gl);
          const int n = 72000; L.assign ((size_t) n, 0.f); std::vector<float> R ((size_t) n); for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * tf * i / 48000));
          for (int i = 0; i < n; i += 64) { e.setBand (0, i >= 48000 ? b : a); e.process (L.data() + i, R.data() + i, nullptr, nullptr, 64); } };
      double w = -300; for (int t : { LowCut, LowShelf, HighShelf, TiltShelf }) { BandParams a = band (t, 100, hasGain (t) ? 6 : 0, .71, 6), b = a; b.f = 101; std::vector<float> A, B; run1 (a, a, 1000, 0.25, A); run1 (a, b, 1000, 0.25, B);
          double da = 0, db2 = 0; for (int i = 47000; i < 49440; ++i) { da = std::max (da, (double) std::abs (A[(size_t) i] - 2 * A[(size_t) i - 1] + A[(size_t) i - 2])); db2 = std::max (db2, (double) std::abs (B[(size_t) i] - 2 * B[(size_t) i - 1] + B[(size_t) i - 2])); }
          w = std::max (w, 20 * std::log10 (db2 / da)); }   // click = gradino: la differenza seconda sale (tono 1 kHz: 0,0043 di regime)
      ok (w < 0.5, "pendenza 6 dB/oct, frequenza 100 -> 101 Hz: differenza seconda dell'uscita %.2f dB oltre quella del filtro fermo (< 0,5; b2eaa83: +15 dB, click)", w);
      std::vector<float> A, B, C; run1 (band (LowShelf, 100, 30, 40), band (LowShelf, 100, 30, 40), 300, 0.25, A); run1 (band (LowShelf, 5000, -30, 40), band (LowShelf, 5000, -30, 40), 300, 0.25, B); run1 (band (LowShelf, 100, 30, 40), band (LowShelf, 5000, -30, 40), 300, 0.25, C);
      double pa = 0, pb = 0, pc = 0; for (int i = 40000; i < 48000; ++i) pa = std::max (pa, (double) std::abs (A[(size_t) i])); for (int i = 60000; i < 72000; ++i) pb = std::max (pb, (double) std::abs (B[(size_t) i])); for (int i = 48000; i < 49440; ++i) pc = std::max (pc, (double) std::abs (C[(size_t) i]));
      ok (20 * std::log10 (pc / std::max (pa, pb)) < 0.5, "Low Shelf Q40 +30 dB 100 Hz -> -30 dB 5 kHz, tono 300 Hz: picco %.2f dB oltre i due regimi (< 0,5; b2eaa83: +38 dB)", 20 * std::log10 (pc / std::max (pa, pb))); }

    std::printf (fails ? "\nRISULTATO: %d FAIL\n" : "\nRISULTATO: tutti i test PASS\n", fails);
    return fails ? 1 : 0;
}
