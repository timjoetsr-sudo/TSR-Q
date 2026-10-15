// TSR Q · DSP QUALITY PASS · harness offline (motore C++ diretto, nessuna GUI). Stampa CSV: sezione,caso,metrica,valore,unita
// target CMake: tsrq_quality_csv (stampa CSV su stdout; argomento opzionale: alias|thdn|null|lat|zip|cpu|dyn|avvio|rob)
#include "../Source/TsrqDsp.h"
#include <cstdio>
#include <complex>
#include <random>
#include <chrono>
#include <algorithm>
using namespace tsrq;
using V = std::vector<float>;
static BandParams mk (int t, double f, double g, double q, int slope = 12) { BandParams b; b.used = true; b.type = t; b.f = f; b.gain = g; b.q = q; b.slope = slope; return b; }
static double db (double x) { return 20 * std::log10 (std::max (x, 1e-20)); }
static void csv (const char* s, const std::string& c, const char* m, double v, const char* u) { std::printf ("%s,%s,%s,%.3f,%s\n", s, c.c_str(), m, v, u); }
static void run (Engine& e, V& L, V& R, int bl) { for (size_t i = 0; i < L.size(); i += bl) { int n = (int) std::min<size_t> (bl, L.size() - i); e.process (L.data() + i, R.data() + i, nullptr, nullptr, n); } }
// FFT radix 2 (iterativa)
static void fft (std::vector<std::complex<double>>& a) { const size_t n = a.size(); for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap (a[i], a[j]); }
  for (size_t len = 2; len <= n; len <<= 1) { const std::complex<double> w = std::polar (1.0, -2 * kPi / len); for (size_t i = 0; i < n; i += len) { std::complex<double> wn (1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * wn; a[i + k] = u + v; a[i + k + len / 2] = u - v; wn *= w; } } } }
// spettro di ampiezza in dBFS (picco di sinusoide = 0 dBFS a ampiezza 1), finestra Blackman-Harris 4 termini, N = 65536
static std::vector<double> spec (const float* x, int N) { std::vector<std::complex<double>> a (N); double ws = 0;
  for (int i = 0; i < N; ++i) { const double t = 2 * kPi * i / (N - 1), w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t); ws += w; a[i] = x[i] * w; }
  fft (a); std::vector<double> m (N / 2); for (int k = 0; k < N / 2; ++k) m[k] = db (2 * std::abs (a[k]) / ws); return m; }
static void sine (V& L, V& R, double f, double amp, double fs) { for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = (float) (amp * std::sin (2 * kPi * f * i / fs)); }
static double thdn (const float* x, int n, double f, double fs) { double a = 0, b = 0, ss = 0, cc = 0, sc = 0; for (int i = 0; i < n; ++i) { const double s = std::sin (2 * kPi * f * i / fs), c = std::cos (2 * kPi * f * i / fs); a += x[i] * s; b += x[i] * c; ss += s * s; cc += c * c; sc += s * c; }
  const double det = ss * cc - sc * sc, A = (a * cc - b * sc) / det, B = (b * ss - a * sc) / det; double er = 0, ef = 0;
  for (int i = 0; i < n; ++i) { const double fi = A * std::sin (2 * kPi * f * i / fs) + B * std::cos (2 * kPi * f * i / fs); ef += fi * fi; er += (x[i] - fi) * (x[i] - fi); } return 10 * std::log10 (er / ef + 1e-30); }
static double tone (const float* x, int n, double f, double fs) { double re = 0, im = 0; for (int i = 0; i < n; ++i) { re += x[i] * std::cos (2 * kPi * f * i / fs); im += x[i] * std::sin (2 * kPi * f * i / fs); } return 2 * std::sqrt (re * re + im * im) / n; }
static void setEq (Engine& e) { e.setBand (0, mk (LowCut, 30, 0, .71, 24)); e.setBand (1, mk (Bell, 250, -3, 1)); e.setBand (2, mk (Bell, 3000, 2, 1)); e.setBand (3, mk (HighShelf, 10000, 2.5, .71));
  e.setBand (4, mk (Bell, 80, 2, .8)); e.setBand (5, mk (Bell, 1200, -1.5, 2)); e.setBand (6, mk (LowShelf, 120, 1, .71)); e.setBand (7, mk (HighCut, 18000, 0, .71, 12)); }
