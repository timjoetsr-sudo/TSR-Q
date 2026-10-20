// cramping a 48 kHz: errore massimo |digitale - analogico| 20 Hz - 20 kHz (punti sopra -30 dB), progetto preciso e veloce (automazione)
#include "common.h"
int main(){ const double fs=48000; Grid g; g.init(fs); struct C{const char*n;BandParams b;}; std::vector<C> cs={{"Bell 10k +12 Q2",band(Bell,10000,12,2)},{"HighCut 15k 24",band(HighCut,15000,0,1,24)},{"HighCut 15k 24 Q3",band(HighCut,15000,0,3,24)},{"HighShelf 15k +12",band(HighShelf,15000,12,1,12)},{"Bell 16k -12 Q4",band(Bell,16000,-12,4)},{"Bell 18k +6 Q1",band(Bell,18000,6,1)},{"LowCut 18k 12 Q10",band(LowCut,18000,0,10,12)}};
 for(auto&c:cs) for(int pr=1;pr>=0;--pr){ BandCoefs bc; designBand(c.b,hasGain(c.b.type)?c.b.gain:0,g,bc,pr); SecSet s; sectionsFor(c.b,hasGain(c.b.type)?c.b.gain:0,s); double e=0,fw=0;
  for(int k=0;k<2000;++k){ double f=20*std::pow(1000.0,k/1999.0); double a=10*std::log10(anaMag2(s,f)); if(a<-30) continue; double d=10*std::log10(bandMag2(bc,f,fs)); if(std::abs(d-a)>e){e=std::abs(d-a);fw=f;} }
  std::printf("%-20s %-7s: %.3f dB @ %.0f Hz (sezioni %d)\n",c.n,pr?"preciso":"veloce",e,fw,bc.n); } }
