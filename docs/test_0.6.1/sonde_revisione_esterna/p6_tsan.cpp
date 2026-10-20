// ThreadSanitizer: thread audio (cambi continui, bande Mid/Side, toggle sync), worker di progetto; cambio risoluzione sotto mutex (come suspendProcessing)
#include "common.h"
#include <mutex>
int main(){ const double fs=48000; static Engine e; e.prepare(fs,256); e.setLinSync(false); e.configurePhase(1,0); Engine::Global g; e.setGlobal(g); std::mutex cb; std::atomic<bool> stop{false};
 std::thread audio([&]{ std::vector<float> l(256),r(256); std::mt19937 rg(3); std::uniform_real_distribution<float> u(-.3f,.3f); for(int it=0;it<6000;++it){ std::lock_guard<std::mutex> lk(cb);
   for(int i=0;i<256;++i){l[i]=u(rg);r[i]=u(rg);} for(int b=0;b<6;++b){ BandParams p=band(b%3==0?Bell:(b%3==1?LowShelf:HighCut),200+b*900+(it%50)*10,(it%7)-3,1,12,b%5); p.dyn = (b==2) && (it/500)%2; e.setBand(b,p);} if(it%1000==999) e.setLinSync(!((it/1000)%2)); e.process(l.data(),r.data(),nullptr,nullptr,256);} stop=true; });
 std::thread timer([&]{ int k=0; while(!stop){ std::this_thread::sleep_for(std::chrono::milliseconds(150)); std::lock_guard<std::mutex> lk(cb); e.configurePhase(1,(++k)%3); } });
 audio.join(); timer.join(); std::printf("fine\n"); }
