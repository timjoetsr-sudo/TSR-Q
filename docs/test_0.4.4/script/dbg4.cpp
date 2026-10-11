#include "TsrqDsp.h"
#include <cstdio>
using namespace tsrq;
// dinamica Bell a 200 Hz (soglia -40, range -6): tono a 5 kHz -10 dBFS (fuori banda) e tono a 200 Hz -10 dBFS (in banda)
int main(){ const double fs=48000; for (double ft: {5000.0, 200.0}) {
  const int n=(int)fs*2; std::vector<float> L(n),R(n); for(int i=0;i<n;++i) L[i]=R[i]=(float)(0.316*std::sqrt(2.0)*std::sin(2*kPi*ft*i/fs));
  Engine e; e.prepare(fs,4096); Engine::Global gl; e.setGlobal(gl); BandParams b; b.used=true; b.type=Bell; b.f=200; b.gain=0; b.q=1; b.dyn=true; b.thr=-40; b.knee=6; b.range=-6; e.setBand(0,b);
  for(int i0=0;i0<n;i0+=512) e.process(L.data()+i0,R.data()+i0,nullptr,nullptr,std::min(512,n-i0));
  printf("Bell dinamica 200 Hz, tono %5.0f Hz -10 dBFS: riduzione dinamica %.2f dB, livello letto dal detector %.2f dB\n", ft, e.meterDelta[0].load(), e.meterLevel[0].load()); } }
