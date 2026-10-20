// Linear Phase: banda statica che entra (biquad, poi dissolvenza verso il FIR) o esce. Seno a frequenza f; livello in finestre di 2 ms dopo l'evento contro i livelli stabili prima e dopo.
// Argomenti: tipo (0 LowCut96@100 Hz, 1 Bell 1 kHz +12 Q1, 2 HighShelf 3 kHz +12), frequenza del seno, entra(1)/esce(0), risoluzione
#include "common.h"
int main(int argc,char**argv){ const double fs=48000; int ty=atoi(argv[1]); double fq=atof(argv[2]); bool in=atoi(argv[3]); int res=atoi(argv[4]);
 BandParams b = ty==0? band(LowCut,100,0,1,96) : ty==1? band(Bell,1000,12,1) : band(HighShelf,3000,12,1,12); b.used=!in;
 Engine e; e.prepare(fs,512); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g); e.setBand(0,b); int lat=e.latencySamples();
 const int sw=512*200, n=(sw+lat+48000)/512*512; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*fq*i/fs);
 for(int i=0;i<n;i+=512){ if(i==sw) b.used=in; e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); }
 auto lv=[&](int s){ double pk=0; for(int i=s;i<s+96;++i) pk=std::max(pk,(double)std::abs(l[i])); return 20*std::log10(pk/0.1+1e-12); };
 double a=lv(sw-4800), z=lv(n-200), lo=std::min(a,z), hi=std::max(a,z), mn=1e9, mx=-1e9; int tmn=0,tmx=0; for(int s=sw-500;s<n-200;s+=48){ double v=lv(s); if(v<mn){mn=v;tmn=s;} if(v>mx){mx=v;tmx=s;} }
 std::printf("tipo %d seno %5.0f Hz %s res %d: prima %+6.2f dB, dopo %+6.2f dB; durante: minimo %+6.2f (%.1f ms), massimo %+6.2f (%.1f ms) -> sotto %.2f dB, sopra %.2f dB\n",ty,fq,in?"entra":"esce ",res,a,z,mn,(tmn-sw)*1000/fs,mx,(tmx-sw)*1000/fs,lo-mn,mx-hi); }
