#include "q1_autom_rt.cpp"
struct P{ ~P(){ int n=tsrq::dbgN.load(); for(int i=1;i<n;++i){ auto&e=tsrq::dbgEv[i]; auto&p=tsrq::dbgEv[i-1]; if(e.t-p.t!=512 || e.ch!=p.ch) std::printf("  ev t=%lld (prec %lld) gen %u slot %d from %d chained %d\n",e.t,p.t,e.gen,e.fc,e.ff,(int)e.ch);} } } pp;
