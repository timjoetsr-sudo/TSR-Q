#include "TsrqDsp.h"
#include <cstdio>
#include <chrono>
#include <vector>
#include <algorithm>
using namespace tsrq;
int main(){ const double fs=48000; const int n=(int)fs*20; std::vector<float> L0(n),R0(n); for(int i=0;i<n;++i){L0[i]=(float)(0.3*std::sin(i*0.0123)+0.2*std::sin(i*0.31));R0[i]=(float)(0.3*std::sin(i*0.017));}
 for (int chm: {0,1,2}) { std::vector<double> t; for(int r=0;r<7;++r){ std::vector<float> L=L0,R=R0; Engine e; e.prepare(fs,4096); Engine::Global g; g.character=chm; e.setGlobal(g); auto t0=std::chrono::steady_clock::now();
   for(int i0=0;i0<n;i0+=512) e.process(L.data()+i0,R.data()+i0,nullptr,nullptr,std::min(512,n-i0)); t.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()); }
   std::sort(t.begin(),t.end()); printf("character %d (%s): %.3f %% di un core (mediana di 7)\n", chm, chm==0?"Clean":chm==1?"Subtle":"Warm", t[3]/20*100); } }
