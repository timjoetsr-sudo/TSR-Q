// costo deterministico del blocco di partizione: mediana dei tempi delle sole chiamate process() (blocco 64) che contengono un confine di partizione B
#include "common.h"
#include <algorithm>
int main(){ for(double fs:{48000.0,96000.0,192000.0}) for(int res=4;res<5;++res) for(int ms=0;ms<2;++ms){ const int blk=64;
  Engine e; e.prepare(fs,blk); e.setLinSync(false); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
  BandParams bs[8]={band(LowCut,30,0,1,24),band(Bell,250,-3,1),band(Bell,3000,2,1),band(HighShelf,10000,2.5,1,12),band(Bell,80,2,.8),band(Bell,1200,-1.5,2),band(LowShelf,120,1,1,12),band(HighCut,18000,0,1,12)}; if(ms) bs[5].place=Mid;
  for(int i=0;i<8;++i) e.setBand(i,bs[i]);
  const int L=LinPhase::firLen(res,fs), B=LinPhase::partLen(L); const int n=std::max((int)fs*3, B*40)/blk*blk; std::vector<float> l(n),r(n); std::mt19937 rg(1); std::uniform_real_distribution<float> u(-.3f,.3f); for(int i=0;i<n;++i){l[i]=u(rg);r[i]=u(rg);}
  std::vector<double> tb,to; for(int i=0;i<n;i+=blk){ { BandParams bb=bs[1]; bb.gain=-3+3*std::sin(i*1e-4); e.setBand(1,bb);} auto t0=std::chrono::steady_clock::now(); e.process(l.data()+i,r.data()+i,nullptr,nullptr,blk); double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count(); if(i<n/4) continue; bool cross=(i/B)!=((i+blk)/B); (cross?tb:to).push_back(us); }
  std::sort(tb.begin(),tb.end()); std::sort(to.begin(),to.end()); const double dl=blk/fs*1e6;
  std::printf("fs %6.0f res %d %-6s B %5d: chiamata con partizione mediana %7.1f us (%.2f x scadenza %5.1f us), altre chiamate mediana %5.1f us\n",fs,res,ms?"Mid":"Stereo",B,tb[tb.size()/2],tb[tb.size()/2]/dl,dl,to[to.size()/2]); } }
