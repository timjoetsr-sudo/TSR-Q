#include "TsrqDsp.h"
#include <cstdio>
#include <chrono>
#include <random>
using namespace tsrq;
static BandParams band(int t,double f,double g,double q,int sl=12,int pl=Stereo){BandParams b;b.used=1;b.type=t;b.f=f;b.gain=g;b.q=q;b.slope=sl;b.place=pl;return b;}
int main(){ for(double fs:{48000.0,96000.0}) for(int mode=0;mode<2;++mode) for(int res=0;res<(mode?5:1);++res) for(int ms=0;ms<2;++ms){
  Engine e; e.prepare(fs,512); e.configurePhase(mode,res); Engine::Global gl; e.setGlobal(gl);
  BandParams bs[8]={band(LowCut,30,0,1,24),band(Bell,250,-3,1),band(Bell,3000,2,1),band(HighShelf,10000,2.5,1),band(Bell,80,2,.8),band(Bell,1200,-1.5,2),band(LowShelf,120,1,1),band(HighCut,18000,0,1,12)};
  if(ms) bs[5].place=Mid;
  for(int i=0;i<8;++i) e.setBand(i,bs[i]);
  const int n=((int)fs*10)/512*512; std::vector<float> L(n),R(n); std::mt19937 rg(1); std::uniform_real_distribution<float> u(-.3f,.3f); for(int i=0;i<n;++i){L[i]=u(rg);R[i]=u(rg);}
  double best=1e9; for(int r=0;r<3;++r){ std::vector<float> a=L,b=R; auto t0=std::chrono::steady_clock::now(); for(int i=0;i<n;i+=512) e.process(a.data()+i,b.data()+i,nullptr,nullptr,512); best=std::min(best,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/(n/512)); }
  // costo di un riprogetto
  double dms=0; if(mode){ auto t0=std::chrono::steady_clock::now(); for(int k=0;k<5;++k){ BandParams b=bs[1]; b.gain=-3+k; e.setBand(1,b); std::vector<float> a(512),b2(512); e.process(a.data(),b2.data(),nullptr,nullptr,512);} dms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count()/5; }
  std::printf("fs %6.0f %-13s %-9s %s: %7.1f us per blocco 512 (%.2f%% di un core); blocco con riprogetto %.2f ms; latenza %d\n",fs,mode?"Linear Phase":"Zero Latency",mode?std::vector<const char*>{"Low","Medium","High","VeryHigh","Maximum"}[res]:"-",ms?"con banda Mid":"tutte Stereo",best,best/(512/fs*1e6)*100,dms,e.latencySamples()); }
}
