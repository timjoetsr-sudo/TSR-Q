// LP: banda statica +12 dB rimossa, 2 s dopo la stessa banda riattivata come dinamica a 0 dB (filtro piatto): la vecchia +12 dB riappare?
#include "common.h"
int main(){ const double fs=48000; Engine e; e.prepare(fs,512); e.configurePhase(1,1); Engine::Global g; e.setGlobal(g); BandParams b=band(Bell,1000,12,1); e.setBand(0,b);
 int n=512*400; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*1000*i/fs);
 for(int i=0;i<n;i+=512){ if(i==512*100) b.used=false; if(i==512*300){ b.used=true; b.gain=0; b.dyn=true; b.thr=0; } e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); }
 for(int w=-2;w<8;++w){ int s=512*300+w*240; double pk=0; for(int i=s;i<s+240;++i) pk=std::max(pk,(double)std::abs(l[i])); std::printf("%+4d ms: %+6.2f dB (atteso 0.00)\n",w*5,20*std::log10(pk/0.1)); } }
