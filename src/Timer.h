#pragma once

#include<chrono>
#include<cassert>

template<typename TYPE>
class Timer{
  public:
    Timer(){
      reset();
    }
    void reset(){
      start = std::chrono::high_resolution_clock::now();
      last  = start;
    }
    TYPE elapsedFromStart(){
      auto const newTime = std::chrono::high_resolution_clock::now();
      std::chrono::duration<TYPE>const elapsed = newTime - start;
      last = newTime;
      return elapsed.count();
    }
    TYPE elapsedFromLast(){
      auto const newTime = std::chrono::high_resolution_clock::now();
      std::chrono::duration<TYPE>const elapsed = newTime - last;
      last = newTime;
      return elapsed.count();
    }
  protected:
    std::chrono::time_point<std::chrono::high_resolution_clock>start;
    std::chrono::time_point<std::chrono::high_resolution_clock>last ;
};
