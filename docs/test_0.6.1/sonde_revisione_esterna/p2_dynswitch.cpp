// LP: banda che passa da statica a dinamica (e ritorno) senza che il guadagno effettivo cambi (soglia 0 dBFS, segnale a -20 dBFS: delta = 0)
#include "common.h"
static void runcase(int mode,int res,bool sync,bool toDyn,double gain){
  const double fs=48000, f0=1000; Engine e; e.prepare(fs,512); e.setLinSync(sync); e.configurePhase(mode,res); Engine::Global g; e.setGlobal(g);
  BandParams b=band(Bell,f0,gain,1); b.dyn=!toDyn; b.thr=0; b.range=-6; e.setBand(0,b);
  const int n=(int)fs*4, sw=512*188; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs);
  for(int i=0;i<n;i+=512){ if(i==sw){ b.dyn=toDyn; e.setBand(0,b);} else e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); if(!sync) std::this_thread::sleep_for(std::chrono::microseconds(500)); }
  // envelope per 240-sample window (5 ms, 5 cycles), dB re steady state before switch
  auto rmsAt=[&](int s){ double a=0; for(int i=s;i<s+240;++i) a+=(double)l[i]*l[i]; return 10*std::log10(a/240+1e-30); };
  double ref=rmsAt(sw-24000); double mx=-1e9,mn=1e9; int tmx=0,tmn=0; for(int s=sw-4800;s<n-240;s+=48){ double v=rmsAt(s)-ref; if(v>mx){mx=v;tmx=s;} if(v<mn){mn=v;tmn=s;} }
  double endv=rmsAt(n-480)-ref; std::printf("ref %.2f dB  ",ref);
  std::printf("mode %d res %d %s %s gain %+5.1f: dopo il cambio  max %+6.2f dB (t=%+.1f ms)  min %+6.2f dB (t=%+.1f ms)  finale %+.2f dB\n",mode,res,sync?"sync ":"async",toDyn?"stat->dyn":"dyn->stat",gain,mx,(tmx-sw)*1000/fs,mn,(tmn-sw)*1000/fs,endv);
}
int main(){ for(double gn:{12.0,-12.0}) for(bool td:{true,false}){ runcase(0,1,true,td,gn); runcase(1,1,true,td,gn); runcase(1,1,false,td,gn); runcase(1,4,true,td,gn);} }
