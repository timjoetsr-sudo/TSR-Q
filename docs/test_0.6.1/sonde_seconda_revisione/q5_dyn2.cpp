// due bande dinamiche in serie (indice 0 espande +12 dB, indice 1 comprime): ZL contro LP; e banda dinamica dopo un All Pass / dopo una statica Mid
#include "common.h"
static double run(int mode,int cfg){ const double fs=48000,f0=1000; Engine e; e.prepare(fs,512); e.configurePhase(mode,1); Engine::Global g; e.setGlobal(g);
 BandParams a=band(Bell,f0,0,1), b=band(Bell,f0,0,1);
 if(cfg==0){ a.dyn=true; a.thr=-40; a.range=12; a.knee=0; b.dyn=true; b.thr=-20; b.range=-12; b.knee=0; }
 if(cfg==1){ a=band(Bell,f0,12,1,12,Mid); b.dyn=true; b.thr=-25; b.range=-12; b.knee=0; }   // statica Mid poi dinamica Stereo
 if(cfg==2){ a=band(Bell,f0,12,1,12,Left); b.dyn=true; b.thr=-25; b.range=-12; b.knee=0; b.place=Side; }
 e.setBand(0,a); e.setBand(1,b); const int n=512*190; std::vector<float> l(n),r(n); for(int i=0;i<n;++i){ l[i]=0.1f*std::sin(2*kPi*f0*i/fs); r[i]=(cfg==2?0.3f:1.0f)*l[i]; }
 for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); double pk=0; for(int i=n-4800;i<n;++i) pk=std::max(pk,(double)std::abs(l[i])); return 20*std::log10(pk/0.1); }
int main(){ const char* c[]={"dinamica +12 poi dinamica -12","statica Mid +12 poi dinamica","statica Left +12 poi dinamica Side"}; for(int k=0;k<3;++k) std::printf("%-36s: ZL %+.2f dB, LP %+.2f dB\n",c[k],run(0,k),run(1,k)); }
