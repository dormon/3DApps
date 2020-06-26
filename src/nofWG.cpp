#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <SDL.h>
#include <cstring>
#include <string>

using namespace ge::gl;
using namespace std;

int main(int argc,char*argv[]){
  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto const wgs = args->getu32("-n",256,"nof work groups");
  auto const tries = args->getu32("-t",1000,"nof of tries");
  auto const size  = args->getu32("-s",64,"work group size");
  auto const local  = args->getu32("-l",2,"local memory size in uints (min 2)");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  uint32_t v = 450;
  if(SDL_Init(SDL_INIT_EVERYTHING)<0)std::cerr << SDL_GetError() << std::endl;
  auto window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, v / 100) < 0)
    std::cerr << SDL_GetError() << std::endl;
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (v % 100) / 10) < 0)
    std::cerr << SDL_GetError() << std::endl;
  auto ctx = SDL_GL_CreateContext(window);
  if(ctx == nullptr)
    std::cerr << SDL_GetError() << std::endl;
  ge::gl::init();

  std::cerr << glGetString(GL_VERSION) << std::endl;

  std::string const csSrc = R".(
  
  layout(local_size_x=WGS)in;

  layout(std430,binding=0)volatile coherent buffer Data{
    uint counter;
    uint data[];
  };

  uniform uint tries = 1000;
  shared uint done;
  shared uint ww;

  #define UTILITY_SHARED 2

  #define SMS  (SHARED_SIZE - UTILITY_SHARED)

  #if SMS > 0
  shared uint sharedMemory[SMS];
  #endif

  void main(){
    
    if(gl_GlobalInvocationID.x == 0)
      data[2] = gl_NumWorkGroups.x;


    if(gl_LocalInvocationIndex == 0){
      ww = atomicAdd(counter,1);
      done = 0;

      #if SSMS > 0
      for(uint i=0;i<SMS;++i)
        sharedMemory[i] = ww+i;
      #endif
    }
    barrier();
    
    
    for(uint i=0;i<tries;++i){
      if(gl_LocalInvocationIndex == 0){
        if(counter >= gl_NumWorkGroups.x)
          done = 1;
      }
      barrier();

      if(done == 1){
        if(gl_LocalInvocationIndex == 0){
          atomicAdd(data[0],1);
        }
        return;
      }

      uint c=0;
      for(uint j=0;j<10;++j)
        c = (c+j+ww)%117;
      if(c==1337){
        #if SMS > 0
        for(uint j=uint(gl_LocalInvocationIndex);j<SHARED_SIZE-2;++j)
          c+= sharedMemory[j]+gl_LocalInvocationIndex;
        #endif
        data[5]=1111+c;
        
      }
      
    }
    if(gl_LocalInvocationIndex==0){
      atomicAdd(data[1],1);
    }
    
    return;
  }
  ).";

  auto buffer = make_shared<Buffer>(sizeof(uint32_t)*16);
  buffer->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  auto prg = make_shared<Program>(make_shared<Shader>(GL_COMPUTE_SHADER
        ,"#version 450\n"
        ,Shader::define("WGS",(int)size)
        ,Shader::define("SHARED_SIZE",(int)local)
        ,csSrc));
  prg->bindBuffer("Data",buffer);
  prg->set1ui("tries",tries);
  prg->use();

  glDispatchCompute(wgs,1,1);
  glFinish();

  std::vector<uint32_t>d;
  buffer->getData(d);
  std::cerr << "counter         : " << d[0] << std::endl;
  std::cerr << "finished in time: " << d[1] << std::endl;
  std::cerr << "not finished    : " << d[2] << std::endl;
  std::cerr << "num wgs         : " << d[3] << std::endl;

  prg = nullptr;
  buffer = nullptr;

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return EXIT_SUCCESS;
}
