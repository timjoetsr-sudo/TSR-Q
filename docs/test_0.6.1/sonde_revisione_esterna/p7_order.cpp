// banda dinamica (indice 0) seguita da banda statica di +12 dB (indice 1): livello a 1 kHz in Zero Latency e in Linear Phase
#include "common.h"
static double run(int mode,bool scExt){ const double fs=48000,f0=1000; Engine e; e.prepare(fs,512); e.configurePhase(mode,1); Engine::Global g; e.setGlobal(g);
 BandParams d=band(Bell,f0,0,1); d.dyn=true; d.thr=-30; d.range=-12; d.knee=0; d.sc=scExt; BandParams s=band(Bell,f0,12,1); e.setBand(0,d); e.setBand(1,s);
 const int n=512*190; std::vector<float> l(n),r(n),sl(n),sr(n); for(int i=0;i<n;++i){ l[i]=r[i]=sl[i]=sr[i]=0.1f*std::sin(2*kPi*f0*i/fs);} 
 for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,scExt?sl.data()+i:nullptr,scExt?sr.data()+i:nullptr,512);
 double pk=0; for(int i=n-4800;i<n;++i) pk=std::max(pk,(double)std::abs(l[i])); return 20*std::log10(pk/0.1); }
int main(){ std::printf("Zero Latency : %+.2f dB\nLinear Phase : %+.2f dB\n",run(0,false),run(1,false)); std::printf("con sidechain esterno = ingresso: ZL %+.2f dB, LP %+.2f dB\n",run(0,true),run(1,true)); }
