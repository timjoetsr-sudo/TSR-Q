// ritardo dell'automazione in tempo reale (thread di progetto): gradino di guadagno 0 -> +12 dB, tempo per arrivare a +11.9 dB (blocchi da 512 al ritmo del tempo reale)
#include "common.h"
int main(){ for(double fs:{48000.0,192000.0}) for(int res:{1,4}) for(int ms=0;ms<2;++ms){ Engine e; e.prepare(fs,512); e.setLinSync(false); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g); e.setBand(1,band(Bell,5000,1,1,12,ms?Mid:Stereo));
  BandParams b=band(Bell,1000,0,1); e.setBand(0,b); const int n=(int)(fs*3)/512*512, sw=(int)(fs*1.5)/512*512; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*1000*i/fs);
  for(int i=0;i<n;i+=512){ if(i==sw) b.gain=12; e.setBand(0,b); auto t0=std::chrono::steady_clock::now(); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); std::this_thread::sleep_until(t0+std::chrono::microseconds((long)(512/fs*1e6))); }
  int t=-1; for(int i=sw;i<n-64;i+=16){ double pk=0; for(int j=i;j<i+(int)(fs/1000);++j) pk=std::max(pk,(double)std::abs(l[j])); if(20*std::log10(pk/0.1)>11.9-0.0){ t=i; break; } }
  std::printf("fs %6.0f res %d %-6s: gradino applicato dopo %.1f ms (Zero Latency: rampa di 10.7 ms)\n",fs,res,ms?"Mid":"Stereo",t<0?-1:(t-sw)*1000/fs); } }
