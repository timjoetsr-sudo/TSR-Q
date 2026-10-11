#include "TsrqDsp.h"
#include <cstdio>
#include <iostream>
using namespace tsrq;
// stdin: righe "fs type f gain q slope gq precise" -> coefficienti %.17g
int main() {
    double fs, f, g, q; int type, slope, gq, precise; Grid grid; double lastFs = -1;
    while (std::cin >> fs >> type >> f >> g >> q >> slope >> gq >> precise) {
        if (fs != lastFs) { grid.init (fs); lastFs = fs; }
        BandParams b; b.used = true; b.type = type; b.f = f; b.gain = g; b.q = q; b.slope = slope; b.gq = gq;
        BandCoefs c; designBand (b, hasGain (type) ? g : 0, grid, c, precise != 0);
        std::printf ("%d", c.n); for (int k = 0; k < c.n; ++k) std::printf (" %.17g %.17g %.17g %.17g %.17g", c.c[k].b0, c.c[k].b1, c.c[k].b2, c.c[k].a1, c.c[k].a2); std::printf ("\n");
    }
}
