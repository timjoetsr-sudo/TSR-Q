// rendering offline (sync: progetti nel thread audio) con automazione continua di una banda: fattore rispetto al tempo reale
#include "common.h"
int main(){ for(double fs:{48000.0,96000.0,192000.0}) for(int res:{1,2,4}) for(int ms=0;ms<2;++ms){ Engine e; e.prepare(fs,512); e.setLinSync(true); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
  for(int i=1;i<6;++i) e.setBand(i,band(Bell,100*i*i,2,1,12,(ms&&i==3)?Mid:Stereo));
  const int n=(int)(fs*2)/512*512; std::vector<float> l(n),r(n); std::mt19937 rg(1); std::uniform_real_distribution<float> u(-.3f,.3f); for(int i=0;i<n;++i){l[i]=u(rg);r[i]=u(rg);}
  auto t0=std::chrono::steady_clock::now(); for(int i=0;i<n;i+=512){ e.setBand(0,band(Bell,1000,6*std::sin(i/fs*2*kPi*0.5),1)); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512);} double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
  std::printf("fs %6.0f res %d %-6s: 2 s di audio in %.2f s -> %.2f x tempo reale%s\n",fs,res,ms?"Mid":"Stereo",s,s/2,s>2?"  (PIU LENTO DEL TEMPO REALE)":""); } }
