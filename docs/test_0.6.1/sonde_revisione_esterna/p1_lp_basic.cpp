// LP: latenza = picco, simmetria, modulo vs analogico, pre-ringing, per risoluzione e fs
#include "common.h"
int main(){
  struct C{const char* name; BandParams b;};
  std::vector<C> cs={{"Bell 40 +12 Q4",band(Bell,40,12,4)},{"Bell 100 -18 Q20",band(Bell,100,-18,20)},{"Bell 1k +12 Q1",band(Bell,1000,12,1)},
    {"LowCut 30 48dB",band(LowCut,30,0,1,48)},{"LowCut 20 96dB Q10",band(LowCut,20,0,10,96)},{"HighCut 18k 96dB",band(HighCut,18000,0,1,96)},
    {"Notch 60 Q10",band(Notch,60,0,10)},{"LowShelf 50 +12",band(LowShelf,50,12,1,12)},{"HighShelf 10k +6",band(HighShelf,10000,6,1,12)}};
  for(double fs:{44100.0,48000.0,96000.0,192000.0}) for(int res=0;res<5;++res){
    int L0=LinPhase::firLen(res,fs);
    for(auto& c:cs){
      Engine e; e.prepare(fs,512); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g); e.setBand(0,c.b);
      const int lat=e.latencySamples(); const int n=lat*2+L0+1024; std::vector<float> l(n,0),r(n,0); l[0]=r[0]=1;
      for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,nullptr,nullptr,std::min(512,n-i));
      std::vector<double> h(l.begin(),l.end());
      int pk=0; for(int i=0;i<n;++i) if(std::abs(h[i])>std::abs(h[pk])) pk=i;
      double asym=0; for(int m=1;m<L0/2;++m){ if(lat-m<0||lat+m>=n) break; asym=std::max(asym,std::abs(h[lat-m]-h[lat+m])); }
      double epre=0,etot=0; for(int i=0;i<n;++i){ etot+=h[i]*h[i]; if(i<lat) epre+=h[i]*h[i]; }
      // magnitude vs analog: 20 Hz..min(20k, 0.45fs) log 200 pts, only where analog > -40 dB
      double emax=0,fwor=0; for(int k=0;k<200;++k){ double f=20*std::pow(std::min(20000.0,0.45*fs)/20,k/199.0); double a=anaDb(c.b,f); if(a<-40) continue; double d=20*std::log10(std::abs(dft(h,f,fs))); if(std::abs(d-a)>emax){emax=std::abs(d-a);fwor=f;} }
      std::printf("fs %6.0f res %d L %6d lat %6d pk %6d asym %.1e preE %5.1f%% err %.3f dB @ %.0f Hz  %s\n",fs,res,L0,lat,pk,asym,100*epre/etot,emax,fwor,c.name);
    }
  }
}
