#include "common.h"
int main(){ const double fs=48000; Engine e; e.prepare(fs,512); e.configurePhase(1,1); Engine::Global g; e.setGlobal(g); int lat=e.latencySamples();
 BandParams b=band(Bell,300,9,2); b.used=false; e.setBand(0,b); e.setBand(1,band(HighShelf,6000,-3,1,12));
 const int sw=512*200, n=sw+512*60; std::vector<float> l(n),r(n); for(int i=0;i<n;++i){ l[i]=0.25f*std::sin(2*kPi*200*i/fs); r[i]=0.5f*l[i]; }
 std::vector<float> ref=l;
 for(int i=0;i<n;i+=512){ if(i==sw) b.used=true; e.setBand(0,b); e.setBand(1,band(HighShelf,6000,-3,1,12)); e.process(l.data()+i,r.data()+i,nullptr,nullptr,512); }
 // seconda differenza come indicatore di discontinuita
 for(int i=sw-1024;i<sw+512*12;i+=128){ double m=0; for(int k=i;k<i+128;++k){ double d2=l[k+1]-2*l[k]+l[k-1]; m=std::max(m,std::abs(d2)); } std::printf("%6d %.2e\n",i-sw,m); } }
