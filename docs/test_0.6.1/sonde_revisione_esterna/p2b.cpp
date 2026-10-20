#include "common.h"
int main(){ const double fs=48000,f0=1000; for(int td=0;td<2;++td){ Engine e; e.prepare(fs,512); e.configurePhase(1,1); Engine::Global g; e.setGlobal(g);
 BandParams b=band(Bell,f0,12,1); b.dyn=td?false:true; b.thr=0; e.setBand(0,b); int n=(int)fs*3, sw=512*188; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs);
 for(int i=0;i<n;i+=512){ if(i==sw){ b.dyn=td?true:false;} e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,std::min(512,n-i));} 
 for(int s=sw-960;s<sw+4000;s+=96){ double pk=0; for(int i=s;i<s+96;++i) pk=std::max(pk,(double)std::abs(l[i])); std::printf("%+7.2f ms pk %.4f (%+.2f dB)\n",(s-sw)*1000/fs,pk,20*std::log10(pk/0.39811));} std::printf("----\n"); } }
