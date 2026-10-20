// CPU: 8 bande (come cpu_linear_phase.cpp), blocco 512 e 64, Zero Latency e Linear Phase; media per blocco e picco (99,9 percentile e massimo) per chiamata da 64
#include "common.h"
#include <algorithm>
int main(){ const double fs=48000; for(int mode=0;mode<2;++mode) for(int res=0;res<(mode?5:1);++res) for(int ms=0;ms<2;++ms){ 
  double avg512=1e9; std::vector<double> t64;
  for(int blk:{512,64}){ Engine e; e.prepare(fs,blk); e.setLinSync(false); e.configurePhase(mode,res); Engine::Global gl; e.setGlobal(gl);
   BandParams bs[8]={band(LowCut,30,0,1,24),band(Bell,250,-3,1),band(Bell,3000,2,1),band(HighShelf,10000,2.5,1,12),band(Bell,80,2,.8),band(Bell,1200,-1.5,2),band(LowShelf,120,1,1,12),band(HighCut,18000,0,1,12)}; if(ms) bs[5].place=Mid;
   for(int i=0;i<8;++i) e.setBand(i,bs[i]); const int n=(int)fs*6/512*512; std::vector<float> L(n),R(n); std::mt19937 rg(1); std::uniform_real_distribution<float> u(-.3f,.3f); for(int i=0;i<n;++i){L[i]=u(rg);R[i]=u(rg);}
   for(int rep=0;rep<3;++rep){ std::vector<float> a=L,b=R; double tot=0; for(int i=0;i<n;i+=blk){ for(int k=0;k<8;++k) e.setBand(k,bs[k]); auto t0=std::chrono::steady_clock::now(); e.process(a.data()+i,b.data()+i,nullptr,nullptr,blk); double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count(); if(i>n/6){ tot+=us; if(blk==64&&rep==2) t64.push_back(us);} }
     if(blk==512) avg512=std::min(avg512,tot/((n-n/6)/512.0)); } }
  std::sort(t64.begin(),t64.end()); double p999=t64[(size_t)(t64.size()*0.999)], mx=t64.back(), med=t64[t64.size()/2];
  std::printf("%-12s %-9s %-6s: %6.1f us / blocco 512; blocco 64: mediana %5.1f us, 99.9%% %6.1f us, massimo %6.1f us (scadenza 1333 us)\n",mode?"Linear Phase":"Zero Latency",mode?std::vector<const char*>{"Low","Medium","High","VeryHigh","Maximum"}[res]:"-",ms?"Mid":"Stereo",avg512,med,p999,mx); } }
