// salto di parametri a regime: picco d'uscita durante i 30 ms dopo il salto rispetto al massimo dei due regimi (stesso segnale)
#include ENGINE
#include <cstdio>
#include <vector>
#include <random>
using namespace tsrq;
static BandParams B (int t, double f, double g, double q, int sl = 12) { BandParams b; b.used = true; b.type = t; b.f = f; b.gain = g; b.q = q; b.slope = sl; return b; }
static double run (double fs, BandParams a, BandParams b, int mode, double tf, int sig, double& pkSteady) {   // mode 0 = solo a, 1 = solo b, 2 = salto a->b a 1 s
  Engine e; e.prepare (fs, 512); Engine::Global gl; e.setGlobal (gl); e.setBand (0, mode == 1 ? b : a);
  const int n = (int) (fs * 1.5); std::vector<float> L (n), R (n); std::mt19937 rg (3); std::normal_distribution<float> nd (0, 0.05f);
  for (int i = 0; i < n; ++i) L[i] = R[i] = sig ? nd (rg) : (float) (0.25 * std::sin (2 * kPi * tf * i / fs));
  const int j = (int) fs; for (int i = 0; i < n; i += 64) { if (mode == 2 && i == j) e.setBand (0, b); e.process (L.data() + i, R.data() + i, nullptr, nullptr, std::min (64, n - i)); }
  double pk = 0; for (int i = j; i < j + (int) (0.03 * fs); ++i) pk = std::max (pk, (double) std::abs (L[i])); return 20 * std::log10 (pk + 1e-30); }
int main() { struct J { double fs; BandParams a, b; double tf; const char* nm; };
  J js[] = { { 192000, B (HighCut, 20000, 0, .71, 48), B (HighCut, 200, 0, .71, 48), 100, "HC48 20k->200 @192k, tono 100 Hz" },
             { 48000, B (HighCut, 20000, 0, .71, 48), B (HighCut, 200, 0, .71, 48), 100, "HC48 20k->200 @48k, tono 100 Hz" },
             { 48000, B (HighCut, 5000, 0, .71, 48), B (HighCut, 500, 0, .71, 48), 200, "HC48 5k->500, tono 200 Hz" },
             { 48000, B (LowCut, 30, 0, .71, 48), B (LowCut, 300, 0, .71, 48), 2000, "LC48 30->300, tono 2 kHz" },
             { 48000, B (LowCut, 300, 0, .71, 48), B (LowCut, 30, 0, .71, 48), 2000, "LC48 300->30, tono 2 kHz" },
             { 48000, B (BandPass, 100, 0, 10, 12), B (BandPass, 10000, 0, 10, 12), 1000, "BP12 Q10 100->10k, tono 1 kHz" },
             { 48000, B (HighShelf, 1000, 12, 2), B (HighShelf, 15000, 12, 2), 6000, "HS +12 Q2 1k->15k, tono 6 kHz" },
             { 48000, B (Bell, 100, 18, 10), B (Bell, 8000, 18, 10), 1000, "Bell +18 Q10 100->8k, tono 1 kHz" },
             { 48000, B (HighCut, 2000, 0, .71, 48), B (HighCut, 1000, 0, .71, 48), 300, "HC48 2k->1k, tono 300 Hz" } };
  for (auto& x : js) for (int sig : { 0, 1 }) { double d; const double sa = run (x.fs, x.a, x.b, 0, x.tf, sig, d), sb = run (x.fs, x.a, x.b, 1, x.tf, sig, d), sj = run (x.fs, x.a, x.b, 2, x.tf, sig, d);
    double path = -1e9; if (! sig) { Grid g; g.init (x.fs); for (int k = 0; k <= 200; ++k) { const double t = k / 200.0; BandParams p = x.b; p.f = x.a.f * std::pow (x.b.f / x.a.f, t); p.q = x.a.q * std::pow (x.b.q / x.a.q, t); p.gain = x.a.gain + (x.b.gain - x.a.gain) * t; BandCoefs c; designBand (p, p.gain, g, c, true); path = std::max (path, 10 * std::log10 (bandMag2 (c, x.tf, x.fs)) + 20 * std::log10 (0.25)); } }
    const double ref = sig ? std::max (sa, sb) : std::max ({ sa, sb, path });
    std::printf ("%-36s %s  regimi %7.2f / %7.2f  max sul percorso %7.2f  salto %7.2f dBFS  eccesso %+6.2f dB\n", x.nm, sig ? "rumore" : "tono  ", sa, sb, sig ? NAN : path, sj, sj - ref); } }
