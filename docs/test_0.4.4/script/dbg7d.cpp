#include "TsrqDsp.h"
#include <cstdio>
#include <vector>
using namespace tsrq;
// energia sopra 6 kHz (blocchi da 64 campioni) con una banda dinamica: sempre accesa, sempre accesa con livello che oscilla, e on/off ogni 0.25 s
static int pkAt = 0;
static double hf (std::vector<float>& L) { Engine hp; hp.prepare (48000, 4096); Engine::Global g; hp.setGlobal (g); BandParams b; b.used = true; b.type = LowCut; b.f = 6000; b.slope = 96; b.q = .71; hp.setBand (0, b);
  std::vector<float> R (L); hp.process (L.data(), R.data(), nullptr, nullptr, (int) L.size()); double pk = 0; for (size_t i0 = 4800; i0 + 64 <= L.size(); i0 += 64) { double s = 0; for (size_t i = i0; i < i0 + 64; ++i) s += L[i] * L[i]; if (s / 64 > pk) { pk = s / 64; pkAt = (int) i0; } } return 10 * std::log10 (pk / 0.0338 + 1e-30); }
int main (int argc, char** argv) { const double off = argc > 1 ? atof (argv[1]) : 0, offp = argc > 2 ? atof (argv[2]) : 0; const double fs = 48000; const int n = 48000 * 6, bl = 256;
  for (int mode = 0; mode < 5; ++mode) {
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) { const double t = i / fs, env = 0.6 + 0.4 * std::sin (2 * kPi * 0.5 * t);
      const double v = env * (0.3 * std::sin (2 * kPi * 110 * t) + 0.25 * std::sin (2 * kPi * 220 * t + 1) + 0.2 * std::sin (2 * kPi * 330 * t + 2) + 0.15 * std::sin (2 * kPi * 440 * t) + 0.1 * std::sin (2 * kPi * 660 * t) + 0.08 * std::sin (2 * kPi * 880 * t) + 0.05 * std::sin (2 * kPi * 1320 * t)); L[i] = (float) v; R[i] = (float) (0.8 * v); }
    Engine e; e.prepare (fs, 4096); Engine::Global g; BandParams b; b.used = true; b.type = Bell; b.f = 500; b.gain = 6; b.q = 2; b.thr = -40; b.range = -1.5; b.dyn = mode != 0;
    for (int i0 = 0; i0 < n; i0 += bl) { const int ph = (i0 / 12000) % 2; if (mode == 2) b.dyn = ph; if (mode == 1) { b.thr = -20 + off + 6 * std::sin (2 * kPi * i0 / 24000.0); } if (mode == 3) b.thr = -20 + off; if (mode == 4) { b.thr = -20 + offp; b.det = 1; }
      e.setGlobal (g); e.setBand (0, b); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0)); }
    double dsum = 0; (void) dsum; printf ("%s: energia sopra 6 kHz %.1f dB\n", mode == 0 ? "statica                         " : mode == 1 ? "dinamica sempre accesa, soglia mobile" : mode == 2 ? "dinamica on/off ogni 0,25 s     " : mode == 3 ? "dinamica RMS, soglia fissa -20   " : "dinamica Peak, soglia fissa -20  ", hf (L)); printf ("   massimo a t = %.4f s (fase nel ciclo on/off da 0,5 s: %.4f s)\n", pkAt / fs, std::fmod (pkAt / fs, 0.5)); } }
