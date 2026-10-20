#include "common.h"
int main(){ const double fs=48000, f0=1000; Engine e; e.prepare(fs,512); e.setLinSync(false); e.configurePhase(1,4); Engine::Global g; e.setGlobal(g);
  std::vector<float> l(512),r(512); long ph=0; auto blk=[&](){ for(int i=0;i<512;++i){ l[i]=r[i]=0.1f*std::sin(2*kPi*f0*(ph+i)/fs);} ph+=512; e.process(l.data(),r.data(),nullptr,nullptr,512); double pk=0; for(int i=0;i<512;++i) pk=std::max(pk,(double)std::abs(l[i])); return 20*std::log10(pk/0.1+1e-12); };
  BandParams b=band(Bell,f0,12,1); e.setBand(0,b); for(int k=0;k<200;++k){ blk(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  std::printf("assestato: %+.2f\n",blk());
  b.gain=6; e.setBand(0,b); blk(); b.gain=0; e.setBand(0,b); blk(); e.setLinSync(true); b.gain=-12; e.setBand(0,b);
  for(int k=0;k<200;++k){ double v=blk(); if(k%40==0||k==199) std::printf("offline blocco %d: %+.2f\n",k,v); }
  e.setLinSync(false); for(int k=0;k<1000;++k){ double v=blk(); if(k%100==0||k==999) std::printf("tempo reale blocco %d: %+.2f\n",k,v); std::this_thread::sleep_for(std::chrono::milliseconds(1)); } }
