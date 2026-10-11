#include "TsrqDsp.h"
#include <cstdio>
using namespace tsrq;
int main(){ const double fs=48000;
 for (int det: {0,1}) for (double att: {0.1, 1.0, 5.0, 50.0}) for (double f: {100.0, 1000.0, 5000.0}) {
  const int n=(int)fs*3; std::vector<float> L(n),R(n); const double lvl=-20, amp=std::pow(10.0,lvl/20)*std::sqrt(2.0);
  for(int i=0;i<n;++i) L[i]=R[i]=(float)(amp*std::sin(2*kPi*f*i/fs));
  Engine e; e.prepare(fs,4096); Engine::Global gl; e.setGlobal(gl); BandParams b; b.used=true; b.type=Bell; b.f=f; b.gain=0; b.q=1; b.dyn=true; b.thr=-60; b.knee=0; b.range=-1.5; b.att=att; b.rel=80; b.det=det; e.setBand(0,b);
  double mn=1e9,mx=-1e9; for(int i0=0;i0<n;i0+=512){ e.process(L.data()+i0,R.data()+i0,nullptr,nullptr,std::min(512,n-i0)); if(i0>n/2){ double v=e.meterLevel[0].load(); mn=std::min(mn,v); mx=std::max(mx,v);} }
  printf("det %s att %5.1f ms f %5.0f Hz: livello letto %.3f .. %.3f dB (vero -20.000)\n", det?"Peak":"RMS ", att, f, mn, mx); }
}
