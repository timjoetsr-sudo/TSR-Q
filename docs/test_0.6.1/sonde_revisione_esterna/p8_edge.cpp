// Linear Phase: fs estreme, blocco 1 contro 512 contro 8192, ingressi NaN/Inf/enormi/denormali, tempo di elaborazione con denormali
#include "common.h"
#include <cfenv>
static std::vector<float> runb(double fs,int res,int blk,const std::vector<float>& in, std::vector<float>* outR=nullptr){ Engine e; e.prepare(fs,std::max(blk,32)); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
 e.setBand(0,band(Bell,300,6,2)); e.setBand(1,band(LowCut,40,0,1,24,Mid)); e.setBand(2,band(HighShelf,std::min(8000.0,fs*0.3),-4,1,12,Side));
 std::vector<float> l=in, r(in.size()); for(size_t i=0;i<in.size();++i) r[i]=in[(i*7)%in.size()];
 for(size_t i=0;i<in.size();i+=blk) e.process(l.data()+i,r.data()+i,nullptr,nullptr,(int)std::min((size_t)blk,in.size()-i)); if(outR) *outR=r; return l; }
int main(){ for(double fs:{22050.0,44100.0,384000.0}){ int res=fs>300000?4:2; int lat; { Engine e; e.prepare(fs,64); e.configurePhase(1,res); lat=e.latencySamples(); }
  const int n=lat+20000; std::vector<float> in(n); std::mt19937 rg(5); std::uniform_real_distribution<float> u(-.5f,.5f); for(auto&v:in) v=u(rg);
  auto a=runb(fs,res,512,in), b=runb(fs,res,1,in), c=runb(fs,res,8192,in); double d1=0,d2=0; bool fin=true; for(int i=0;i<n;++i){ d1=std::max(d1,(double)std::abs(a[i]-b[i])); d2=std::max(d2,(double)std::abs(a[i]-c[i])); fin=fin&&std::isfinite(a[i]); }
  std::printf("fs %6.0f res %d lat %6d: blocco1 vs 512 max diff %.3g, 8192 vs 512 %.3g, finito %d\n",fs,res,lat,d1,d2,(int)fin); }
 // ingressi estremi
 { double fs=48000; Engine e; e.prepare(fs,512); e.configurePhase(1,1); Engine::Global g; e.setGlobal(g); e.setBand(0,band(Bell,100,12,4)); e.setBand(1,band(LowCut,30,0,10,96,Side)); int lat=e.latencySamples(); int n=lat*3+512*20; n=n/512*512;
  std::vector<float> l(n,0),r(n,0); for(int i=0;i<n;++i){ l[i]=0.1f*std::sin(i*0.01f); r[i]=l[i]; } l[1000]=NAN; r[1001]=INFINITY; l[2000]=1e30f; r[3000]=-1e30f; l[4000]=999.f; for(int i=5000;i<5100;++i){ l[i]=1e-40f; }
  for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); bool fin=true; double mx=0; for(int i=0;i<n;++i){ fin=fin&&std::isfinite(l[i])&&std::isfinite(r[i]); mx=std::max(mx,(double)std::max(std::abs(l[i]),std::abs(r[i]))); } std::printf("ingressi NaN/Inf/1e30/999: uscita finita %d, picco %.1f\n",(int)fin,mx); }
 // silenzio dopo segnale: tempo per blocco (denormali senza FTZ)
 { double fs=48000; Engine e; e.prepare(fs,512); e.configurePhase(1,4); Engine::Global g; e.setGlobal(g); e.setBand(0,band(Bell,100,12,4)); int n=512*400; std::vector<float> l(n,0),r(n,0); for(int i=0;i<48000;++i) l[i]=r[i]=0.3f*std::sin(i*0.05f);
  double t1=0,t2=0; for(int i=0;i<n;i+=512){ auto t0=std::chrono::steady_clock::now(); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count(); if(i<48000) t1+=us; else if(i>48000*3) t2+=us; } int sub=0; for(int i=48000*3;i<n;++i) if(l[i]!=0&&std::abs(l[i])<1.18e-38f) ++sub;
  std::printf("Maximum: us/blocco con segnale %.1f, in silenzio dopo 3 s %.1f; campioni subnormali in uscita %d\n",t1/(48000/512),t2/((n-48000*3)/512),sub); } }
