// Zero Latency: automazione rapida di tagli risonanti (Q 10) e shelf; picco d'uscita contro il massimo statico lungo il percorso
#include "common.h"
int main(){ const double fs=48000; struct C{int t;int sl;double q;double g;};
 std::vector<C> cs={{HighCut,48,10,0},{LowCut,96,10,0},{HighCut,12,10,0},{LowCut,24,10,0},{HighCut,96,10,0},{LowShelf,12,10,18},{BandPass,48,10,0},{Bell,12,40,30},{Notch,12,40,0}};
 for(auto c:cs) for(double per:{0.02,0.1,0.5}) for(int sig=0;sig<2;++sig){ Engine e; e.prepare(fs,64); Engine::Global g; e.setGlobal(g);
  const int n=48000*2; std::vector<float> l(n),r(n); std::mt19937 rg(9); std::uniform_real_distribution<float> u(-.1f,.1f); for(int i=0;i<n;++i){ l[i]=sig? 0.1f*std::sin(2*kPi*1000*i/fs) : u(rg); r[i]=l[i]; }
  double fmax=0; Grid gr; gr.init(fs); double statMax=0; bool fin=true; double pk=0;
  for(int i=0;i<n;i+=64){ double ph=std::fmod(i/fs/per,1.0); double f=100*std::pow(150.0,ph<0.5?2*ph:2-2*ph); BandParams b=band(c.t,f,c.g,c.q,c.sl); e.setBand(0,b);
    // massimo statico del modulo lungo il percorso
    SecSet s; sectionsFor(b,c.g,s); for(int k=0;k<60;++k){ double fr=20*std::pow(1000.0,k/59.0); statMax=std::max(statMax,anaMag2(s,fr)); } for(double fr:{f*0.97,f,f*1.03}) statMax=std::max(statMax,anaMag2(s,fr));
    e.process(l.data()+i,r.data()+i,nullptr,nullptr,64); for(int j=i;j<i+64;++j){ fin=fin&&std::isfinite(l[j]); pk=std::max(pk,(double)std::abs(l[j])); } }
  double inpk=0.1; std::printf("%-9s %2d Q%-3g g%+3g periodo %4.0f ms %-5s: picco uscita %+6.1f dB rispetto all'ingresso, massimo statico %+6.1f dB, eccesso %+5.1f dB, finito %d\n",
    std::vector<const char*>{"Bell","LowShelf","HighShelf","LowCut","HighCut","Notch","BandPass"}[c.t],c.sl,c.q,c.g,per*1000,sig?"seno":"rumore",20*std::log10(pk/inpk),10*std::log10(statMax),20*std::log10(pk/inpk)-10*std::log10(statMax),(int)fin); } }