int main (int argc, char** argv) {
  const std::string only = argc > 1 ? argv[1] : "";
  auto want = [&] (const char* s) { return only.empty() || only == s; };
  std::printf ("sezione,caso,metrica,valore,unita\n");
  const double fs = 48000;
  // ---------- 1. Aliasing: sinusoide a 0 dBFS (picco 1.0), 20 Hz-20 kHz, picco spettrale peggiore NON armonico (prodotti ripiegati), dBFS
  if (want ("alias")) for (int ch : { 0, 1, 2 }) { double worst = -300, wf = 0, wfund = 0;
    for (int k = 0; k <= 30; ++k) { double f = 20 * std::pow (1000.0, k / 30.0); const int N = 65536; f = std::round (f * N / fs) * fs / N;   // su un bin esatto
      Engine e; e.prepare (fs, 512); Engine::Global gl; gl.character = ch; e.setGlobal (gl); V L (N + 24000), R (L.size()); sine (L, R, f, 1.0, fs); run (e, L, R, 512);
      auto m = spec (L.data() + 24000, N); const double bw = fs / N;
      for (int b = 2; b < N / 2; ++b) { const double fb = b * bw; bool harm = false; for (int h = 1; h * f < fs / 2 + 1; ++h) if (std::abs (fb - h * f) <= 8 * bw) { harm = true; break; } if (fb < 10) harm = true; if (! harm && m[b] > worst) { worst = m[b]; wf = fb; wfund = f; } } }
    const char* nm[] = { "Clean", "Subtle", "Warm" }; csv ("aliasing", nm[ch], "picco_non_armonico_peggiore", worst, "dBFS"); csv ("aliasing", nm[ch], "a_frequenza", wf, "Hz"); csv ("aliasing", nm[ch], "con_fondamentale", wfund, "Hz"); }
  // ---------- 2. THD+N in impostazione neutra (nessuna banda, Clean) e con EQ tipica: sinusoide 997 Hz -6 dBFS
  if (want ("thdn")) for (int cfg : { 0, 1 }) for (double f : { 100.0, 997.0, 9973.0 }) { Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); if (cfg) setEq (e);
    V L ((size_t) fs * 2), R (L.size()); sine (L, R, f, 0.5, fs); run (e, L, R, 512); csv ("thdn", std::string (cfg ? "eq_tipica_" : "neutro_") + std::to_string ((int) f) + "Hz", "thd+n", thdn (L.data() + (int) fs, (int) fs, f, fs), "dB"); }
  // ---------- 3. Null: bypass globale e impostazione neutra, rumore bianco -6 dBFS + transitori; con e senza bande attive
  if (want ("null")) for (int cfg : { 0, 1, 2, 3 }) { Engine e; e.prepare (fs, 512); Engine::Global gl; gl.bypass = cfg >= 2; if (cfg == 3) gl.character = 2; e.setGlobal (gl); if (cfg == 1 || cfg == 3) setEq (e);
    V L ((size_t) fs * 2), R (L.size()); std::mt19937 rg (7); std::uniform_real_distribution<float> u (-.5f, .5f); for (size_t i = 0; i < L.size(); ++i) { L[i] = u (rg); R[i] = u (rg); } V I = L, J = R; run (e, L, R, 512);
    double mx = 0; for (size_t i = 0; i < L.size(); ++i) mx = std::max ({ mx, (double) std::abs (L[i] - I[i]), (double) std::abs (R[i] - J[i]) });
    const char* nm[] = { "neutro_nessuna_banda", "eq_attiva(solo_riferimento)", "bypass_globale", "bypass_con_eq_e_warm" }; csv ("null", nm[cfg], "residuo_max", mx == 0 ? -999 : db (mx), "dBFS"); }
  // ---------- 4. Latenza: impulso, posizione del picco e ritardo di gruppo a 1 kHz (riportata all'host: 0)
  if (want ("lat")) for (int ch : { 0, 1, 2 }) { Engine e; e.prepare (fs, 512); Engine::Global gl; gl.character = ch; e.setGlobal (gl); const int N = 8192; V L (N, 0.f), R (N, 0.f); L[100] = R[100] = 0.5f; run (e, L, R, 512);
    int pk = 0; for (int i = 0; i < N; ++i) if (std::abs (L[i]) > std::abs (L[pk])) pk = i;
    // ritardo di gruppo a 1 kHz da fase (differenza finita)
    auto ph = [&] (double f) { std::complex<double> h (0); for (int i = 0; i < N; ++i) h += (double) L[i] * std::polar (1.0, -2 * kPi * f * (i - 100) / fs); return std::arg (h); };
    double gd = 0; { const double f1 = 990, f2 = 1010; double d = ph (f2) - ph (f1); while (d > kPi) d -= 2 * kPi; while (d < -kPi) d += 2 * kPi; gd = -d / (2 * kPi * (f2 - f1)) * fs; }
    const char* nm[] = { "Clean", "Subtle", "Warm" }; csv ("latenza", nm[ch], "picco_impulso", pk - 100, "campioni"); csv ("latenza", nm[ch], "ritardo_gruppo_1kHz", gd, "campioni"); csv ("latenza", nm[ch], "riportata_host", 0, "campioni(LETTO_NEL_CODICE)"); }
  // ---------- 5. Zipper: automazione rapida (triangolo 4 Hz) mossa a ogni blocco su sinusoide 1 kHz -10 dBFS; picco spettrale > 2 kHz (fuori segnale e bande laterali), dBFS
  if (want ("zip")) { const char* pn[] = { "bell_gain_pm12", "bell_freq_500-2k", "bell_q_0.3-8", "lowcut_freq_20-200", "highshelf_gain_pm12", "out_gain_pm12", "in_gain_pm12", "pan_pm100", "scale_0-200", "nessuna(riferimento)" };
    for (int p = 0; p < 10; ++p) for (int bl : { 64, 512 }) { Engine e; e.prepare (fs, 512); const int N = 65536, n = N + 48000; V L (n), R (n); sine (L, R, 1000, 0.316, fs);
      for (int i = 0; i < n; i += bl) { const double ph = std::fmod (4.0 * i / fs, 1.0), t = ph < .5 ? 4 * ph - 1 : 3 - 4 * ph;   // -1..1
        Engine::Global gl; BandParams b = mk (Bell, 1000, 6, 1); BandParams b2 = mk (HighShelf, 6000, 0, .71); bool b2on = false;
        switch (p) { case 0: b.gain = 12 * t; break; case 1: b.f = 1000 * std::pow (2.0, 2 * t); break; case 2: b.q = std::exp (std::log (0.3) + (t + 1) / 2 * std::log (8 / 0.3)); break;
          case 3: b = mk (LowCut, 20 * std::pow (10.0, (t + 1) / 2), 0, .71, 24); break; case 4: b2.gain = 12 * t; b2on = true; break; case 5: gl.outDb = 12 * t - 6; break; case 6: gl.inDb = 12 * t - 6; break;
          case 7: gl.pan = t; break; case 8: gl.scale = 1 + t; break; default: break; }
        e.setGlobal (gl); e.setBand (0, b); if (b2on) e.setBand (1, b2); e.process (L.data() + i, R.data() + i, nullptr, nullptr, std::min (bl, n - i)); }
      double w = -300; for (const V* c : { &L, &R }) { auto m = spec (c->data() + 48000, N); const double bw = fs / N; for (int k = (int) (2000 / bw); k < (int) (20000 / bw); ++k) { const double fb = k * bw; bool h = false; for (int j = 2; j < 20; ++j) if (std::abs (fb - j * 1000) < 8 * bw) h = true; if (! h) w = std::max (w, m[k]); } }
      csv ("zipper", std::string (pn[p]) + "_blocco" + std::to_string (bl), "picco_2-20kHz", w, "dBFS"); } }
  // ---------- 6. CPU: µs per blocco da 512 (media e picco = max dei blocchi, mediana di 5 ripetizioni) a 48 e 96 kHz; EQ tipica 8 bande, 8 bande + 2 dinamiche, 8 bande con automazione a ogni blocco
  if (want ("cpu")) for (double f2 : { 48000.0, 96000.0 }) for (int cfg : { 0, 1, 2, 3, 4 }) { std::vector<double> mean, p99;
    for (int rep = 0; rep < 9; ++rep) { Engine e; e.prepare (f2, 512); Engine::Global gl; if (cfg == 3) gl.character = 2; e.setGlobal (gl); setEq (e);
      if (cfg == 1) { auto b = mk (Bell, 200, 0, 1); b.dyn = true; b.thr = -30; b.range = -6; e.setBand (8, b); b.f = 4000; e.setBand (9, b); }
      const int nb = (int) (f2 * 4 / 512); V L (512), R (512); std::mt19937 rg (3); std::uniform_real_distribution<float> u (-.3f, .3f); std::vector<double> t;
      for (int k = 0; k < nb; ++k) { for (int i = 0; i < 512; ++i) { L[i] = u (rg); R[i] = u (rg); } if (cfg == 2) e.setBand (2, mk (Bell, 3000, 6 * std::sin (k * 0.05), 1));
        if (cfg == 4 && k % 50 == 0) { for (int j = 0; j < 8; ++j) e.setBand (j, mk (j % 2 ? Bell : HighShelf, 100 * (1 + j + (k / 50) % 3), (k / 50) % 2 ? 3 : -3, 1 + j * .2)); }   // cambio di preset: 8 bande nuove
        const auto t0 = std::chrono::steady_clock::now(); e.process (L.data(), R.data(), nullptr, nullptr, 512); const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count(); if (k >= 20) t.push_back (us); }
      double s = 0; for (double x : t) s += x; mean.push_back (s / t.size()); std::sort (t.begin(), t.end()); p99.push_back (cfg == 4 ? t.back() : t[(size_t) (t.size() * 0.99)]); }
    std::sort (mean.begin(), mean.end()); std::sort (p99.begin(), p99.end()); const char* nm[] = { "eq8", "eq8+2dyn", "eq8_automazione", "eq8+warm", "eq8_cambio_preset" };
    const std::string c = std::string (nm[cfg]) + "_" + std::to_string ((int) (f2 / 1000)) + "k"; csv ("cpu", c, "media_blocco512_min9", mean[0], "us"); csv ("cpu", c, cfg == 4 ? "peggiore_blocco512_mediana9" : "p99_blocco512_mediana9", p99[4], "us"); }
  // ---------- 7. Dinamica: curva statica (Bell dinamico 1 kHz, soglia -24, range -12, knee 6, RMS) e risposta a gradino (-40 -> -10 dBFS)
  if (want ("dyn")) { for (int lv = -60; lv <= 0; lv += 3) { Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); auto b = mk (Bell, 1000, 0, 1); b.dyn = true; b.thr = -24; b.range = -12; b.knee = 6; e.setBand (0, b);
      V L ((size_t) fs * 2), R (L.size()); const double a = std::pow (10.0, lv / 20.0); sine (L, R, 1000, a, fs); run (e, L, R, 512); csv ("dinamica_statica", "in_" + std::to_string (lv) + "dBFS", "uscita", db (tone (L.data() + (int) fs, (int) fs, 1000, fs)), "dBFS"); }
    for (double att : { 1.0, 10.0 }) { Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); auto b = mk (Bell, 1000, 0, 1); b.dyn = true; b.thr = -24; b.range = -12; b.knee = 0; b.att = att; b.rel = 100; e.setBand (0, b);
      const int n = (int) fs * 2; V L (n), R (n); for (int i = 0; i < n; ++i) L[i] = R[i] = (float) ((i < fs ? 0.01 : 0.316) * std::sin (2 * kPi * 1000 * i / fs)); run (e, L, R, 512);
      // guadagno per periodo (48 campioni) dopo il gradino
      double g0 = 0, gEnd = db (tone (L.data() + n - 4800, 4800, 1000, fs) / 0.316); double t63 = -1;
      for (int k = 0; k < 400; ++k) { const int s0 = (int) fs + k * 48; const double g = db (tone (L.data() + s0, 48, 1000, fs) / 0.316); if (k == 0) g0 = g; if (t63 < 0 && (g - g0) <= 0.632 * (gEnd - g0)) t63 = k * 1.0; if (k < 60 && k % 4 == 0) csv ("dinamica_gradino", "att" + std::to_string ((int) att) + "ms_t" + std::to_string (k) + "ms", "guadagno", g, "dB"); }
      csv ("dinamica_gradino", "att" + std::to_string ((int) att) + "ms", "tempo_63pc", t63, "ms"); csv ("dinamica_gradino", "att" + std::to_string ((int) att) + "ms", "guadagno_finale", gEnd, "dB"); } }
  // ---------- 7b. Avvio dopo prepare: motore appena preparato vs stesso motore già a regime (1 s di silenzio con gli stessi parametri), stesso segnale
  if (want ("avvio")) { const char* nm[] = { "out_-12dB", "in_-12dB", "out_-inf(mute)", "bypass_con_eq", "polarita_invertita", "pan_+50", "character_warm", "eq_sola(riferimento)" };
    for (int c = 0; c < 8; ++c) { auto cfg = [&] (Engine& e) { Engine::Global gl; switch (c) { case 0: gl.outDb = -12; break; case 1: gl.inDb = -12; break; case 2: gl.outDb = -60; break; case 3: gl.bypass = true; break; case 4: gl.invert = true; break; case 5: gl.pan = .5; break; case 6: gl.character = 2; break; } e.setGlobal (gl); setEq (e); };
      const int n = (int) fs; V L (n), R (n); std::mt19937 rg (8); std::uniform_real_distribution<float> u (-.5f, .5f); for (int i = 0; i < n; ++i) { L[i] = u (rg); R[i] = u (rg); } V L2 = L, R2 = R;
      Engine a; a.prepare (fs, 512); cfg (a); run (a, L, R, 512);
      Engine b; b.prepare (fs, 512); cfg (b); { V z ((size_t) fs, 0.f), z2 = z; run (b, z, z2, 512); cfg (b); } run (b, L2, R2, 512);
      double mx = 0; int last = -1; for (int i = 0; i < n; ++i) { const double d = std::max (std::abs (L[i] - L2[i]), std::abs (R[i] - R2[i])); if (d > mx) mx = d; if (d > 1e-6) last = i; }
      csv ("avvio", nm[c], "scarto_max_vs_regime", mx == 0 ? -999 : db (mx), "dBFS"); csv ("avvio", nm[c], "durata_scarto>-120dBFS", last < 0 ? 0 : (last + 1) * 1000.0 / fs, "ms"); } }
  // ---------- 8. Robustezza
  if (want ("rob")) {
    // 8a sample rate: guadagno Bell 1 kHz +6 a 1 kHz, attacco al 63% del Bell dinamico (ms)
    for (double f2 : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }) { const std::string c = std::to_string ((int) f2);
      { Engine e; e.prepare (f2, 512); Engine::Global gl; e.setGlobal (gl); e.setBand (0, mk (Bell, 1000, 6, 1)); V L ((size_t) f2), R (L.size()); sine (L, R, 1000, .1, f2); run (e, L, R, 512); csv ("sample_rate", c, "bell1k+6_a_1kHz", db (tone (L.data() + (int) f2 / 2, (int) f2 / 2, 1000, f2) / .1), "dB"); }
      { Engine e; e.prepare (f2, 512); Engine::Global gl; e.setGlobal (gl); auto b = mk (Bell, 1000, 0, 1); b.dyn = true; b.thr = -24; b.range = -12; b.knee = 0; b.att = 10; b.rel = 100; e.setBand (0, b);
        const int n = (int) f2 * 2; V L (n), R (n); for (int i = 0; i < n; ++i) L[i] = R[i] = (float) ((i < f2 ? 0.01 : 0.316) * std::sin (2 * kPi * 1000 * i / f2)); run (e, L, R, 512);
        const int P = (int) (f2 / 1000); const double gEnd = db (tone (L.data() + n - 100 * P, 100 * P, 1000, f2) / 0.316), g0 = db (tone (L.data() + (int) f2, P, 1000, f2) / 0.316); double t63 = -1;
        for (int k = 0; k < 400 && t63 < 0; ++k) { const double g = db (tone (L.data() + (int) f2 + k * P, P, 1000, f2) / 0.316); if ((g - g0) <= 0.632 * (gEnd - g0)) t63 = k; } csv ("sample_rate", c, "attacco10ms_t63", t63, "ms"); } }
    // 8b dimensioni del blocco: EQ + 2 dinamiche + Warm, rumore; parametri cambiati ogni 1024 campioni; confronto con blocco 512
    auto render = [&] (int mode) { Engine e; e.prepare (fs, 4096); const int n = (int) fs * 2; V L (n), R (n); std::mt19937 rg (5); std::uniform_real_distribution<float> u (-.4f, .4f); for (int i = 0; i < n; ++i) { L[i] = u (rg); R[i] = u (rg); }
      std::mt19937 rb (9); std::uniform_int_distribution<int> ub (1, 1500); const int fixed[] = { 1, 7, 64, 512, 4096 }; int i = 0, nextP = 0;
      while (i < n) { if (i >= nextP) { Engine::Global gl; gl.character = 2; e.setGlobal (gl); setEq (e); auto b = mk (Bell, 200, 0, 1); b.dyn = true; b.thr = -30; b.range = -6; e.setBand (8, b); b.f = 3000 + 1000 * std::sin (nextP * 1e-3); e.setBand (9, b);
          e.setBand (2, mk (Bell, 3000, 6 * std::sin (nextP * 3e-4), 1)); nextP += 1024; }
        int bl = mode < 5 ? fixed[mode] : ub (rb); bl = std::min ({ bl, n - i, nextP - i }); e.process (L.data() + i, R.data() + i, nullptr, nullptr, bl); i += bl; } return L; };
    const V ref = render (3); const char* bn[] = { "1", "7", "64", "512", "4096", "variabile_1-1500" };
    for (int m = 0; m < 6; ++m) { const V o = render (m); double mx = 0; for (size_t i = 0; i < o.size(); ++i) mx = std::max (mx, (double) std::abs (o[i] - ref[i])); csv ("blocchi", bn[m], "scarto_max_vs_512", mx == 0 ? -999 : db (mx), "dBFS"); }
    // 8c ingressi estremi: silenzio, DC 1.0, full scale, +12 dBFS, rumore +12 dBFS con 8 bande + Warm
    for (int s = 0; s < 5; ++s) { Engine e; e.prepare (fs, 512); Engine::Global gl; gl.character = 2; e.setGlobal (gl); setEq (e); const int n = (int) fs * 2; V L (n), R (n); std::mt19937 rg (11); std::uniform_real_distribution<float> u (-4.f, 4.f);
      for (int i = 0; i < n; ++i) { float v = 0; switch (s) { case 1: v = 1; break; case 2: v = (float) std::sin (2 * kPi * 1000 * i / fs); break; case 3: v = (float) (3.98 * std::sin (2 * kPi * 1000 * i / fs)); break; case 4: v = u (rg); break; } L[i] = R[i] = v; }
      run (e, L, R, 512); int bad = 0; double mx = 0; for (int i = 0; i < n; ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ++bad; else mx = std::max (mx, (double) std::abs (L[i])); }
      const char* nm[] = { "silenzio", "DC_1.0", "sin_0dBFS", "sin_+12dBFS", "rumore_+12dBFS" }; csv ("estremi", nm[s], "campioni_NaN_Inf", bad, "n"); csv ("estremi", nm[s], "picco_uscita", db (mx), "dBFS"); }
    // 8d prepare/reset ripetuti: render, prepare (stesso fs) e render di nuovo = motore nuovo?
    { auto go = [&] (Engine& e) { Engine::Global gl; gl.character = 2; e.setGlobal (gl); setEq (e); auto b = mk (Bell, 200, 0, 1); b.dyn = true; b.thr = -30; e.setBand (8, b); V L ((size_t) fs), R (L.size()); std::mt19937 rg (4); std::uniform_real_distribution<float> u (-.5f, .5f); for (auto& v : L) v = u (rg); R = L; run (e, L, R, 512); return L; };
      Engine a; a.prepare (fs, 512); const V r1 = go (a); a.prepare (96000, 256); go (a); a.prepare (fs, 512); const V r2 = go (a); a.prepare (fs, 512); a.prepare (fs, 512); const V r3 = go (a);
      double m2 = 0, m3 = 0; for (size_t i = 0; i < r1.size(); ++i) { m2 = std::max (m2, (double) std::abs (r1[i] - r2[i])); m3 = std::max (m3, (double) std::abs (r1[i] - r3[i])); }
      csv ("prepare_ripetuto", "dopo_96k_poi_48k", "scarto_max_vs_nuovo", m2 == 0 ? -999 : db (m2), "dBFS"); csv ("prepare_ripetuto", "prepare_x2", "scarto_max_vs_nuovo", m3 == 0 ? -999 : db (m3), "dBFS"); }
    // 8e sidechain assente con banda in sidechain esterno: niente NaN, comportamento = detector interno?
    { Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); auto b = mk (Bell, 1000, 0, 1); b.dyn = true; b.sc = true; b.thr = -30; b.range = -6; e.setBand (0, b); V L ((size_t) fs), R (L.size()); sine (L, R, 1000, .3, fs); run (e, L, R, 512);
      int bad = 0; for (float v : L) if (! std::isfinite (v)) ++bad; csv ("sidechain", "sc_attivo_bus_assente", "campioni_NaN_Inf", bad, "n"); csv ("sidechain", "sc_attivo_bus_assente", "guadagno_1kHz", db (tone (L.data() + (int) fs / 2, (int) fs / 2, 1000, fs) / .3), "dB"); }
  }
  return 0;
}
