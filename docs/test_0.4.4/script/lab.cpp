// TSR Q · laboratorio di misura (stesso programma compilato contro il motore 0.4.3 e contro il motore nuovo)
// uso: lab <sezione> [args]   sezioni: precision, recip, stability, designcost, cpu, block, nan, denormal, extreme, fuzz, dynacc
#include "TsrqDsp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <functional>
using namespace tsrq;
using Clock = std::chrono::steady_clock;

static std::vector<float> CL, CR;   // clip ESO - SAFE, 48 kHz
static void loadClip (const char* path) {
    std::ifstream f (path, std::ios::binary); if (! f) { std::fprintf (stderr, "ERRORE: clip non trovata: %s\n", path); std::exit (2); }
    f.seekg (0, std::ios::end); const size_t n = (size_t) f.tellg() / 8; f.seekg (0); std::vector<float> il (n * 2); f.read ((char*) il.data(), (std::streamsize) (n * 8));
    CL.resize (n); CR.resize (n); for (size_t i = 0; i < n; ++i) { CL[i] = il[2 * i]; CR[i] = il[2 * i + 1]; }
}
static const char* TN[] = { "Bell", "LowShelf", "HighShelf", "LowCut", "HighCut", "Notch", "BandPass", "TiltShelf", "FlatTilt", "AllPass" };
static BandParams mk (int t, double f, double g, double q, int slope = 12, int place = Stereo) { BandParams b; b.used = true; b.type = t; b.f = f; b.gain = g; b.q = q; b.slope = slope; b.place = place; return b; }

