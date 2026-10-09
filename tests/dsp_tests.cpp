// TSR Q · test del core DSP (stessa batteria del prototipo, + All Pass e 32 bande)
#include "../Source/TsrqDsp.h"
#include <cstdio>
#include <random>
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

    std::printf ("== 1b. Band Pass 12/24/36/48 dB/oct: precisione nella banda utile (sopra -20 dB) ==\n");
    { double m = 0, mAll = 0; for (double fs : { 44100.0, 48000.0, 96000.0 }) { Grid g; g.init (fs);
        for (double f : { 30.0, 250.0, 1000.0, 4000.0, 8000.0 }) for (double q : { 0.5, 1.0, 4.0 }) for (int sl : { 12, 24, 36, 48 }) {
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

    std::printf (fails ? "\nRISULTATO: %d FAIL\n" : "\nRISULTATO: tutti i test PASS\n", fails);
    return fails ? 1 : 0;
}
