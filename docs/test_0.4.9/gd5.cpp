#include ENGINE
#include <cstdio>
#include <complex>
#include <vector>
using namespace tsrq;
int main() { const double fs = 48000; for (int ch : { 1, 2 }) { Engine e; e.prepare (fs, 512); Engine::Global gl; gl.character = ch; e.setGlobal (gl);
    const int N = 16384; std::vector<float> L (N, 0.f), R (N, 0.f); L[5000] = R[5000] = 0.01f; for (int i = 0; i < N; i += 512) e.process (L.data() + i, R.data() + i, nullptr, nullptr, 512);
    std::printf ("%s:", ch == 1 ? "Subtle" : "Warm"); auto H = [&] (double f) { std::complex<double> h (0); for (int i = 0; i < N; ++i) h += (double) L[i] / 0.01 * std::polar (1.0, -2 * kPi * f * (i - 5000) / fs); return h; };
    for (double f : { 20.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 }) { const double d = std::arg (H (f + 5) / H (f - 5)); std::printf ("  %g Hz: ritardo %.2f camp, mod %.3f dB", f, -d / (2 * kPi * 10) * fs, 20 * std::log10 (std::abs (H (f)))); } std::printf ("\n"); } }
