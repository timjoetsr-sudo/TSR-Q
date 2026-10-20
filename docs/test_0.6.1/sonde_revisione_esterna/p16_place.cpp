// placement in Linear Phase contro Zero Latency: Bell 1 kHz +6 dB Q1 (fase nulla al centro), ingresso L = seno 1 kHz, R = 0.3 * seno; ampiezza L/R in uscita
#include "common.h"
static void amp(int mode,int pl,double& aL,double& aR){ const double fs=48000; Engine e; e.prepare(fs,512); e.configurePhase(mode,3); Engine::Global g; e.setGlobal(g); e.setBand(0,band(Bell,1000,6,1,12,pl)); e.setBand(1,band(Bell,1000,-3,1,12,(pl+2)%5));
 int n=512*300; std::vector<float> l(n),r(n); for(int i=0;i<n;++i){ l[i]=0.2f*std::sin(2*kPi*1000*i/fs); r[i]=0.3f*l[i]; } for(int i=0;i<n;i+=512) e.process(l.data()+i,r.data()+i,nullptr,nullptr,512);
 aL=0;aR=0; for(int i=n-9600;i<n;++i){ aL=std::max(aL,(double)std::abs(l[i])); aR=std::max(aR,(double)std::abs(r[i])); } }
int main(){ const char* P[]={"Stereo","Left","Right","Mid","Side"}; for(int pl=0;pl<5;++pl){ double zl,zr,ll,lr; amp(0,pl,zl,zr); amp(1,pl,ll,lr); std::printf("%-6s + %-6s: ZL L %.5f R %.5f | LP L %.5f R %.5f | diff %.4f / %.4f dB\n",P[pl],P[(pl+2)%5],zl,zr,ll,lr,20*std::log10(ll/zl),20*std::log10(lr/zr)); } }
