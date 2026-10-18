// automazione continua: frequenza (log) spazzata da fA a fB in T secondi, aggiornata a ogni blocco da 64; picco d'uscita oltre il massimo sul percorso (tono) o oltre il massimo dei regimi (rumore)
#include ENGINE
#include <cstdio>
#include <vector>
#include <random>
using namespace tsrq;
static BandParams B (int t, double f, double g, double q, int sl = 12) { BandParams b; b.used = true; b.type = t; b.f = f; b.gain = g; b.q = q; b.slope = sl; return b; }
int main() { struct S { double fs; BandParams a; double fB; double tf; const char* nm; };
  S ss[] = { { 48000, B (HighCut, 20000, 0, .71, 48), 200, 100, "HC48 20k->200" }, { 192000, B (HighCut, 20000, 0, .71, 48), 200, 100, "HC48 20k->200 @192k" }, { 48000, B (LowCut, 20, 0, .71, 48), 2000, 5000, "LC48 20->2k" },
             { 48000, B (BandPass, 100, 0, 10, 12), 10000, 1000, "BP12 Q10 100->10k" }, { 48000, B (HighShelf, 1000, 12, 2), 15000, 6000, "HS+12 Q2 1k->15k" }, { 48000, B (Bell, 100, 18, 10), 8000, 1000, "Bell+18 Q10 100->8k" } };
  for (auto& x : ss) for (double T : { 0.02, 0.05, 0.1, 0.3, 1.0 }) { double worst = -1e9;
    for (int sig : { 0 }) { const int n = (int) (x.fs * (1.2 + T)); std::vector<float> L (n), R (n); std::mt19937 rg (3); std::normal_distribution<float> nd (0, 0.05f);
      for (int i = 0; i < n; ++i) L[i] = R[i] = sig ? nd (rg) : (float) (0.25 * std::sin (2 * kPi * x.tf * i / x.fs));
      Engine e; e.prepare (x.fs, 512); Engine::Global gl; e.setGlobal (gl); const int j = (int) x.fs; double pkS = 0;
      for (int i = 0; i < n; i += 64) { BandParams b = x.a; if (i >= j) { const double u = std::min (1.0, (i - j) / (T * x.fs)); b.f = x.a.f * std::pow (x.fB / x.a.f, u); } e.setBand (0, b); e.process (L.data() + i, R.data() + i, nullptr, nullptr, std::min (64, n - i)); }
      double pk = 0; for (int i = j; i < n; ++i) pk = std::max (pk, (double) std::abs (L[i])); for (int i = j / 2; i < j; ++i) pkS = std::max (pkS, (double) std::abs (L[i])); double pkE = 0; for (int i = n - (int) (0.1 * x.fs); i < n; ++i) pkE = std::max (pkE, (double) std::abs (L[i]));
      double ref; if (sig) ref = std::max (pkS, pkE); else { Grid g; g.init (x.fs); double m = 0; for (int k = 0; k <= 200; ++k) { BandParams b = x.a; b.f = x.a.f * std::pow (x.fB / x.a.f, k / 200.0); BandCoefs c; designBand (b, b.gain, g, c, true); m = std::max (m, bandMag2 (c, x.tf, x.fs)); } ref = 0.25 * std::sqrt (m); }
      worst = std::max (worst, 20 * std::log10 (pk / ref)); }
    std::printf ("%-22s in %5.0f ms: eccesso peggiore (tono/rumore) %+6.2f dB\n", x.nm, T * 1000, worst); } }
