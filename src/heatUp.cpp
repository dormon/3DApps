#include<thread>
#include<sstream>
#include<vector>
#include<iostream>
#include<CL/cl.hpp>

bool running = true;

void heatCPU(){
  while(running){
  }
}

void heatGPU(){

}

int main(int argc,char*argv[]){
  size_t nofThreads = argc>1?atoi(argv[1]):4;
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
