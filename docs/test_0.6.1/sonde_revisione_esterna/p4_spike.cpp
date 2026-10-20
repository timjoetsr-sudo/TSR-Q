// costo massimo di una singola chiamata process() (blocco host piccolo) in Linear Phase: picco contro scadenza del blocco
#include "common.h"
int main(){ for(double fs:{48000.0,96000.0,192000.0}) for(int res=0;res<5;++res) for(int ms=0;ms<2;++ms){ const int blk=64;
  Engine e; e.prepare(fs,blk); e.setLinSync(false); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
  BandParams bs[8]={band(LowCut,30,0,1,24),band(Bell,250,-3,1),band(Bell,3000,2,1),band(HighShelf,10000,2.5,1,12),band(Bell,80,2,.8),band(Bell,1200,-1.5,2),band(LowShelf,120,1,1,12),band(HighCut,18000,0,1,12)}; if(ms) bs[5].place=Mid;
  for(int i=0;i<8;++i) e.setBand(i,bs[i]);
  const int n=(int)fs*4/blk*blk; std::vector<float> l(n),r(n); std::mt19937 rg(1); std::uniform_real_distribution<float> u(-.3f,.3f); for(int i=0;i<n;++i){l[i]=u(rg);r[i]=u(rg);}
  double mx=0,sum=0,mxAut=0; int cnt=0; int lat=e.latencySamples();
  for(int i=0;i<n;i+=blk){ bool aut=i>n/2; if(aut){ BandParams b=bs[1]; b.gain=-3+3*std::sin(i*1e-4); e.setBand(1,b);} auto t0=std::chrono::steady_clock::now(); e.process(l.data()+i,r.data()+i,nullptr,nullptr,blk); double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count(); if(i>n/8){ if(aut) mxAut=std::max(mxAut,us); else {mx=std::max(mx,us); sum+=us; ++cnt;} } }
  const double dl=blk/fs*1e6; std::printf("fs %6.0f res %d %-6s lat %6d: media %6.1f us, picco fermo %7.1f us, picco in automazione %7.1f us, scadenza blocco 64 = %6.1f us -> picco/scadenza %.2f\n",fs,res,ms?"Mid":"Stereo",lat,sum/cnt,mx,mxAut,dl,std::max(mx,mxAut)/dl); } }