// ---------------- 1. PRECISIONE: errore massimo |dB| vs prototipo analogico, 20 Hz - 20 kHz, punti con analogico > -30 dB ----------------
// valutazione INDIPENDENTE dal motore: H(e^jw) in aritmetica complessa long double (nessuna cancellazione)
#include <complex>
static long double hMag2 (const BandCoefs& bc, double fr, double fs) {
    typedef std::complex<long double> C; const long double w = 2.0L * 3.14159265358979323846264338327950288L * (long double) fr / (long double) fs;
    const C z1 = std::polar (1.0L, -w), z2 = std::polar (1.0L, -2.0L * w); long double m = 1;
    for (int k = 0; k < bc.n; ++k) { const Coef& c = bc.c[k]; const C nu = (long double) c.b0 + (long double) c.b1 * z1 + (long double) c.b2 * z2, de = 1.0L + (long double) c.a1 * z1 + (long double) c.a2 * z2; m *= std::norm (nu) / std::norm (de); }
    return m;
}
static double precErr (const BandParams& b, const Grid& g, double* worstF) {
    BandParams bb = b; bb.f = std::min (b.f, g.fs * 0.495);
    const double gain = hasGain (b.type) ? b.gain : 0;
    BandCoefs bc; designBand (bb, gain, g, bc); SecSet set; sectionsFor (bb, gain, set);
    const double top = std::min (20000.0, g.fs / 2 * 0.999); double m = 0; *worstF = 0;
    for (int i = 0; i < 512; ++i) {
        const double fr = 20.0 * std::pow (top / 20.0, i / 511.0), a = 10 * std::log10 (std::max (anaMag2 (set, fr), 1e-30));
        if (a < -30) continue;
        const double d = 10 * std::log10 (std::max ((double) hMag2 (bc, fr, g.fs), 1e-30)), e = std::abs (d - a);
        if (! (e < 1e9)) { *worstF = fr; return 1e9; }
        if (e > m) { m = e; *worstF = fr; }
    }
    return m;
}
static void secPrecision (const char* out) {
    std::FILE* o = std::fopen (out, "w"); std::fprintf (o, "fs,type,f,g,q,slope,err,worstF\n");
    const double FS[] = { 44100, 48000, 88200, 96000, 192000 };
    std::vector<double> F; for (int i = 0; i < 24; ++i) F.push_back (20.0 * std::pow (1000.0, i / 23.0));
    const double GB[] = { -18, -9, -3, 3, 9, 18 };
    for (double fs : FS) { Grid g; g.init (fs);
        auto rec = [&] (const BandParams& b) { double wf; const double e = precErr (b, g, &wf); std::fprintf (o, "%.0f,%s,%.3f,%.1f,%.3f,%d,%.6f,%.1f\n", fs, TN[b.type], b.f, b.gain, b.q, b.slope, e, wf); };
        for (double f : F) {
            for (double gn : GB) for (double q : { 0.3, 0.7, 2.0, 6.0, 20.0 }) rec (mk (Bell, f, gn, q));
            for (int t : { LowShelf, HighShelf }) for (int sl : { 6, 12 }) for (double gn : GB) for (double q : { 0.5, 0.71, 1.5 }) rec (mk (t, f, gn, q, sl));
            for (double gn : GB) rec (mk (TiltShelf, f, gn, 0.71));
            for (int t : { LowCut, HighCut }) for (int sl : { 6, 12, 18, 24, 36, 48, 72, 96 }) rec (mk (t, f, 0, 0.71, sl));
            for (double q : { 0.5, 2.0, 8.0, 30.0 }) rec (mk (Notch, f, 0, q));
            for (int sl : { 6, 12, 24, 48, 96 }) for (double q : { 0.5, 2.0, 8.0 }) rec (mk (BandPass, f, 0, q, sl));
            for (double gn : GB) rec (mk (FlatTilt, f, gn, 1));
        }
    }
    std::fclose (o);
}
// ---------------- 1x. PRECISIONE estesa: shelf con Q alto e guadagni fino a +/-30 dB, Bell fino a Q 40 (la griglia 1 aveva shelf solo fino a Q 1.5) ----------------
static void secPrecisionX (const char* out) {
    std::FILE* o = std::fopen (out, "w"); std::fprintf (o, "fs,type,f,g,q,slope,err,worstF\n");
    const double FS[] = { 44100, 48000, 88200, 96000, 192000 };
    std::vector<double> F; for (int i = 0; i < 24; ++i) F.push_back (20.0 * std::pow (1000.0, i / 23.0));
    const double GB[] = { -30, -18, -9, -3, 3, 9, 18, 30 };
    for (double fs : FS) { Grid g; g.init (fs);
        auto rec = [&] (const BandParams& b) { double wf; const double e = precErr (b, g, &wf); std::fprintf (o, "%.0f,%s,%.3f,%.1f,%.3f,%d,%.6f,%.1f\n", fs, TN[b.type], b.f, b.gain, b.q, b.slope, e, wf); };
        for (double f : F) {
            for (int t : { LowShelf, HighShelf }) for (double gn : GB) for (double q : { 0.5, 1.0, 2.0, 4.0, 10.0, 20.0, 40.0 }) rec (mk (t, f, gn, q, 12));
            for (double gn : GB) for (double q : { 0.3, 1.0, 4.0, 10.0, 20.0, 40.0 }) rec (mk (Bell, f, gn, q));
        }
    }
    std::fclose (o);
}
// ---------------- CPU nel caso peggiore: tempo di OGNI blocco da 64 campioni (1,33 ms a 48 kHz), non solo la media ----------------
static void secCpuWorst (int reps) {   // costo vero di ogni blocco = minimo tra le ripetizioni (stesso ingresso, stesso stato: il rumore del sistema operativo si somma, non si sottrae)
    const double fs = 48000; const int bl = 64, n = (int) (fs * 12); std::vector<float> L ((size_t) n), R ((size_t) n);
    auto preset = [&] (int k, int seed) { std::mt19937 r2 ((unsigned) (seed * 97 + k)); std::uniform_real_distribution<double> V (0, 1);
        const int types[] = { Bell, Bell, Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, FlatTilt }; const int t = types[(int) (V (r2) * 10) % 10];
        const int sl[] = { 12, 24, 48, 96 }; return mk (t, 20 * std::pow (1000.0, V (r2)), -18 + 36 * V (r2), 0.3 * std::pow (60.0, V (r2)), t == BandPass || t == LowCut || t == HighCut ? sl[(int) (V (r2) * 4) % 4] : 12); };
    const char* names[] = { "preset: 32 bande cambiano tutte insieme ogni secondo", "32 bande dinamiche, raffiche di rumore e silenzio (200 ms)", "32 bande automatizzate a ogni blocco" };
    std::printf ("[");
    for (int sc = 0; sc < 3; ++sc) {
        const size_t nb = (size_t) ((n + bl - 1) / bl); std::vector<double> best (nb, 1e300);
        for (int r = 0; r < reps; ++r) {
            for (int i = 0; i < n; ++i) { double gate = 1; if (sc == 1) gate = ((i / 9600) % 2) ? 0.0 : 1.0; L[(size_t) i] = (float) (CL[(size_t) i % CL.size()] * gate); R[(size_t) i] = (float) (CR[(size_t) i % CR.size()] * gate); }
            Engine e; e.prepare (fs, 4096); Engine::Global g; e.setGlobal (g); size_t bi = 0;
            for (int i0 = 0; i0 < n; i0 += bl, ++bi) { const double u = (double) i0 / n; const auto t0 = Clock::now();
                for (int k = 0; k < 32; ++k) {
                    if (sc == 0) e.setBand (k, preset (k, i0 / 48000));
                    else if (sc == 1) { BandParams b = mk (k % 4 == 1 ? LowShelf : k % 4 == 2 ? HighShelf : Bell, 30 * std::pow (1.24, k), 0, 1.5); b.dyn = true; b.thr = -40; b.range = -12; b.att = 1; b.rel = 30; e.setBand (k, b); }
                    else e.setBand (k, mk (k % 4 == 3 ? HighCut : Bell, 30 * std::pow (1.22, k) * (1 + 0.5 * std::sin (2 * kPi * (u * 7 + k * 0.1))), 3, 1.5, 24));
                }
                e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); best[bi] = std::min (best[bi], std::chrono::duration<double, std::micro> (Clock::now() - t0).count()); }
        }
        double m = 0; for (double x : best) m += x; m /= (double) nb; std::vector<double> srt = best; std::sort (srt.begin(), srt.end()); const double blockUs = bl / fs * 1e6;
        std::printf ("%s{\"scen\": \"%s\", \"block\": %d, \"block_us\": %.1f, \"reps\": %d, \"mean_us\": %.2f, \"p99_us\": %.1f, \"p999_us\": %.1f, \"max_us\": %.1f, \"max_pct_of_block\": %.1f, \"blocks_over_50pct\": %d}", sc ? ", " : "", names[sc], bl, blockUs, reps,
                     m, srt[(size_t) (nb * 0.99)], srt[(size_t) (nb * 0.999)], srt.back(), srt.back() / blockUs * 100, (int) std::count_if (srt.begin(), srt.end(), [&] (double x) { return x > blockUs * 0.5; }));
    }
    std::printf ("]\n");
}
// ---------------- 2. RECIPROCITA: banda +g seguita da banda -g (analogico = esattamente 1) ----------------
static void secRecip (const char* out) {
    std::FILE* o = std::fopen (out, "w"); std::fprintf (o, "fs,type,f,g,q,dev\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 }) { Grid g; g.init (fs);
        for (int i = 0; i < 24; ++i) { const double f = std::min (20.0 * std::pow (1000.0, i / 23.0), fs * 0.495);
            for (int t : { Bell, LowShelf, HighShelf, TiltShelf }) for (double gn : { 3.0, 9.0, 18.0 }) for (double q : { 0.71, 2.0 }) {
                BandParams b = mk (t, f, gn, q); BandCoefs p, m; designBand (b, gn, g, p); designBand (b, -gn, g, m);
                SecSet sp, sm; sectionsFor (b, gn, sp); sectionsFor (b, -gn, sm);
                double dev = 0; const double top = std::min (20000.0, fs / 2 * 0.999);
                for (int k = 0; k < 256; ++k) { const double fr = 20.0 * std::pow (top / 20.0, k / 255.0);
                    const double ana = 10 * std::log10 (anaMag2 (sp, fr) * anaMag2 (sm, fr)); if (std::abs (ana) > 1e-6) continue;   // solo dove l'analogico e davvero reciproco
                    dev = std::max (dev, std::abs (10 * std::log10 ((double) (hMag2 (p, fr, fs) * hMag2 (m, fr, fs))))); }
                std::fprintf (o, "%.0f,%s,%.3f,%.1f,%.2f,%.6f\n", fs, TN[t], f, gn, q, dev); } } }
    std::fclose (o);
}
// ---------------- 3. STABILITA dei progetti: raggio massimo dei poli su tutto lo spazio dei parametri ----------------
static double poleR (const Coef& c) {   // modulo massimo delle radici di z^2 + a1 z + a2
    const double disc = c.a1 * c.a1 - 4 * c.a2;
    if (disc < 0) return std::sqrt (std::max (0.0, c.a2));
    const double s = std::sqrt (disc); return std::max (std::abs ((-c.a1 + s) / 2), std::abs ((-c.a1 - s) / 2));
}
static void secStability() {
    long n = 0, bad = 0, nonfin = 0; double rmax = 0; char worst[200] = "";
    for (double fs : { 8000.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 }) { Grid g; g.init (fs);
        for (int i = 0; i <= 40; ++i) { const double f = 10.0 * std::pow (3000.0, i / 40.0);   // 10 Hz - 30 kHz
            for (int t = 0; t < NumTypes; ++t) for (double gn : { -30.0, -12.0, 0.0, 12.0, 30.0 }) for (double q : { 0.025, 0.1, 0.71, 4.0, 40.0 }) for (int sl : { 6, 12, 24, 48, 96 }) {
                if (! hasGain (t) && gn != 0) continue; if (! hasSlope (t) && sl != 12) continue;
                BandParams b = mk (t, std::min (f, fs * 0.495), gn, q, sl); for (bool gq : { false, true }) { if (gq && t != Bell) continue; b.gq = gq;
                    BandCoefs bc; designBand (b, hasGain (t) ? gn : 0, g, bc); ++n;
                    for (int k = 0; k < bc.n; ++k) { const Coef& c = bc.c[k]; if (! std::isfinite (c.b0 + c.b1 + c.b2 + c.a1 + c.a2)) { ++nonfin; continue; }
                        const double r = poleR (c); if (r >= 1) ++bad; if (r > rmax) { rmax = r; std::snprintf (worst, sizeof worst, "%s fs %.0f f %.2f g %.0f q %.3f slope %d", TN[t], fs, b.f, gn, q, sl); } } } } } }
    std::printf ("{\"designs\": %ld, \"unstable_sections\": %ld, \"nonfinite_sections\": %ld, \"max_pole_radius\": %.12f, \"worst\": \"%s\"}\n", n, bad, nonfin, rmax, worst);
}
// ---------------- 4. COSTO DI PROGETTO (microsecondi per chiamata a designBand) ----------------
static void secDesignCost() {
    Grid g; g.init (48000); std::printf ("{");
    struct C { const char* name; BandParams b; } cs[] = { { "Bell", mk (Bell, 1000, 6, 1) }, { "Bell_HF", mk (Bell, 12000, 9, 2) }, { "LowShelf12", mk (LowShelf, 120, 4, 0.71, 12) },
        { "HighShelf12", mk (HighShelf, 9000, -4, 0.71, 12) }, { "LowCut48", mk (LowCut, 40, 0, 0.71, 48) }, { "HighCut96", mk (HighCut, 15000, 0, 0.71, 96) },
        { "BandPass96", mk (BandPass, 1000, 0, 2, 96) }, { "FlatTilt", mk (FlatTilt, 1000, 3, 1) }, { "Notch", mk (Notch, 3000, 0, 8) } };
    bool first = true;
    for (auto& c : cs) { BandCoefs bc; volatile double sink = 0; int it = 0; const auto t0 = Clock::now(); double el = 0;
        do { for (int k = 0; k < 200; ++k) { const double gn = c.b.gain + 0.01 * ((it + k) % 37); designBand (c.b, gn, g, bc); sink = sink + bc.c[0].b0; } it += 200; el = std::chrono::duration<double> (Clock::now() - t0).count(); } while (el < 0.4);
        std::printf ("%s\"%s\": %.3f", first ? "" : ", ", c.name, el / it * 1e6); first = false; }
    std::printf ("}\n");
}
// ---------------- 5. CPU: % di un core per elaborare audio stereo in tempo reale ----------------
struct Scen { std::string name; std::vector<BandParams> bands; Engine::Global gl; };
static std::vector<Scen> scenarios() {
    std::vector<Scen> v; Engine::Global g0;
    auto add = [&] (const std::string& n, std::vector<BandParams> b, Engine::Global gl) { v.push_back ({ n, b, gl }); };
    add ("1 Bell", { mk (Bell, 1000, 4, 1) }, g0);
    { std::vector<BandParams> b; for (int i = 0; i < 8; ++i) b.push_back (mk (Bell, 60 * std::pow (1.9, i), (i % 2 ? -3 : 3), 1.2)); add ("8 Bell", b, g0); }
    { std::vector<BandParams> b; const int T[] = { Bell, LowShelf, HighShelf, Bell, Notch, Bell, TiltShelf, Bell };
      for (int i = 0; i < 32; ++i) b.push_back (mk (T[i % 8], 30 * std::pow (1.22, i), (i % 2 ? -3 : 3), 1.5)); add ("32 statiche", b, g0); }
    { std::vector<BandParams> b; for (int i = 0; i < 32; ++i) b.push_back (mk (Bell, 30 * std::pow (1.22, i), (i % 2 ? -3 : 3), 1.5, 12, i % 2 ? Mid : Side)); add ("32 Mid/Side", b, g0); }
    { std::vector<BandParams> b; b.push_back (mk (LowCut, 30, 0, .71, 96)); b.push_back (mk (HighCut, 18000, 0, .71, 96)); b.push_back (mk (BandPass, 1000, 0, 1, 96)); add ("tagli 96 dB/oct", b, g0); }
    { std::vector<BandParams> b; for (int i = 0; i < 32; ++i) { BandParams x = mk (i % 2 ? Bell : HighShelf, 30 * std::pow (1.22, i), 3, 1.5);
          if (i % 2 == 0) { x.dyn = true; x.thr = -30; x.range = -1.5; x.att = 5; x.rel = 80; } b.push_back (x); } add ("32 bande, 16 dinamiche (range 1.5)", b, g0); }
    { std::vector<BandParams> b; for (int i = 0; i < 4; ++i) { BandParams x = mk (Bell, 150 * std::pow (3.0, i), 0, 1); x.dyn = true; x.thr = -42; x.range = -12; x.knee = 6; x.att = 1; x.rel = 30; b.push_back (x); }
      add ("4 dinamiche in movimento (range 12, att 1 ms)", b, g0); }
    { std::vector<BandParams> b; for (int i = 0; i < 16; ++i) { BandParams x = mk (Bell, 40 * std::pow (1.5, i), 0, 1); x.dyn = true; x.thr = -45; x.range = -12; x.knee = 6; x.att = 1; x.rel = 30; b.push_back (x); }
      add ("16 dinamiche in movimento (range 12, att 1 ms)", b, g0); }
    { Engine::Global g = g0; g.character = 1; add ("Character Subtle", { mk (Bell, 1000, 3, 1) }, g); }
    { Engine::Global g = g0; g.character = 2; add ("Character Warm", { mk (Bell, 1000, 3, 1) }, g); }
    return v;
}
static double runCpu (const Scen& s, double fs, int bl, double seconds, int reps) {
    const int n = (int) (fs * seconds); std::vector<float> L ((size_t) n), R ((size_t) n);
    std::vector<double> times;
    for (int r = 0; r < reps; ++r) {
        for (int i = 0; i < n; ++i) { L[(size_t) i] = CL[(size_t) i % CL.size()]; R[(size_t) i] = CR[(size_t) i % CR.size()]; }
        Engine e; e.prepare (fs, 4096); e.setGlobal (s.gl); for (size_t k = 0; k < s.bands.size(); ++k) e.setBand ((int) k, s.bands[k]);
        const auto t0 = Clock::now();
        for (int i0 = 0; i0 < n; i0 += bl) { e.setGlobal (s.gl); for (size_t k = 0; k < s.bands.size(); ++k) e.setBand ((int) k, s.bands[k]); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); }
        times.push_back (std::chrono::duration<double> (Clock::now() - t0).count());
    }
    std::sort (times.begin(), times.end()); return times[(size_t) reps / 2] / seconds * 100.0;   // mediana, % di un core
}
static void secCpu (int reps) {
    auto sc = scenarios(); std::printf ("[\n"); bool first = true;
    for (auto& s : sc) for (double fs : { 48000.0, 96000.0 }) for (int bl : { 64, 512 }) {
        if (fs == 96000 && bl == 64) continue;
        const double p = runCpu (s, fs, bl, 20.0, reps);
        std::printf ("%s  {\"scen\": \"%s\", \"fs\": %.0f, \"block\": %d, \"core_pct\": %.4f}", first ? "" : ",\n", s.name.c_str(), fs, bl, p); first = false; std::fflush (stdout); }
    std::printf ("\n]\n");
}
// CPU con automazione: tutte le 32 bande cambiano frequenza a ogni blocco (riprogetto + interpolazione)
static void secCpuAutoHF (int reps) {
    const double fs = 48000; const int bl = 64, n = (int) (fs * 20); std::vector<float> L ((size_t) n), R ((size_t) n); std::vector<double> times;
    for (int r = 0; r < reps; ++r) {
        for (int i = 0; i < n; ++i) { L[(size_t) i] = CL[(size_t) i % CL.size()]; R[(size_t) i] = CR[(size_t) i % CR.size()]; }
        Engine e; e.prepare (fs, 4096); Engine::Global g; e.setGlobal (g); const auto t0 = Clock::now();
        for (int i0 = 0; i0 < n; i0 += bl) { const double u = (double) i0 / n;
            for (int k = 0; k < 4; ++k) e.setBand (k, mk (k == 3 ? HighShelf : Bell, 8000 * std::pow (2.0, 0.5 * (1 + std::sin (2 * kPi * (u * 5 + k * 0.25)))), 6, 1.5, 12));
            e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); }
        times.push_back (std::chrono::duration<double> (Clock::now() - t0).count()); }
    std::sort (times.begin(), times.end()); std::printf ("{\"scen\": \"4 bande acute (8-16 kHz) automatizzate, blocchi da 64\", \"fs\": 48000, \"block\": 64, \"core_pct\": %.4f}\n", times[(size_t) reps / 2] / 20.0 * 100);
}
static void secCpuAuto (int reps) {
    const double fs = 48000; const int bl = 128, n = (int) (fs * 20); std::vector<float> L ((size_t) n), R ((size_t) n); std::vector<double> times;
    for (int r = 0; r < reps; ++r) {
        for (int i = 0; i < n; ++i) { L[(size_t) i] = CL[(size_t) i % CL.size()]; R[(size_t) i] = CR[(size_t) i % CR.size()]; }
        Engine e; e.prepare (fs, 4096); Engine::Global g; e.setGlobal (g); const auto t0 = Clock::now();
        for (int i0 = 0; i0 < n; i0 += bl) { const double u = (double) i0 / n;
            for (int k = 0; k < 32; ++k) e.setBand (k, mk (k % 4 == 3 ? HighCut : Bell, 30 * std::pow (1.22, k) * (1 + 0.5 * std::sin (2 * kPi * (u * 7 + k * 0.1))), 3, 1.5, 24));
            e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); }
        times.push_back (std::chrono::duration<double> (Clock::now() - t0).count()); }
    std::sort (times.begin(), times.end()); std::printf ("{\"scen\": \"32 bande automatizzate a ogni blocco da 128\", \"fs\": 48000, \"block\": 128, \"core_pct\": %.4f}\n", times[(size_t) reps / 2] / 20.0 * 100);
}
// ---------------- 6. INVARIANZA AL BLOCCO: stessa uscita con blocchi diversi ----------------
static std::vector<float> render (const std::vector<BandParams>& bands, const Engine::Global& gl, int n, int bl, bool randomBl, unsigned seed) {
    std::vector<float> L (CL.begin() + 48000 * 40, CL.begin() + 48000 * 40 + n), R (CR.begin() + 48000 * 40, CR.begin() + 48000 * 40 + n);
    Engine e; e.prepare (48000, 4096); std::mt19937 rng (seed);
    for (int i0 = 0; i0 < n; ) { const int b = randomBl ? 1 + (int) (rng() % 1500) : bl; const int m = std::min (b, n - i0);
        e.setGlobal (gl); for (size_t k = 0; k < bands.size(); ++k) e.setBand ((int) k, bands[k]); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, m); i0 += m; }
    std::vector<float> o (L); o.insert (o.end(), R.begin(), R.end()); return o;
}
static void secBlock() {
    std::vector<BandParams> b = { mk (LowCut, 35, 0, .71, 24), mk (Bell, 220, -3, 1.4), mk (HighShelf, 8000, 2.5, .71), mk (Bell, 3000, 2, 2, 12, Mid), mk (Bell, 6000, -2, 1, 12, Side) };
    BandParams d = mk (Bell, 400, 0, 1); d.dyn = true; d.thr = -40; d.range = -6; d.att = 2; d.rel = 60; b.push_back (d);
    Engine::Global gl; gl.character = 1;
    const int n = 48000 * 8; const auto ref = render (b, gl, n, 512, false, 0); std::printf ("[");
    bool first = true;
    for (int bl : { 1, 7, 32, 64, 127, 333, 1024, 4096, -1 }) { const auto o = render (b, gl, n, bl, bl < 0, 1234); double m = 0;
        for (size_t i = 0; i < o.size(); ++i) m = std::max (m, (double) std::abs (o[i] - ref[i]));
        std::printf ("%s{\"block\": \"%s\", \"maxdiff_dbfs\": %.1f}", first ? "" : ", ", bl < 0 ? "casuale 1-1500" : std::to_string (bl).c_str(), 20 * std::log10 (std::max (m, 1e-30))); first = false; }
    std::printf ("]\n");
}
// ---------------- 7. NaN / Inf / valori enormi in ingresso: il motore si riprende? ----------------
static void secNan() {
    std::printf ("[");
    const char* kinds[] = { "NaN", "+Inf", "1e30" }; bool first = true;
    for (int k = 0; k < 3; ++k) {
        const int n = 48000 * 4; std::vector<float> L (CL.begin() + 48000 * 40, CL.begin() + 48000 * 40 + n), R (CR.begin() + 48000 * 40, CR.begin() + 48000 * 40 + n);
        const float bad = k == 0 ? std::nanf ("") : (k == 1 ? INFINITY : 1e30f); L[48000] = bad; R[48000] = bad;
        Engine e; e.prepare (48000, 4096); Engine::Global gl; e.setGlobal (gl); e.setBand (0, mk (Bell, 1000, 4, 1)); e.setBand (1, mk (LowCut, 40, 0, .71, 24));
        for (int i0 = 0; i0 < n; i0 += 512) { e.setGlobal (gl); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, 512); }
        int badOut = 0, lastBad = -1; for (int i = 0; i < n; ++i) if (! std::isfinite (L[(size_t) i]) || std::abs (L[(size_t) i]) > 1e6) { ++badOut; lastBad = i; }
        std::printf ("%s{\"input\": \"%s al campione 48000\", \"bad_samples\": %d, \"last_bad\": %d, \"recovered\": %s}", first ? "" : ", ", kinds[k], badOut, lastBad, (lastBad < 48000 * 2) ? "true" : "false"); first = false; }
    std::printf ("]\n");
}
// ---------------- 8. DENORMALI (senza flush-to-zero): costo della coda di silenzio ----------------
static void secDenormal() {
    const double fs = 48000; const int n = (int) fs * 6; std::vector<float> L ((size_t) n, 0.f), R ((size_t) n, 0.f);
    for (int i = 0; i < 4800; ++i) { L[(size_t) i] = R[(size_t) i] = (float) (0.5 * std::sin (2 * kPi * 440 * i / fs)); }
    Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); for (int k = 0; k < 32; ++k) e.setBand (k, mk (Bell, 30 * std::pow (1.22, k), 3, 1.5));
    std::vector<double> perSec;
    for (int s = 0; s < 6; ++s) { const auto t0 = Clock::now(); for (int i0 = s * 48000; i0 < (s + 1) * 48000; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, (s + 1) * 48000 - i0));
        perSec.push_back (std::chrono::duration<double> (Clock::now() - t0).count() * 100); }
    // riferimento: stesso carico con audio vero
    std::vector<float> A (CL.begin(), CL.begin() + 48000), B (CR.begin(), CR.begin() + 48000); Engine e2; e2.prepare (fs, 4096); e2.setGlobal (gl); for (int k = 0; k < 32; ++k) e2.setBand (k, mk (Bell, 30 * std::pow (1.22, k), 3, 1.5));
    const auto t0 = Clock::now(); for (int i0 = 0; i0 < 48000; i0 += 512) e2.process (A.data() + i0, B.data() + i0, nullptr, nullptr, std::min (512, 48000 - i0)); const double ref = std::chrono::duration<double> (Clock::now() - t0).count() * 100;
    std::printf ("{\"core_pct_per_second_after_tone\": [%.3f, %.3f, %.3f, %.3f, %.3f, %.3f], \"core_pct_music\": %.3f, \"tail_last_sample\": %.3e}\n", perSec[0], perSec[1], perSec[2], perSec[3], perSec[4], perSec[5], ref, (double) L[(size_t) n - 1]);
}
// ---------------- 9. PARAMETRI ESTREMI: impulso finito e che decade, a tutte le frequenze di campionamento ----------------
static void secExtreme() {
    long cases = 0, nonfinite = 0, notDecay = 0; char worst[200] = "";
    for (double fs : { 8000.0, 22050.0, 44100.0, 96000.0, 192000.0, 384000.0 }) {
        for (int t = 0; t < NumTypes; ++t) for (double f : { 10.0, 30000.0 }) for (double gn : { -30.0, 30.0 }) for (double q : { 0.025, 40.0 }) for (int sl : { 6, 96 }) {
            if (! hasSlope (t) && sl != 6) continue; if (! hasGain (t) && gn != 30) continue;
            const int n = (int) std::min (fs * 20, 2000000.0); std::vector<float> L ((size_t) n, 0.f), R ((size_t) n, 0.f); L[0] = R[0] = 1;
            Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); BandParams b = mk (t, std::min (f, fs * 0.495), gn, q, sl); e.setBand (0, b);
            e.process (L.data(), R.data(), nullptr, nullptr, n); ++cases;
            bool fin = true; double tail = 0; for (int i = 0; i < n; ++i) { if (! std::isfinite (L[(size_t) i])) fin = false; if (i > n - n / 10) tail = std::max (tail, (double) std::abs (L[(size_t) i])); }
            if (! fin) { ++nonfinite; std::snprintf (worst, sizeof worst, "NONFINITO %s fs %.0f f %.0f g %.0f q %.3f sl %d", TN[t], fs, b.f, gn, q, sl); }
            else if (tail > 1e-4) { ++notDecay; std::snprintf (worst, sizeof worst, "coda %.2e %s fs %.0f f %.0f g %.0f q %.3f sl %d", tail, TN[t], fs, b.f, gn, q, sl); } } }
    std::printf ("{\"cases\": %ld, \"nonfinite\": %ld, \"not_decaying_after_20s\": %ld, \"note\": \"%s\"}\n", cases, nonfinite, notDecay, worst);
}
// ---------------- 10. FUZZ: automazione casuale di tutto, blocchi casuali, 60 s di clip ----------------
static void secFuzz (unsigned seed, double seconds) {
    std::mt19937 rng (seed); auto U = [&] (double a, double b) { return a + (b - a) * (rng() / 4294967296.0); }; auto I = [&] (int a, int b) { return a + (int) (rng() % (unsigned) (b - a + 1)); };
    const double fs = 48000; const int n = (int) (fs * seconds); std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) { L[(size_t) i] = CL[(size_t) i % CL.size()]; R[(size_t) i] = CR[(size_t) i % CR.size()]; }
    Engine e; e.prepare (fs, 4096); BandParams b[32]; Engine::Global g;
    for (int k = 0; k < 32; ++k) { b[k] = mk (I (0, 9), std::exp (U (std::log (10.0), std::log (30000.0))), U (-30, 30), std::exp (U (std::log (0.025), std::log (40.0))), 6 * I (1, 16)); b[k].used = rng() % 3 == 0; }
    long nonfin = 0; double peak = 0; int blocks = 0; const auto t0 = Clock::now();
    for (int i0 = 0; i0 < n; ) {
        const int m = std::min (I (1, 2048), n - i0);
        for (int c = 0; c < 3; ++c) { const int k = I (0, 31); switch (I (0, 9)) {
            case 0: b[k].used = ! b[k].used; break; case 1: b[k].type = I (0, 9); break; case 2: b[k].f = std::exp (U (std::log (10.0), std::log (30000.0))); break;
            case 3: b[k].gain = U (-30, 30); break; case 4: b[k].q = std::exp (U (std::log (0.025), std::log (40.0))); break; case 5: b[k].slope = 6 * I (1, 16); break;
            case 6: b[k].place = I (0, 4); break; case 7: b[k].dyn = ! b[k].dyn; b[k].thr = U (-60, 0); b[k].range = U (-24, 24); b[k].att = U (0.1, 200); b[k].rel = U (5, 2000); b[k].knee = U (0, 24); b[k].det = I (0, 1); break;
            case 8: b[k].bypass = ! b[k].bypass; break; default: b[k].gq = ! b[k].gq; } }
        if (I (0, 50) == 0) g.character = I (0, 2); if (I (0, 80) == 0) g.solo = I (-1, 31); if (I (0, 60) == 0) g.bypass = ! g.bypass;
        if (I (0, 40) == 0) g.outDb = U (-60, 12); if (I (0, 40) == 0) g.inDb = U (-24, 12); if (I (0, 40) == 0) g.pan = U (-1, 1); if (I (0, 60) == 0) g.panMS = ! g.panMS; if (I (0, 60) == 0) g.invert = ! g.invert; if (I (0, 60) == 0) g.scale = U (-1, 2);
        e.setGlobal (g); for (int k = 0; k < 32; ++k) { BandParams x = b[k]; x.f = std::min (x.f, fs * 0.495); x.gq = x.gq && x.type == Bell; e.setBand (k, x); }
        e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, m);
        for (int i = i0; i < i0 + m; ++i) { if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) ++nonfin; else peak = std::max (peak, (double) std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]))); }
        i0 += m; ++blocks; }
    const double el = std::chrono::duration<double> (Clock::now() - t0).count();
    std::printf ("{\"seed\": %u, \"seconds\": %.0f, \"blocks\": %d, \"nonfinite_samples\": %ld, \"peak_dbfs\": %.1f, \"core_pct\": %.2f}\n", seed, seconds, blocks, nonfin, 20 * std::log10 (std::max (peak, 1e-30)), el / seconds * 100);
}
// ---------------- 11. DINAMICA: guadagno a regime vs caratteristica teorica ----------------
static void secDynAcc() {
    const double fs = 48000; double worst = 0; char wc[160] = ""; int cases = 0;
    for (double thr : { -40.0, -24.0, -12.0 }) for (double knee : { 0.0, 6.0, 12.0 }) for (double range : { -1.5, -6.0, 6.0 }) for (double lvl : { -30.0, -20.0, -10.0 }) for (int det : { 0, 1 }) {
        const int n = (int) fs * 2; std::vector<float> L ((size_t) n), R ((size_t) n); const double amp = std::pow (10.0, lvl / 20) * std::sqrt (2.0);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * 1000 * i / fs));
        Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); BandParams b = mk (Bell, 1000, 0, 1); b.dyn = true; b.thr = thr; b.knee = knee; b.range = range; b.att = 5; b.rel = 80; b.det = det; e.setBand (0, b);
        e.process (L.data(), R.data(), nullptr, nullptr, n);
        double s = 0; for (int i = n - 9600; i < n; ++i) s += (double) L[(size_t) i] * L[(size_t) i]; const double outDb = 10 * std::log10 (s / 9600) - lvl;
        BandParams bb = b; const double expect = dynDelta (lvl, bb);   // detector a banda: tono al centro della banda, livello RMS = lvl (Peak scalato come sinusoide RMS)
        const double err = std::abs (outDb - expect); ++cases; if (std::getenv ("DYNLOG")) std::fprintf (stderr, "thr %.0f knee %.0f range %.1f lvl %.0f det %d misurato %.3f atteso %.3f\n", thr, knee, range, lvl, det, outDb, expect); if (err > worst) { worst = err; std::snprintf (wc, sizeof wc, "thr %.0f knee %.0f range %.1f lvl %.0f det %d: misurato %.3f atteso %.3f", thr, knee, range, lvl, det, outDb, expect); } }
    std::printf ("{\"cases\": %d, \"worst_err_db\": %.4f, \"worst\": \"%s\"}\n", cases, worst, wc);
}

