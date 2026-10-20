// modulo TSR Q (Zero Latency, progetto preciso) e modulo analogico a 192 kHz, frequenze F di proq4_192k.npz
#include "common.h"
#include <fstream>
int main(int argc,char**argv){ double fs=atof(argv[1]); int ana=atoi(argv[2]); std::ifstream cf("cases.txt"), ff("F.txt"); std::vector<double> F; double v; while(ff>>v) F.push_back(v);
  Grid g; g.init(fs); int t; double gn,q; int sl;
  while(cf>>t>>gn>>q>>sl){ BandParams b; b.used=true; b.type=t; b.f=1000; b.gain=gn; b.q=q; b.slope=sl; BandCoefs bc; designBand(b,gn,g,bc); SecSet s; sectionsFor(b,gn,s);
    for(double fr:F){ double m=1; if(ana) m=anaMag2(s,fr); else for(int k=0;k<bc.n;++k) m*=coefMag2(bc.c[k],fr,fs); std::printf("%.5f ",10*std::log10(std::max(m,1e-300))); } std::printf("\n"); } }
