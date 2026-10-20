// automazione continua: Bell 1 kHz Q1 guadagno 0 -> +12 dB in 1 s, sinusoide 1 kHz -20 dBFS. Energia spuria fuori da 1 kHz +-60 Hz (dB rispetto alla fondamentale)
#include "common.h"
static void spur(const std::vector<float>& x,int s0,int N,double fs,double f0,const char* tag){
  Fft f; f.init(N); std::vector<Cpx> a(N); for(int i=0;i<N;++i){ double w=0.5-0.5*std::cos(2*kPi*i/N); a[i]=x[s0+i]*w; } f.run(a.data(),false);
  double ef=0,es=0; for(int k=1;k<N/2;++k){ double fr=k*fs/N, p=std::norm(a[k]); if(std::abs(fr-f0)<60) ef+=p; else if(fr>20&&fr<20000) es+=p; }
  std::printf("%-44s spurie %.1f dB\n",tag,10*std::log10(es/ef)); }
int main(){ const double fs=48000,f0=1000; const int N=65536;
 for(int cfg=0;cfg<6;++cfg){ int mode=cfg==0?0:1, res=cfg<=2?1:(cfg==3?0:4); bool sync = cfg!=2 && cfg!=5; int blk=cfg==5?64:512;
  Engine e; e.prepare(fs,512); e.setLinSync(sync); e.configurePhase(mode,res); Engine::Global g; e.setGlobal(g);
  const int lat=e.latencySamples(); const int n=lat+N+48000; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs);
  for(int i=0;i<n;i+=blk){ double t=std::clamp((double)(i-lat-8000)/N,0.0,1.0); BandParams b=band(Bell,f0,12*t,1); e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,std::min(blk,n-i)); if(!sync) std::this_thread::sleep_for(std::chrono::microseconds(blk*1000000/48000/3)); }
  char tag[100]; std::snprintf(tag,100,"mode %d res %d %s blk %d",mode,res,sync?"sync":"async(rt 3x)",blk); spur(l,lat+8000,N,fs,f0,tag); }
 // riferimento: stessa rampa, statico (nessuna automazione)
 { Engine e; e.prepare(fs,512); Engine::Global g; e.setGlobal(g); e.setBand(0,band(Bell,f0,6,1)); int n=N+20000; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs); for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,nullptr,nullptr,std::min(512,n-i)); spur(l,10000,N,fs,f0,"statico (pavimento della misura)"); }
}
