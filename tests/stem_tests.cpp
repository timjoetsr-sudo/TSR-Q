// TSR Q · test profondo del motore C++ sugli stem reali (float32 planari L|R, 48 kHz)
// uso: stem_tests <cartella stem>   — esce con codice 1 se un test fallisce
#include "../Source/TsrqDsp.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <complex>

using namespace tsrq;
static int fails = 0, passes = 0;
static void ok (bool c, const std::string& m) { std::printf ("%s %s\n", c ? "PASS" : "FAIL", m.c_str()); (c ? passes : fails)++; }
static double db (double x) { return 20 * std::log10 (std::max (x, 1e-15)); }

struct Stem { std::string name; std::vector<float> L, R; };
static bool load (const std::string& path, int n, Stem& s) {
    std::ifstream f (path, std::ios::binary); if (! f) return false;
    s.L.resize ((size_t) n); s.R.resize ((size_t) n);
    f.read ((char*) s.L.data(), n * 4); f.read ((char*) s.R.data(), n * 4); return (bool) f;
}
static BandParams band (int type, double f, double g, double q, int slope = 12, int place = Stereo) {
    BandParams b; b.used = true; b.type = type; b.f = f; b.gain = g; b.q = q; b.slope = slope; b.place = place; return b;
}
static BandParams dyn (BandParams b, double thr, double range, int det = DetRMS) { b.dyn = true; b.thr = thr; b.range = range; b.det = det; return b; }

// catena "da mix" per ogni tipo di stem
static std::vector<BandParams> chainFor (const std::string& n) {
    if (n == "KICK")   return { band (LowCut, 25, 0, .707, 24), band (Bell, 60, 3, 1.2), band (Bell, 350, -4, 2), dyn (band (Bell, 4000, 0, 1.5), -30, 3) };
    if (n == "BASS")   return { band (LowCut, 30, 0, .707, 24), band (LowShelf, 80, 2, .707), dyn (band (Bell, 200, 0, 1.2), -30, -4) };
    if (n == "VOCAL")  return { band (LowCut, 90, 0, .707, 24), band (Bell, 300, -3, 1.5), dyn (band (Bell, 6500, 0, 3), -35, -6), band (HighShelf, 10000, 2, .707) };
    if (n == "MELODY") return { band (Bell, 500, -2, 1, 12, Mid), band (HighShelf, 8000, 3, .707, 12, Side), band (LowCut, 150, 0, .707, 12, Side) };
    if (n.rfind ("HH", 0) == 0)   return { band (LowCut, 300, 0, .707, 48), dyn (band (HighShelf, 8000, 0, .707), -40, -4) };
    return { band (Bell, 1500, 2, 1), dyn (band (Bell, 250, 0, 1.5), -35, -5), band (Notch, 3000, 0, 8) };   // CLAP
}

static void run (Engine& e, const std::vector<BandParams>& ch, const Engine::Global& g, std::vector<float>& L, std::vector<float>& R, int block) {
    e.setGlobal (g);
    for (int i = 0; i < kMaxBands; ++i) e.setBand (i, i < (int) ch.size() ? ch[(size_t) i] : BandParams());
    for (size_t i0 = 0; i0 < L.size(); i0 += (size_t) block) {
        const int n = (int) std::min ((size_t) block, L.size() - i0);
        for (int i = 0; i < kMaxBands; ++i) e.setBand (i, i < (int) ch.size() ? ch[(size_t) i] : BandParams());
        e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, n);
    }
}
static double maxAbs (const std::vector<float>& a) { double m = 0; for (float v : a) m = std::max (m, (double) std::abs (v)); return m; }
static double maxDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from = 0) { double m = 0; for (size_t i = from; i < a.size(); ++i) m = std::max (m, (double) std::abs (a[i] - b[i])); return m; }
static bool finite (const std::vector<float>& a) { for (float v : a) if (! std::isfinite (v)) return false; return true; }

