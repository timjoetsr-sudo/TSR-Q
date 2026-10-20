#include "common.h"
int main(int argc,char**argv){ const double fs=48000,f0=1000; int td=atoi(argv[1]); double g2=atof(argv[2]); int res=atoi(argv[3]); Engine e; e.prepare(fs,512); e.configurePhase(1,res); Engine::Global g; e.setGlobal(g);
 BandParams b=band(Bell,f0,12,1); b.dyn=td?false:true; b.thr=0; e.setBand(0,b); int n=(int)fs*3, sw=512*188; std::vector<float> l(n),r(n); for(int i=0;i<n;++i) l[i]=r[i]=0.1f*std::sin(2*kPi*f0*i/fs);
 for(int i=0;i<n;i+=512){ if(i==sw){ b.dyn=td?true:false; b.gain=g2;} e.setBand(0,b); e.process(l.data()+i,r.data()+i,nullptr,nullptr,std::min(512,n-i));}
 double mx=0; int tm=0; for(int s=sw-4800;s<n-48;s+=48){ double pk=0; for(int i=s;i<s+48;++i) pk=std::max(pk,(double)std::abs(l[i])); if(pk>mx){mx=pk;tm=s;} if((s-sw)%960==0 && s<sw+9600) std::printf("%+7.1f ms %+.2f dB\n",(s-sw)*1000/fs,20*std::log10(pk/0.1)); }
 std::printf("max %+.2f dB at %+.1f ms\n",20*std::log10(mx/0.1),(tm-sw)*1000/fs); }
