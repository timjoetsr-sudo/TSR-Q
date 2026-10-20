// richiesta Linear Phase rimasta nel triplo buffer quando si passa da tempo reale a offline e ritorno (es. bounce subito dopo un ritocco)
#include "common.h"
int main(){ const double fs=48000, f0=1000; int stale=0, tot=0;
 for(int trial=0;trial<10;++trial){ Engine e; e.prepare(fs,512); e.setLinSync(false); e.configurePhase(1,4); Engine::Global g; e.setGlobal(g);
  std::vector<float> l(512),r(512); auto blk=[&](){ for(int i=0;i<512;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs); e.process(l.data(),r.data(),nullptr,nullptr,512); };
  BandParams b=band(Bell,f0,12,1); e.setBand(0,b); for(int k=0;k<200;++k){ blk(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }   // tempo reale, assestato a +12
  b.gain=6; e.setBand(0,b); blk();      // R0: worker occupato (Maximum)
  b.gain=0; e.setBand(0,b); blk();      // R1: resta in attesa nel triplo buffer
  e.setLinSync(true);                   // il host passa a offline (bounce)
  b.gain=-12; e.setBand(0,b); for(int k=0;k<200;++k) blk();   // l'ultima impostazione e -12 dB
  e.setLinSync(false);                  // ritorno al tempo reale, nessun cambio di parametri
  for(int k=0;k<300;++k){ blk(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  double pk=0; for(int i=0;i<512;++i) pk=std::max(pk,(double)std::abs(l[i])); const double db=20*std::log10(pk/0.1); ++tot; if(std::abs(db+12)>0.1) ++stale;
  std::printf("prova %d: guadagno finale a 1 kHz %+.2f dB (atteso -12.00)\n",trial,db); }
 std::printf("stato stantio in %d / %d prove\n",stale,tot); }
