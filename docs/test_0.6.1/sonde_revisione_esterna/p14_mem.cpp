// memoria residente di un'istanza in Linear Phase (RSS da /proc/self/status) e tempo di configurePhase (thread del timer, elaborazione sospesa)
#include "common.h"
#include <fstream>
#include <string>
static long rssKB(){ std::ifstream f("/proc/self/status"); std::string s; while(std::getline(f,s)) if(s.rfind("VmRSS:",0)==0) return std::stol(s.substr(6)); return 0; }
int main(){ for(double fs:{48000.0,192000.0}) for(int res:{1,4}){ long r0=rssKB(); auto* e=new Engine(); e->prepare(fs,512); auto t0=std::chrono::steady_clock::now(); e->configurePhase(1,res); double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count(); Engine::Global g; e->setGlobal(g); e->setBand(0,band(Bell,1000,3,1,12,Mid));
  std::vector<float> l(512),r(512); for(int k=0;k<400;++k) e->process(l.data(),r.data(),nullptr,nullptr,512); long r1=rssKB(); std::printf("fs %6.0f res %d: RSS +%ld MB, configurePhase %.1f ms\n",fs,res,(r1-r0)/1024,ms); delete e; } }
