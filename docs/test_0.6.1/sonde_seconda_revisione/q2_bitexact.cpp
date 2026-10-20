// Zero Latency: scenario deterministico difficile, uscita scritta su file (per confronto al bit tra 0.6.0 e 0.6.1). Argomento: fs, scenario, file
#include "common.h"
#include <fstream>
int main(int argc,char**argv){ const double fs=atof(argv[1]); const int sc=atoi(argv[2]); std::ofstream of(argv[3],std::ios::binary);
 Engine e; e.prepare(fs,1024); e.configurePhase(0,1); Engine::Global g; e.setGlobal(g);
 std::mt19937 rg(11+sc); std::uniform_real_distribution<float> u(-.4f,.4f); std::uniform_int_distribution<int> bl(1,1024);
 const int n=(int)fs*4; std::vector<float> l(n),r(n),sl(n),sr(n); for(int i=0;i<n;++i){ l[i]=u(rg); r[i]=0.6f*l[i]+0.4f*u(rg); sl[i]=u(rg)*((i/20000)%2); sr[i]=sl[i]; }
 BandParams b[8]; for(int k=0;k<8;++k) b[k]=band(Bell,100*(k+1)*(k+1),3,1);
 b[1]=band(LowCut,40,0,2,48); b[2]=band(HighShelf,8000,-4,1,12); b[3]=band(Bell,2500,6,3,12,sc>=1?Mid:Stereo); b[4]=band(HighCut,15000,0,1,24,sc>=1?Side:Stereo); b[5]=band(LowShelf,150,2,1,12); b[6]=band(Notch,60,0,10); b[7]=band(TiltShelf,1000,2,1,12);
 if(sc>=2){ b[5].dyn=true; b[5].thr=-30; b[5].range=-8; b[0].dyn=true; b[0].thr=-25; b[0].range=6; b[0].sc=true; b[0].det=DetPeak; }
 int pos=0, step=0;
 while(pos<n){ int m=std::min(n-pos, sc>=3 ? bl(rg) : 512); double t=pos/fs;
   // automazione
   b[3].f=2500*std::pow(2.0,std::sin(t*3)); b[3].gain=6*std::cos(t*2); b[1].f=40*std::pow(2.0,2*std::sin(t*1.3)); b[1].q=1+5*(0.5+0.5*std::sin(t*0.7));
   if(sc>=1){ b[7].used = ((int)(t*3))%2==0; b[4].type = ((int)(t*2))%2 ? HighCut : LowShelf; b[2].place = ((int)(t*2.5))%3; }
   if(sc>=2){ g.solo = ((int)(t*4))%5==1 ? 3 : -1; g.bypass = ((int)(t*3.3))%7==3; g.character = ((int)(t*1.7))%3; g.panMS = ((int)t)%2; g.pan = sc>=4 ? 0.0 : 0.3*std::sin(t); g.invert = ((int)(t*2.2))%4==2; g.outDb = ((int)(t*5))%6==0 ? -70 : 1.5; g.inDb = 0.5*std::sin(t); }
   if(sc>=4){ g.solo=-1; g.bypass=false; g.character=0; g.invert=false; g.outDb=0; g.inDb=0; g.panMS = true; }   // regime con Mid/Side pan
   e.setGlobal(g); for(int k=0;k<8;++k) e.setBand(k,b[k]);
   e.process(l.data()+pos,r.data()+pos,sc>=2?sl.data()+pos:nullptr,sc>=2?sr.data()+pos:nullptr,m); pos+=m; ++step; }
 of.write((char*)l.data(),n*4); of.write((char*)r.data(),n*4); }
