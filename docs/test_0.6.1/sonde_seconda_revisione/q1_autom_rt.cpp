// automazione continua con progetti nel thread dedicato, blocchi al ritmo del tempo reale (sleep_until): Bell 1 kHz Q1 0 -> +12 dB in 1,36 s, seno 1 kHz.
// Energia spuria fuori da 1 kHz +-60 Hz rispetto alla fondamentale. Argomenti: risoluzione, blocco
#include "common.h"
static double spur(const std::vector<float>& x,int s0,int N,double fs,double f0){ Fft f; f.init(N); std::vector<Cpx> a(N); for(int i=0;i<N;++i){ double w=0.5-0.5*std::cos(2*kPi*i/N); a[i]=x[s0+i]*w; } f.run(a.data(),false);
  double ef=0,es=0; for(int k=1;k<N/2;++k){ double fr=k*fs/N, p=std::norm(a[k]); if(std::abs(fr-f0)<60) ef+=p; else if(fr>20&&fr<20000) es+=p; } return 10*std::log10(es/ef); }
int main(int argc,char**argv){ const double fs=48000,f0=1000; const int N=65536; int res=atoi(argv[1]), blk=atoi(argv[2]); bool sync=argc>3;
  Engine e; e.prepare(fs,blk); e.setLinSync(sync); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
  const int lat=e.latencySamples(); const int n=(lat+N+48000)/blk*blk; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs);
  auto t0=std::chrono::steady_clock::now();
  for(int i=0;i<n;i+=blk){ double t=std::clamp((double)(i-lat-8000)/N,0.0,1.0); e.setBand(0,band(Bell,f0,12*t,1)); e.process(l.data()+i,r.data()+i,nullptr,nullptr,blk); if(!sync) std::this_thread::sleep_until(t0+std::chrono::microseconds((long long)((i+blk)/fs*1e6))); }
  // gradini: livello a 1 kHz in finestre di 2 ms, massimo salto tra finestre consecutive
  double mj=0; double prev=-1; for(int s=lat+8000;s<lat+8000+N;s+=96){ double pk=0; for(int i=s;i<s+96;++i) pk=std::max(pk,(double)std::abs(l[i])); double d=20*std::log10(pk/0.1); if(prev>-1e8&&prev!=-1) mj=std::max(mj,std::abs(d-prev)); prev=d; }
  { double prev=-1; for(int s=lat+8000;s<lat+8000+N;s+=96){ double pk=0; for(int i=s;i<s+96;++i) pk=std::max(pk,(double)std::abs(l[i])); double d=20*std::log10(pk/0.1); if(prev!=-1 && std::abs(d-prev)>0.5) std::printf("salto %.2f dB all uscita t=%d (ingresso t=%d)\n",d-prev,s,s-lat); prev=d; } }
  std::printf("res %d blocco %4d %s: spurie %.1f dB, salto massimo tra finestre di 2 ms %.3f dB\n",res,blk,sync?"sync ":"tempo reale",spur(l,lat+8000,N,fs,f0),mj); }