// risposta in ampiezza misurata dall'audio (Welch, Hann 8192, 50%) — stima statistica, solo informativa: |Sxy|/Sxx, solo dove la coerenza è alta
static void measured (const std::vector<float>& x, const std::vector<float>& y, double fs, std::vector<double>& fr, std::vector<double>& mag, std::vector<double>& coh) {
    const int N = 8192; std::vector<std::complex<double>> Sxy (N / 2), X (N), Y (N); std::vector<double> Sxx (N / 2), Syy (N / 2), w (N);
    for (int i = 0; i < N; ++i) w[(size_t) i] = 0.5 - 0.5 * std::cos (2 * kPi * i / (N - 1));
    auto fft = [] (std::vector<std::complex<double>>& a) { const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1) { const double ang = -2 * kPi / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len) { std::complex<double> wv (1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * wv; a[i + k] = u + v; a[i + k + len / 2] = u - v; wv *= wl; } } } };
    for (size_t s = 0; s + N <= x.size(); s += N / 2) {
        for (int i = 0; i < N; ++i) { X[(size_t) i] = x[s + (size_t) i] * w[(size_t) i]; Y[(size_t) i] = y[s + (size_t) i] * w[(size_t) i]; }
        fft (X); fft (Y);
        for (int k = 0; k < N / 2; ++k) { Sxy[(size_t) k] += std::conj (X[(size_t) k]) * Y[(size_t) k]; Sxx[(size_t) k] += std::norm (X[(size_t) k]); Syy[(size_t) k] += std::norm (Y[(size_t) k]); }
    }
    fr.clear(); mag.clear(); coh.clear();
    for (int k = 1; k < N / 2; ++k) { fr.push_back (k * fs / N); mag.push_back (std::abs (Sxy[(size_t) k]) / std::max (Sxx[(size_t) k], 1e-30)); coh.push_back (std::norm (Sxy[(size_t) k]) / std::max (Sxx[(size_t) k] * Syy[(size_t) k], 1e-60)); }
}
static double designed (const std::vector<BandParams>& ch, const Grid& gr, double f, double fs, int place) {
    double m = 1; for (auto& b : ch) if (b.place == place && ! b.dyn) { BandCoefs c; designBand (b, hasGain (b.type) ? b.gain : 0, gr, c); m *= std::sqrt (bandMag2 (c, f, fs)); } return m;
}