// ---------------- 12. VERIFICA CON SEGNALE: sinusoide stazionaria attraverso l'Engine alla frequenza di errore massimo ----------------
static void secSigVerify (const char* csvIn) {
    std::ifstream f (csvIn); std::string line; std::getline (f, line); int shown = 0;
    std::printf ("[");
    while (std::getline (f, line)) {
        double fs, fr, g, q, err, wf; int sl; char tn[32];
        if (std::sscanf (line.c_str(), "%lf,%31[^,],%lf,%lf,%lf,%d,%lf,%lf", &fs, tn, &fr, &g, &q, &sl, &err, &wf) != 8) continue;
        int t = -1; for (int k = 0; k < NumTypes; ++k) if (std::string (TN[k]) == tn) t = k; if (t < 0) continue;
        BandParams b = mk (t, std::min (fr, fs * 0.495), g, q, sl); Grid gr; gr.init (fs);
        SecSet set; sectionsFor (b, hasGain (t) ? g : 0, set); const double ana = 10 * std::log10 (anaMag2 (set, wf));
        // segnale: sinusoide a wf, 30 s (le bande strette a bassa frequenza hanno code lunghe), ampiezza misurata negli ultimi 2 s per correlazione
        const int n = (int) (fs * 30); std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (2 * kPi * wf * i / fs));
        Engine e; e.prepare (fs, 4096); Engine::Global gl; e.setGlobal (gl); e.setBand (0, b);
        for (int i0 = 0; i0 < n; i0 += 4096) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (4096, n - i0));
        double sc = 0, ss = 0; const int m0 = n - (int) (fs * 2);
        for (int i = m0; i < n; ++i) { const double ph = 2 * kPi * wf * i / fs; sc += L[(size_t) i] * std::cos (ph); ss += L[(size_t) i] * std::sin (ph); }
        const double amp = 2 * std::sqrt (sc * sc + ss * ss) / (n - m0), meas = 20 * std::log10 (std::max (amp / 0.25, 1e-15));
        std::printf ("%s{\"case\": \"%s fs %.0f f %.2f g %.1f q %.2f sl %d\", \"at_hz\": %.1f, \"analog_db\": %.3f, \"measured_db\": %.3f, \"err_db\": %.3f, \"csv_err\": %.3f}",
                     shown ? ",\n " : "", tn, fs, fr, g, q, sl, wf, ana, meas, std::abs (meas - ana), err); ++shown; }
    std::printf ("]\n");
}
// ---------------- 13. COSTO DI PROGETTO su tutta la griglia (media, p95, max per tipo) a 48 kHz ----------------
#ifdef NEWENG
#define DESIGN_FAST(b, gn, g, bc) designBand (b, gn, g, bc, false)
#else
#define DESIGN_FAST(b, gn, g, bc) designBand (b, gn, g, bc)
#endif
static void secDesignGrid() {
    const double fs = 48000; Grid g; g.init (fs); std::vector<double> F; for (int i = 0; i < 24; ++i) F.push_back (20.0 * std::pow (1000.0, i / 23.0));
    std::vector<std::pair<std::string, std::vector<double>>> T;
    auto timeIt = [&] (const std::string& nm, const BandParams& b) { BandCoefs bc; const auto t0 = Clock::now(); for (int r = 0; r < 20; ++r) designBand (b, hasGain (b.type) ? b.gain + 1e-6 * r : 0, g, bc);
        const double us = std::chrono::duration<double> (Clock::now() - t0).count() / 20 * 1e6; for (auto& x : T) if (x.first == nm) { x.second.push_back (us); return; } T.push_back ({ nm, { us } }); };
    for (double f : F) {
        for (double gn : { -18.0, -9.0, -3.0, 3.0, 9.0, 18.0 }) for (double q : { 0.3, 0.7, 2.0, 6.0, 20.0 }) timeIt ("Bell", mk (Bell, f, gn, q));
        for (int t : { LowShelf, HighShelf }) for (double gn : { -18.0, -9.0, 9.0, 18.0 }) for (double q : { 0.5, 0.71, 1.5 }) timeIt ("Shelf", mk (t, f, gn, q, 12));
        for (int sl : { 12, 24, 48, 96 }) { timeIt ("HighCut", mk (HighCut, f, 0, .71, sl)); timeIt ("LowCut", mk (LowCut, f, 0, .71, sl)); }
        for (double q : { 0.5, 2.0, 8.0, 30.0 }) timeIt ("Notch", mk (Notch, f, 0, q));
        for (int sl : { 6, 24, 96 }) for (double q : { 0.5, 2.0, 8.0 }) timeIt ("BandPass", mk (BandPass, f, 0, q, sl));
        for (double gn : { -9.0, 9.0 }) timeIt ("FlatTilt", mk (FlatTilt, f, gn, 1));
    }
    std::printf ("{"); bool first = true;
    for (auto& x : T) { auto v = x.second; std::sort (v.begin(), v.end()); double m = 0; for (double u : v) m += u; m /= v.size();
        std::printf ("%s\"%s\": {\"mean_us\": %.2f, \"p95_us\": %.2f, \"max_us\": %.2f}", first ? "" : ", ", x.first.c_str(), m, v[(size_t) (v.size() * 0.95)], v.back()); first = false; }
    std::printf ("}\n");
}
static void secDesignGridFast() {   // progetto veloce (quello usato mentre i parametri si muovono); 0.4.3 ha un solo modo
    const double fs = 48000; Grid g; g.init (fs); std::vector<double> F; for (int i = 0; i < 24; ++i) F.push_back (20.0 * std::pow (1000.0, i / 23.0));
    std::vector<std::pair<std::string, std::vector<double>>> T;
    auto timeIt = [&] (const std::string& nm, const BandParams& b) { BandCoefs bc; const auto t0 = Clock::now(); for (int r = 0; r < 20; ++r) DESIGN_FAST (b, hasGain (b.type) ? b.gain + 1e-6 * r : 0, g, bc);
        const double us = std::chrono::duration<double> (Clock::now() - t0).count() / 20 * 1e6; for (auto& x : T) if (x.first == nm) { x.second.push_back (us); return; } T.push_back ({ nm, { us } }); };
    for (double f : F) {
        for (double gn : { -18.0, -9.0, -3.0, 3.0, 9.0, 18.0 }) for (double q : { 0.3, 0.7, 2.0, 6.0, 20.0 }) timeIt ("Bell", mk (Bell, f, gn, q));
        for (int t : { LowShelf, HighShelf }) for (double gn : { -18.0, -9.0, 9.0, 18.0 }) for (double q : { 0.5, 0.71, 1.5 }) timeIt ("Shelf", mk (t, f, gn, q, 12));
        for (int sl : { 12, 24, 48, 96 }) { timeIt ("HighCut", mk (HighCut, f, 0, .71, sl)); timeIt ("LowCut", mk (LowCut, f, 0, .71, sl)); }
        for (double q : { 0.5, 2.0, 8.0, 30.0 }) timeIt ("Notch", mk (Notch, f, 0, q));
        for (int sl : { 6, 24, 96 }) for (double q : { 0.5, 2.0, 8.0 }) timeIt ("BandPass", mk (BandPass, f, 0, q, sl));
        for (double gn : { -9.0, 9.0 }) timeIt ("FlatTilt", mk (FlatTilt, f, gn, 1));
    }
    std::printf ("{"); bool first = true;
    for (auto& x : T) { auto v = x.second; std::sort (v.begin(), v.end()); double m = 0; for (double u : v) m += u; m /= v.size();
        std::printf ("%s\"%s\": {\"mean_us\": %.2f, \"p95_us\": %.2f, \"max_us\": %.2f}", first ? "" : ", ", x.first.c_str(), m, v[(size_t) (v.size() * 0.95)], v.back()); first = false; }
    std::printf ("}\n");
}
int main (int argc, char** argv) {
    if (argc < 2) { std::fprintf (stderr, "uso: lab sezione\n"); return 2; }
    const std::string s = argv[1]; const char* clip = std::getenv ("TSRQ_CLIP"); if (clip) loadClip (clip);
    if (s == "precision") secPrecision (argv[2]);
    else if (s == "precisionx") secPrecisionX (argv[2]);
    else if (s == "designgridfast") secDesignGridFast();
    else if (s == "cpuworst") secCpuWorst (argc > 2 ? std::atoi (argv[2]) : 3);
    else if (s == "recip") secRecip (argv[2]);
    else if (s == "stability") secStability();
    else if (s == "designcost") secDesignCost();
    else if (s == "cpu") secCpu (argc > 2 ? std::atoi (argv[2]) : 5);
    else if (s == "cpuauto") secCpuAuto (argc > 2 ? std::atoi (argv[2]) : 5);
    else if (s == "cpuautohf") secCpuAutoHF (argc > 2 ? std::atoi (argv[2]) : 5);
    else if (s == "block") secBlock();
    else if (s == "nan") secNan();
    else if (s == "denormal") secDenormal();
    else if (s == "extreme") secExtreme();
    else if (s == "fuzz") secFuzz ((unsigned) std::atoi (argv[2]), std::atof (argv[3]));
    else if (s == "dynacc") secDynAcc();
    else if (s == "sigverify") secSigVerify (argv[2]);
    else if (s == "designgrid") secDesignGrid();
    else { std::fprintf (stderr, "sezione sconosciuta\n"); return 2; }
    return 0;
}
