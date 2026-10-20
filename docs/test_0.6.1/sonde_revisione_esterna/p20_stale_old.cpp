// banda rimossa in Linear Phase (copia di dissolvenza old[] mai avanzata) e poi ritorno a Zero Latency: la banda rimossa si risente?
#include "common.h"
int main(){ const double fs=48000; auto mk=[&](std::vector<float>& l,std::vector<float>& r,int n){ l.resize(n); r.resize(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*1000*i/fs); };
 Engine e; e.prepare(fs,512); e.configurePhase(1,1); Engine::Global g; e.setGlobal(g); BandParams b=band(Bell,1000,12,1); e.setBand(0,b);
 std::vector<float> l,r; mk(l,r,512*200); for(int i=0;i<512*200;i+=512){ if(i==512*100){ b.used=false; } e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); }
 e.configurePhase(0,1);   // ritorno a Zero Latency (come fa il timer, elaborazione sospesa)
 std::vector<float> a,ar; mk(a,ar,512*20); for(int i=0;i<512*20;i+=512){ e.setBand(0,b); e.process(a.data()+i,ar.data()+i,nullptr,nullptr,512); }
 Engine f; f.prepare(fs,512); f.configurePhase(1,1); f.setGlobal(g); f.setBand(0,b); std::vector<float> l2,r2; mk(l2,r2,512*200); for(int i=0;i<512*200;i+=512){ f.setBand(0,b); f.process(l2.data()+i,r2.data()+i,nullptr,nullptr,512);} f.configurePhase(0,1);
 std::vector<float> c,cr; mk(c,cr,512*20); for(int i=0;i<512*20;i+=512){ f.setBand(0,b); f.process(c.data()+i,cr.data()+i,nullptr,nullptr,512); }
 for(int w=0;w<12;++w){ double pa=0,pc=0; for(int i=w*240;i<(w+1)*240;++i){ pa=std::max(pa,(double)std::abs(a[i])); pc=std::max(pc,(double)std::abs(c[i])); } std::printf("%5.0f-%3.0f ms: con banda rimossa in LP %.4f, riferimento senza banda %.4f (%+.2f dB)\n",w*5.0,(w+1)*5.0,pa,pc,20*std::log10((pa+1e-9)/(pc+1e-9))); } }
