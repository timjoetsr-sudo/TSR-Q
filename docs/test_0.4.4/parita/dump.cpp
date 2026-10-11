#include "TsrqDsp.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace tsrq;
// argomenti: stem.f32 out.f32 n blocco, poi bande come: tipo f g q slope place [thr range]  (tipo = indice enum)
int main (int argc, char** argv) {
    const int n = std::atoi (argv[3]), bl = std::atoi (argv[4]); std::vector<float> L ((size_t) n), R ((size_t) n);
    std::ifstream f (argv[1], std::ios::binary); f.read ((char*) L.data(), n * 4); f.read ((char*) R.data(), n * 4);
    std::vector<BandParams> ch; for (int a = 5; a < argc; ) { BandParams b; b.used = true; b.type = std::atoi (argv[a]); b.f = std::atof (argv[a+1]); b.gain = std::atof (argv[a+2]); b.q = std::atof (argv[a+3]); b.slope = std::atoi (argv[a+4]); b.place = std::atoi (argv[a+5]);
        int dynf = std::atoi (argv[a+6]); if (dynf) { b.dyn = true; b.thr = std::atof (argv[a+7]); b.range = std::atof (argv[a+8]); b.det = dynf == 2 ? DetPeak : DetRMS; if (dynf == 2 || dynf == 3) { b.att = 0.5; b.rel = 40; } } a += 9; ch.push_back (b); }
    Engine e; e.prepare (48000, 4096); Engine::Global g; e.setGlobal (g);
    for (size_t i0 = 0; i0 < (size_t) n; i0 += (size_t) bl) { for (size_t i = 0; i < ch.size(); ++i) e.setBand ((int) i, ch[i]); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, (int) std::min ((size_t) bl, (size_t) n - i0)); }
    std::ofstream o (argv[2], std::ios::binary); o.write ((char*) L.data(), n * 4); o.write ((char*) R.data(), n * 4);
}
