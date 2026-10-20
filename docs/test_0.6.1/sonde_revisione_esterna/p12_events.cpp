// eventi discreti in Linear Phase e Zero Latency: banda on/off, cambio tipo, cambio placement, solo, bypass. Seno 200 Hz -12 dBFS (L=R*0.5 per avere Side);
// misura: energia sopra 4 kHz (filtro 8° ordine sull'uscita) nella finestra dell'evento, dBFS di picco
#include "common.h"
static double hfPeak(const std::vector<float>& x,int s0,int s1,double fs){ Grid g; g.init(fs); BandCoefs c; BandParams b=band(LowCut,4000,0,1,96); designBand(b,0,g,c); double z[2*kMaxSecs]={}; double pk=0; for(int i=0;i<s1;++i){ double v=x[i]; for(int k=0;k<c.n;++k){ const Coef&q=c.c[k]; double y=q.b0*v+z[2*k]; z[2*k]=q.b1*v-q.a1*y+z[2*k+1]; z[2*k+1]=q.b2*v-q.a2*y; v=y;} if(i>=s0) pk=std::max(pk,std::abs(v)); } return 20*std::log10(pk+1e-12); }
int main(){ const double fs=48000; const char* ev[]={"banda on","banda off","tipo Bell->LowShelf","placement Stereo->Side","solo on","bypass on","freq 300->3000 (salto)"};
 for(int mode=0;mode<2;++mode) for(int k=0;k<7;++k){ Engine e; e.prepare(fs,512); e.configurePhase(mode,1); Engine::Global g; e.setGlobal(g); int lat=e.latencySamples();
  BandParams b=band(Bell,300,9,2); b.used=k!=0; e.setBand(0,b); e.setBand(1,band(HighShelf,6000,-3,1,12));
  const int sw=512*200, n=(sw+lat+48000+512*20)/512*512; std::vector<float> l(n),r(n); for(int i=0;i<n;++i){ l[i]=0.25f*std::sin(2*kPi*200*i/fs); r[i]=0.5f*l[i]; }
  for(int i=0;i<n;i+=512){ if(i==sw){ if(k==0) b.used=true; if(k==1) b.used=false; if(k==2) b.type=LowShelf; if(k==3) b.place=Side; if(k==4) g.solo=0; if(k==5) g.bypass=true; if(k==6) b.f=3000; e.setGlobal(g);} e.setBand(0,b); e.setBand(1,band(HighShelf,6000,-3,1,12)); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); }
  double base=hfPeak(l,sw-24000,sw-4000,fs), evp=std::max(hfPeak(l,sw-4000,n,fs),-200.0); std::vector<float> rr(r); double evr=hfPeak(rr,sw-4000,n,fs);
  std::printf("%-13s %-24s: picco sopra 4 kHz prima %7.1f dBFS, dopo l'evento L %7.1f dBFS, R %7.1f dBFS\n",mode?"Linear Phase":"Zero Latency",ev[k],base,evp,evr); } }
