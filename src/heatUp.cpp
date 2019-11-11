#include <SDL.h>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include<thread>
#include<sstream>

bool running = true;

void heatCPU(){
  while(running){
  }
}

int main(int argc,char*argv[]){
  size_t nofThreads = 8;
  std::vector<std::thread>threads(nofThreads);
  for(size_t i=0;i<nofThreads;++i)
    threads.at(i) = std::thread(heatCPU);

  int a;
  std::cin >> a;
  running = false;

  for(size_t i=0;i<nofThreads;++i)
    threads.at(i).join();

  return 0;
}
