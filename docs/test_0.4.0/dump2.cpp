#include "/home/claude/tsr-q/Source/TsrqDsp.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace tsrq;
// argomenti: stem.f32 out.f32 n blocco char out pan panMS invert gq, poi bande: tipo f g q slope place dyn thr range
int main (int argc, char** argv) {
    const int n = std::atoi (argv[3]), bl = std::atoi (argv[4]); std::vector<float> L ((size_t) n), R ((size_t) n);
    std::ifstream f (argv[1], std::ios::binary); f.read ((char*) L.data(), n * 4); f.read ((char*) R.data(), n * 4);
    Engine::Global g; g.character = std::atoi (argv[5]); g.outDb = std::atof (argv[6]); g.pan = std::atof (argv[7]); g.panMS = std::atoi (argv[8]); g.invert = std::atoi (argv[9]); g.gq = std::atoi (argv[10]);
    std::vector<BandParams> ch; for (int a = 11; a < argc; ) { BandParams b; b.used = true; b.type = std::atoi (argv[a]); b.f = std::atof (argv[a+1]); b.gain = std::atof (argv[a+2]); b.q = std::atof (argv[a+3]); b.slope = std::atoi (argv[a+4]); b.place = std::atoi (argv[a+5]);
        int dynf = std::atoi (argv[a+6]); if (dynf) { b.dyn = true; b.thr = std::atof (argv[a+7]); b.range = std::atof (argv[a+8]); } b.gq = g.gq; a += 9; ch.push_back (b); }
    Engine e; e.prepare (48000, 4096); e.setGlobal (g);
    for (size_t i0 = 0; i0 < (size_t) n; i0 += (size_t) bl) { for (size_t i = 0; i < ch.size(); ++i) e.setBand ((int) i, ch[i]); e.process (L.data() + i0, R.data() + i0, nullptr, nullptr, (int) std::min ((size_t) bl, (size_t) n - i0)); }
    std::ofstream o (argv[2], std::ios::binary); o.write ((char*) L.data(), n * 4); o.write ((char*) R.data(), n * 4);
}
