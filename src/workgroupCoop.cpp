#include <SDL.h>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include <iostream>

int main(int argc,char*argv[]){
  size_t v = 450;
  if(argc>1)v = atoi(argv[1]);
  if(SDL_Init(SDL_INIT_EVERYTHING)<0)std::cerr << SDL_GetError() << std::endl;
  auto window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, v / 100) < 0)std::cerr << SDL_GetError() << std::endl;
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (v % 100) / 10) < 0)std::cerr << SDL_GetError() << std::endl;
  auto ctx = SDL_GL_CreateContext(window);
  if(ctx == nullptr)std::cerr << SDL_GetError() << std::endl;
  ge::gl::init();
  std::cerr << "RENDERER: " << ge::gl::glGetString(GL_RENDERER) << std::endl;
  std::cerr << "VENDOR  : " << ge::gl::glGetString(GL_VENDOR  ) << std::endl;
  std::cerr << "VERSION : " << ge::gl::glGetString(GL_VERSION ) << std::endl;


  std::string src = R".(
  #version 450
  layout(local_size_x=64)in;

  
  layout(binding=0,std430)volatile buffer Data{uint data[];};
  layout(binding=1,std430)volatile buffer Sig {uint signal [];};

  void main(){

    //writer work group
    if(gl_WorkGroupID.x==0){

      //write data to buffer
      for(uint i=0;i<1024*1024;++i){
        data[i*64+gl_LocalInvocationID.x] = 1;
      }

      if(gl_LocalInvocationID.x == 0)signal[0] = 2;

    }

    //reader work group
    if(gl_WorkGroupID.x==17){

      while(signal[0] < 2)continue;//wait for signal

      //modify data in buffer
      for(uint i=0;i<1024*1024;++i){
        data[(1024*1024-1-i)*64+gl_LocalInvocationID.x] += 1;
      }
    }

  }

  ).";

  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src));

  auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*1024*1024*64);
  buf->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  
  auto sig = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t));
  sig->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  sig->bindBase(GL_SHADER_STORAGE_BUFFER,1);

  prg->use();

  ge::gl::glDispatchCompute(1024,1,1);
  ge::gl::glFinish();
  

  std::vector<uint32_t>data;

  buf->getData(data);

  uint32_t wrong = 0;
  for(uint32_t i=0;i<1024*1024*64;++i){
    if(data[i] != 2)++wrong;
  }

  std::cerr << "wrong: " << wrong << std::endl;


  return 0;
}
