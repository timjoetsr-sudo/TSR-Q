#include "q1_autom_rt.cpp"
struct P{ ~P(){ std::printf("scritture nello slot in uso: %d, dissolvenze a catena %d, isolate %d\n",tsrq::dbgClash.load(),tsrq::dbgChain.load(),tsrq::dbgIso.load()); } } pp;
