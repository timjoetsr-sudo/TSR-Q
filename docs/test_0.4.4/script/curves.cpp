// curve di risposta per il report: analogico (prototipo) e digitale del motore incluso, casi vicino a Nyquist + trasferimento del Character misurato sul motore
#include "TsrqDsp.h"
#include <cstdio>
#include <complex>
using namespace tsrq;
static double db (double x) { return 10 * std::log10 (std::max (x, 1e-30)); }
static double dmag (const BandCoefs& c, double f, double fs) { const std::complex<long double> z = std::polar ((long double) 1, (long double) (-2 * kPi * f / fs)), z2 = z * z; long double m = 1;
    for (int k = 0; k < c.n; ++k) { const auto& q = c.c[k]; m *= std::norm (((long double) q.b0 + (long double) q.b1 * z + (long double) q.b2 * z2) / ((long double) 1 + (long double) q.a1 * z + (long double) q.a2 * z2)); } return (double) m; }
int main() {
    struct C { const char* id; int t; double f, g, q; int sl; double fs; } cs[] = { { "bell18k", Bell, 18000, 12, 8, 12, 44100 }, { "notch20k", Notch, 20000, 0, 2, 12, 44100 }, { "hshelf16k", HighShelf, 16000, 18, 20, 12, 44100 },
        { "hcut15k", HighCut, 14811.369, 0, 0.71, 96, 44100 }, { "bell10k48", Bell, 10000, 12, 2, 12, 48000 } };
    std::printf ("{\"curves\": {");
    for (size_t k = 0; k < sizeof (cs) / sizeof (*cs); ++k) { const C& c = cs[k]; Grid g; g.init (c.fs); BandParams b; b.used = true; b.type = c.t; b.f = c.f; b.gain = c.g; b.q = c.q; b.slope = c.sl;
        SecSet set; sectionsFor (b, hasGain (c.t) ? c.g : 0, set); BandCoefs bc; designBand (b, hasGain (c.t) ? c.g : 0, g, bc);
        std::printf ("%s\"%s\": {\"fs\": %.0f, \"f\": [", k ? ", " : "", c.id, c.fs); const int N = 240;
        for (int i = 0; i < N; ++i) { const double f = 1000 * std::pow (20.0, i / (N - 1.0)); std::printf ("%s%.1f", i ? "," : "", f); } std::printf ("], \"ana\": [");
        for (int i = 0; i < N; ++i) { const double f = 1000 * std::pow (20.0, i / (N - 1.0)); std::printf ("%s%.3f", i ? "," : "", db (anaMag2 (set, f))); } std::printf ("], \"dig\": [");
        for (int i = 0; i < N; ++i) { const double f = 1000 * std::pow (20.0, i / (N - 1.0)); std::printf ("%s%.3f", i ? "," : "", f < c.fs / 2 ? db (dmag (bc, f, c.fs)) : -999.0); } std::printf ("]}"); }
    std::printf ("}, \"character\": {");
    for (int chm : { 1, 2 }) { const double fs = 48000; const int n = 48000; std::vector<float> L ((size_t) n), R ((size_t) n), X ((size_t) n); const double amp = 3.0;
        for (int i = 0; i < n; ++i) X[(size_t) i] = L[(size_t) i] = R[(size_t) i] = (float) (amp * std::sin (2 * kPi * 100 * i / fs));
        Engine e; e.prepare (fs, 512); Engine::Global gl; gl.character = chm; e.setGlobal (gl); for (int i0 = 0; i0 < n; i0 += 512) e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (512, n - i0));
        std::printf ("%s\"%s\": {\"x\": [", chm == 2 ? ", " : "", chm == 1 ? "subtle" : "warm"); const int a = n - 480 - 7;   // un periodo (480 campioni a 100 Hz), ritardo dei filtri 7 campioni
        for (int i = 0; i < 480; i += 2) std::printf ("%s%.4f", i ? "," : "", X[(size_t) (a + i)]); std::printf ("], \"y\": [");
        for (int i = 0; i < 480; i += 2) std::printf ("%s%.4f", i ? "," : "", L[(size_t) (a + i + 7)]); std::printf ("]}"); }
    std::printf ("}}\n");
}
