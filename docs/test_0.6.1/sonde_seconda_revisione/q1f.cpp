#include "q1_autom_rt.cpp"
struct P{ ~P(){ int n=tsrq::dbgN.load(); for(int i=0;i<n;++i){ auto&e=tsrq::dbgEv[i]; if(e.t>=11000&&e.t<=17000) std::printf("  ev t=%lld gen %u slot %d from %d chained %d\n",e.t,e.gen,e.fc,e.ff,(int)e.ch);} } } pp;
