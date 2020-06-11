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

  layout(binding=0,std430)buffer Data{
    uint a[3];
    uint b[3];
  };

  void main(){
    if(gl_GlobalInvocationID.x==0){
      for(uint i=0;i<3;++i){
        a[i] = i;
        b[i] = 3 + i;
      }
    }
  }

  ).";

  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src));

  auto aBuf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*10);
  auto bBuf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*10);

  aBuf->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  bBuf->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);

  prg->use();

  aBuf->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  bBuf->bindBase(GL_SHADER_STORAGE_BUFFER,1);

  ge::gl::glDispatchCompute(1,1,1);
  ge::gl::glFinish();
  

  std::vector<uint32_t>aData;
  std::vector<uint32_t>bData;

  aBuf->getData(aData);
  bBuf->getData(bData);

  for(auto const&x:aData)std::cerr << x << std::endl;
  for(auto const&x:bData)std::cerr << x << std::endl;

  return 0;
}
