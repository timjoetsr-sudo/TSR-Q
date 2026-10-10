#include "TsrqDsp.h"
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
using namespace tsrq;
// uso: sc_h in.f32 out.f32 n scenario blocco
int main (int argc, char** argv) {
    const int n = std::atoi (argv[3]); std::string sc = argv[4]; const int bl = argc > 5 ? std::atoi (argv[5]) : 256; std::vector<float> L ((size_t) n), R ((size_t) n);
    std::ifstream f (argv[1], std::ios::binary); f.read ((char*) L.data(), n * 4); f.read ((char*) R.data(), n * 4);
    Engine e; e.prepare (48000, 4096); Engine::Global g;
    BandParams b; b.used = true; b.type = Bell; b.f = 500; b.gain = 6; b.q = 2;
    if (sc == "slope") { b.type = LowCut; b.f = 300; }
    for (int i0 = 0; i0 < n; i0 += bl) {
        const double u = (double) i0 / n; const int ph = (i0 / 24000) % 2, k = i0 / 24000;
        if (sc == "drag" || sc == "solo_drag") b.f = 200 * std::pow (10.0, u);
        if (sc == "solo_q") b.q = 0.3 * std::pow (10 / 0.3, u);
        g.solo = (sc.rfind ("solo", 0) == 0) ? 0 : -1;
        if (sc == "solo_toggle") g.solo = ph ? 0 : -1;
        if (sc == "bypass_band") b.bypass = ph;
        if (sc == "used") b.used = ! ph;
        if (sc == "type") { const int t[4] = { Bell, LowShelf, HighShelf, Notch }; b.type = t[k % 4]; b.f = 800; }
        if (sc == "slope") { const int s[3] = { 12, 24, 48 }; b.slope = s[k % 3]; }
        if (sc == "place") { const int p[3] = { Stereo, Mid, Side }; b.place = p[k % 3]; }
        if (sc == "dyn") { b.dyn = ph; b.thr = -40; b.range = -1.5; }
        if (sc == "gbypass") g.bypass = ph;
        if (sc == "character") g.character = ph ? 2 : 0;
        if (sc == "warm_static") g.character = 2;
        if (sc == "subtle_toggle") g.character = ph ? 1 : 0;
        if (sc == "sub_static") g.character = 1;
        if (sc == "outstep") g.outDb = ph ? -12 : 0;
        if (sc == "instep") g.inDb = ph ? -12 : 0;
        if (sc == "polarity") g.invert = ph;
        if (sc == "pan") g.pan = ph ? -1 : 0;
        e.setGlobal (g); e.setBand (0, b);
        e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, std::min (bl, n - i0));
    }
    std::ofstream o (argv[2], std::ios::binary); o.write ((char*) L.data(), n * 4); o.write ((char*) R.data(), n * 4);
}