int main (int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    const char* names[] = { "KICK", "BASS", "VOCAL", "MELODY", "HH", "HH-1", "HH-2", "HH-3", "CLAP", "CLAP-1", "CLAP-2", "CLAP-3" };
    const double fs = 48000; const int n = 1047273;   // 21.82 s (lunghezza reale degli stem)
    std::vector<Stem> stems;
    for (auto nm : names) { Stem s; s.name = nm; if (! load (dir + "/" + nm + ".f32", n, s)) { std::printf ("FAIL stem %s non leggibile\n", nm); return 1; } stems.push_back (std::move (s)); }
    std::printf ("== stem caricati: %zu, %.2f s ciascuno ==\n", stems.size(), n / fs);

    for (auto& s : stems) {
        std::printf ("\n== %s (picco %.1f dBFS) ==\n", s.name.c_str(), db (std::max (maxAbs (s.L), maxAbs (s.R))));
        const auto ch = chainFor (s.name); Engine::Global g;
        // 1. nessuna banda = trasparente al bit
        { Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; run (e, {}, g, L, R, 512); ok (maxDiff (L, s.L) == 0 && maxDiff (R, s.R) == 0, s.name + ": senza bande l'uscita è identica all'ingresso (bit per bit)"); }
        // 2. bande a 0 dB (Bell/shelf/tilt) = trasparente
        { Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; run (e, { band (Bell, 200, 0, 2), band (LowShelf, 100, 0, .7), band (HighShelf, 8000, 0, .7), band (TiltShelf, 1000, 0, .7) }, g, L, R, 512);
          ok (db (std::max (maxDiff (L, s.L), maxDiff (R, s.R))) < -100, s.name + ": 4 bande a 0 dB, residuo " + std::to_string ((int) db (std::max (maxDiff (L, s.L), maxDiff (R, s.R)))) + " dBFS (< -100)"); }
        // 3. catena da mix: uscita finita e limitata
        Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; run (e, ch, g, L, R, 512);
        ok (finite (L) && finite (R), s.name + ": catena da mix, nessun NaN/Inf");
        ok (std::max (maxAbs (L), maxAbs (R)) < 1.0, s.name + ": catena da mix, picco " + std::to_string (db (std::max (maxAbs (L), maxAbs (R)))).substr (0, 6) + " dBFS (< 0)");
        // 4. bypass globale: dopo la dissolvenza di 8 ms l'uscita è l'ingresso
        { Engine e2; e2.prepare (fs, 512); auto L2 = s.L, R2 = s.R; Engine::Global gb; gb.bypass = true; run (e2, ch, gb, L2, R2, 512);
          ok (db (std::max (maxDiff (L2, s.L, 4800), maxDiff (R2, s.R, 4800))) < -100, s.name + ": BYPASS, residuo dopo 100 ms " + std::to_string ((int) db (std::max (maxDiff (L2, s.L, 4800), maxDiff (R2, s.R, 4800)))) + " dBFS"); }
        // 5. indipendenza dalla dimensione del blocco (DAW diverse)
        { double worst = 0; for (int bl : { 32, 64, 100, 256, 1024, 4096 }) { Engine e2; e2.prepare (fs, 4096); auto L2 = s.L, R2 = s.R; run (e2, ch, g, L2, R2, bl); worst = std::max (worst, std::max (maxDiff (L2, L), maxDiff (R2, R))); }
          const bool anyDyn = std::any_of (ch.begin(), ch.end(), [] (const BandParams& b) { return b.dyn; });
          ok (db (worst) < -100, s.name + ": blocchi 32…4096 (DAW diverse), differenza max " + std::to_string ((int) db (worst)) + " dBFS (soglia -100" + (anyDyn ? ", anche con dinamica" : "") + ")"); }
        // 6. risposta misurata sull'audio = curva progettata (solo bande statiche Stereo, dove la coerenza è alta)
        if (s.name != "HH-1" && s.name != "MELODY") {
            std::vector<BandParams> st; for (auto b : ch) if (! b.dyn) st.push_back (b);
            Engine e2; e2.prepare (fs, 512); auto L2 = s.L, R2 = s.R; run (e2, st, g, L2, R2, 512);
            std::vector<double> fr, mg, ch2; measured (s.L, L2, fs, fr, mg, ch2); double worst = 0, wf = 0; int used = 0;
            for (size_t k = 0; k < fr.size(); ++k) if (fr[k] > 30 && fr[k] < 18000 && ch2[k] > 0.999) { const double d = std::abs (db (mg[k]) - db (designed (st, e2.getGrid(), fr[k], fs, Stereo))); if (designed (st, e2.getGrid(), fr[k], fs, Stereo) > 0.03) { if (d > worst) { worst = d; wf = fr[k]; } ++used; } }
            std::printf ("INFO %s: risposta stimata dall'audio (Welch) vs progettata, scarto max %.3f dB su %d frequenze (peggiore a %d Hz) — stima statistica, non è un test\n", s.name.c_str(), worst, used, (int) wf);
            // riferimento indipendente: stessi coefficienti applicati in Direct Form I (double) campione per campione → deve coincidere con il motore
            std::vector<double> rl (s.L.begin(), s.L.end());
            for (auto& b : st) { BandCoefs c; designBand (b, hasGain (b.type) ? b.gain : 0, e2.getGrid(), c);
                for (int k = 0; k < c.n; ++k) { const Coef& q = c.c[k]; double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                    for (auto& v : rl) { const double y = q.b0 * v + q.b1 * x1 + q.b2 * x2 - q.a1 * y1 - q.a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; v = y; } } }
            double rd = 0; for (size_t i = 0; i < rl.size(); ++i) rd = std::max (rd, std::abs (rl[i] - (double) L2[i]));
            ok (db (rd) < -100, s.name + ": uscita = riferimento indipendente (Direct Form I, double), differenza max " + std::to_string ((int) db (rd)) + " dBFS (< -100)");
        }
        // 7. dinamica: mai oltre il RANGE, zero sotto soglia
        for (size_t bi = 0; bi < ch.size(); ++bi) if (ch[bi].dyn) {
            Engine e2; e2.prepare (fs, 512); auto L2 = s.L, R2 = s.R; e2.setGlobal (g); double mx = 0, mn = 0; int on = 0, blocks = 0;
            for (size_t i0 = 0; i0 < L2.size(); i0 += 512) { for (size_t b = 0; b < ch.size(); ++b) e2.setBand ((int) b, ch[b]); e2.process (L2.data() + i0, R2.data() + i0, nullptr, nullptr, (int) std::min ((size_t) 512, L2.size() - i0));
                const double d = e2.meterDelta[bi].load(); mx = std::max (mx, d); mn = std::min (mn, d); if (std::abs (d) > 0.1) ++on; ++blocks; }
            const double r = ch[bi].range; const bool inRange = r < 0 ? (mn >= r - 1e-6 && mx <= 1e-9) : (mx <= r + 1e-6 && mn >= -1e-9);
            ok (inRange, s.name + ": banda dinamica " + std::to_string ((int) ch[bi].f) + " Hz, intervento " + std::to_string (r < 0 ? mn : mx).substr (0, 5) + " dB dentro RANGE " + std::to_string ((int) r) + ", attiva nel " + std::to_string (100 * on / std::max (1, blocks)) + "% dei blocchi");
            BandParams hi = ch[bi]; hi.thr = 0; std::vector<BandParams> c2 = ch; c2[bi] = hi;                  // soglia sopra il segnale → nessun intervento
            Engine e3; e3.prepare (fs, 512); auto L3 = s.L, R3 = s.R; run (e3, c2, g, L3, R3, 512);
            std::vector<BandParams> c3 = ch; c3[bi].dyn = false; c3[bi].gain = 0; Engine e4; e4.prepare (fs, 512); auto L4 = s.L, R4 = s.R; run (e4, c3, g, L4, R4, 512);
            ok (db (std::max (maxDiff (L3, L4), maxDiff (R3, R4))) < -90, s.name + ": soglia 0 dB (sopra il segnale) = banda ferma, residuo " + std::to_string ((int) db (std::max (maxDiff (L3, L4), maxDiff (R3, R4)))) + " dBFS");
        }
    }

    // 8. collocazione: Mid lascia intatto il Side, Left lascia intatto il Right
    { const Stem& s = stems[3]; Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; run (e, { band (Bell, 800, 9, 1, 12, Mid) }, {}, L, R, 512);
      double ds = 0, dm = 0; for (size_t i = 0; i < L.size(); ++i) { ds = std::max (ds, std::abs (0.5 * ((double) L[i] - R[i]) - 0.5 * ((double) s.L[i] - s.R[i]))); dm = std::max (dm, std::abs (0.5 * ((double) L[i] + R[i]) - 0.5 * ((double) s.L[i] + s.R[i]))); }
      ok (db (ds) < -100 && db (dm) > -40, "MELODY: Bell +9 dB solo su MID → Side intatto (" + std::to_string ((int) db (ds)) + " dBFS), Mid cambiato (" + std::to_string ((int) db (dm)) + " dBFS)");
      Engine e2; e2.prepare (fs, 512); auto L2 = s.L, R2 = s.R; run (e2, { band (HighShelf, 3000, 6, .7, 12, Left) }, {}, L2, R2, 512);
      ok (maxDiff (R2, s.R) == 0 && maxDiff (L2, s.L) > 1e-3, "MELODY: shelf solo su LEFT → Right identico al bit, Left cambiato"); }

    // 9. automazione aggressiva: gain che salta ±12 dB ogni blocco → niente click (derivata limitata)
    { const Stem& s = stems[2]; Engine e; e.prepare (fs, 256); auto L = s.L, R = s.R; std::mt19937 rng (7); std::uniform_real_distribution<double> u (-12, 12);
      for (size_t i0 = 0; i0 < L.size(); i0 += 256) { e.setBand (0, band (Bell, 1000, u (rng), 1)); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, (int) std::min ((size_t) 256, L.size() - i0)); }
      double din = 0, dout = 0; for (size_t i = 1; i < L.size(); ++i) { din = std::max (din, (double) std::abs (s.L[i] - s.L[i - 1])); dout = std::max (dout, (double) std::abs (L[i] - L[i - 1])); }
      ok (finite (L) && dout < din * 4.0, "VOCAL: gain automatizzato ±12 dB ogni 256 campioni, salto max uscita " + std::to_string (dout).substr (0, 6) + " vs ingresso " + std::to_string (din).substr (0, 6) + " (≤ ×4 = guadagno max)"); }

    // 10. sample rate da 44.1 a 192 kHz con parametri estremi: sempre stabile e finito
    { bool all = true; for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }) { Engine e; e.prepare (sr, 512); auto L = stems[0].L, R = stems[0].R;
        std::vector<BandParams> ex = { band (Bell, 10, 30, 40), band (Bell, 30000, -30, .025), band (LowCut, 20000, 0, 10, 96), band (HighCut, 12, 0, .1, 96), band (Notch, 19000, 0, 40), dyn (band (HighShelf, 15000, 30, 3), -60, 24, DetPeak), band (AllPass, 50, 0, 40), band (TiltShelf, 20, -30, 1) };
        run (e, ex, {}, L, R, 512); if (! finite (L) || ! finite (R)) all = false; }
      ok (all, "parametri estremi (Q 0.025/40, ±30 dB, 96 dB/oct, 10 Hz/30 kHz) a 44.1–192 kHz: uscita sempre finita"); }

    // 11. silenzio e denormali: lo stem HH-1 è tutto silenzio
    { const Stem& s = stems[5]; Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; auto t0 = std::chrono::steady_clock::now(); run (e, chainFor ("KICK"), {}, L, R, 512); const double ts = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
      Engine e2; e2.prepare (fs, 512); auto L2 = stems[0].L, R2 = stems[0].R; t0 = std::chrono::steady_clock::now(); run (e2, chainFor ("KICK"), {}, L2, R2, 512); const double tl = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
      ok (maxAbs (L) == 0 && maxAbs (R) == 0, "HH-1 (silenzio): uscita esattamente zero");
      ok (ts < tl * 3, "silenzio non rallenta (denormali): " + std::to_string (ts * 1000).substr (0, 5) + " ms vs audio " + std::to_string (tl * 1000).substr (0, 5) + " ms"); }

    // 12. CPU: 32 bande, metà dinamiche, sullo stem più lungo
    { std::vector<BandParams> many; for (int i = 0; i < 32; ++i) { auto b = band (i % 3 == 0 ? Bell : i % 3 == 1 ? HighShelf : Bell, 30 * std::pow (2, i * 9.0 / 31), (i % 2 ? 3 : -3), 1.5); if (i % 2 == 0) b = dyn (b, -30, -6); many.push_back (b); }
      Engine e; e.prepare (fs, 512); auto L = stems[2].L, R = stems[2].R; auto t0 = std::chrono::steady_clock::now(); run (e, many, {}, L, R, 512);
      const double t = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count(), rt = (n / fs) / t;
      ok (finite (L) && rt > 10, "32 bande (16 dinamiche) stereo a 48 kHz: " + std::to_string (rt).substr (0, 5) + "× più veloce del tempo reale (un core)"); }

    // 13. mix completo: tutti gli stem sommati, catena da bus, 10 minuti in loop
    { std::vector<float> L ((size_t) n), R ((size_t) n); for (auto& s : stems) for (size_t i = 0; i < (size_t) n; ++i) { L[i] += s.L[i]; R[i] += s.R[i]; }
      std::vector<BandParams> bus = { band (LowCut, 28, 0, .707, 24), dyn (band (Bell, 120, 0, 1), -24, -3), band (Bell, 400, -1.5, 1), dyn (band (Bell, 5000, 0, 2), -30, -3), band (HighShelf, 12000, 1.5, .7) };
      Engine e; e.prepare (fs, 512); double rms0 = 0, rmsN = 0; bool fin = true; std::vector<float> l2, r2;
      for (int loop = 0; loop < 28; ++loop) { l2 = L; r2 = R; run (e, bus, {}, l2, r2, 512); if (! finite (l2) || ! finite (r2)) fin = false;
        double s2 = 0; for (float v : l2) s2 += (double) v * v; s2 = std::sqrt (s2 / (double) n); if (loop == 1) rms0 = s2; if (loop == 27) rmsN = s2; }
      ok (fin && std::abs (db (rmsN) - db (rms0)) < 0.01, "mix di tutti gli stem, catena bus, 28 giri (~10 min): stabile, RMS giro 2 vs 28 diff " + std::to_string (std::abs (db (rmsN) - db (rms0))).substr (0, 6) + " dB"); }

    // 14. SOLO: si sente solo la zona della banda
    { const Stem& s = stems[2]; Engine e; e.prepare (fs, 512); auto L = s.L, R = s.R; Engine::Global g; g.solo = 0; run (e, { band (Bell, 3000, 0, 4) }, g, L, R, 512);
      std::vector<double> fr, mg, co; measured (s.L, L, fs, fr, mg, co); double at3k = 0, at200 = 0;
      for (size_t k = 0; k < fr.size(); ++k) { if (std::abs (fr[k] - 3000) < 6) at3k = db (mg[k]); if (std::abs (fr[k] - 200) < 6) at200 = db (mg[k]); }
      ok (at3k > -1 && at200 < -25, "SOLO banda 3 kHz Q4: 3 kHz passa (" + std::to_string (at3k).substr (0, 5) + " dB), 200 Hz tagliato (" + std::to_string (at200).substr (0, 6) + " dB)"); }

    std::printf ("\nRISULTATO: %d PASS, %d FAIL\n", passes, fails);
    return fails ? 1 : 0;
}
