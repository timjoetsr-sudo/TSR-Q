// Zero Latency: progetto preciso e veloce contro modulo analogico, griglia ampia (tipi x pendenze x Q 0.025..40 x f 20 Hz..20 kHz x guadagni), 44.1 e 48 kHz
#include "common.h"
#include <algorithm>
struct R{double e; char d[160];};
int main(){ std::vector<R> worst; for(double fs:{44100.0,48000.0}){ Grid g; g.init(fs);
 for(int t:{Bell,LowShelf,HighShelf,LowCut,HighCut,Notch,TiltShelf}) for(int sl:{6,12,18,24,36,48,72,96}) for(double q:{0.025,0.1,0.3,0.71,1.0,3.0,10.0,40.0}) for(double f:{20.0,60.0,200.0,1000.0,5000.0,10000.0,15000.0,18000.0,20000.0}) for(double gn:{-30.0,-12.0,6.0,18.0,30.0}) for(int pr=0;pr<2;++pr){
  if(!hasSlope(t) && sl!=12) continue; if((t==LowShelf||t==HighShelf)&&sl>12) continue; if(!hasGain(t) && gn!=6.0) continue; if(t==TiltShelf && sl!=12) continue;
  BandParams b=band(t,std::min(f,fs*0.495),gn,q,sl); double gg=hasGain(t)?gn:0; BandCoefs bc; designBand(b,gg,g,bc,pr); SecSet s; sectionsFor(b,gg,s); bool st=true; for(int k=0;k<bc.n;++k) st=st&&stable(bc.c[k]);
  double e=0,fw=0; for(int k=0;k<400;++k){ double fr=20*std::pow(1000.0,k/399.0); if(fr>0.4999*fs) break; double a=10*std::log10(anaMag2(s,fr)); if(a<-30) continue; double d=10*std::log10(bandMag2(bc,fr,fs)); if(!(std::abs(d-a)<1e9)){e=1e9;break;} if(std::abs(d-a)>e){e=std::abs(d-a);fw=fr;} }
  if(!st) e=1e10; R r; r.e=e; std::snprintf(r.d,160,"fs %.0f %s t%d sl%d Q%g f%g g%+g: %.3f dB @ %.0f Hz%s",fs,pr?"preciso":"veloce ",t,sl,q,f,gn,e,fw,st?"":" INSTABILE"); worst.push_back(r); } }
 std::sort(worst.begin(),worst.end(),[](const R&a,const R&b){return a.e>b.e;}); int np=0; for(auto&r:worst){ if(std::strstr(r.d,"preciso")&&np<25){ std::printf("%s\n",r.d); ++np; } }
 int c1=0,c3=0,ct=0; for(auto&r:worst) if(std::strstr(r.d,"preciso")){ ++ct; if(r.e>1) ++c1; if(r.e>3) ++c3; } std::printf("preciso: %d casi, >1 dB: %d, >3 dB: %d\n",ct,c1,c3); }
