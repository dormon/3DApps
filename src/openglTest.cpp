#include <SDL.h>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>

#define STRINGIFY(S) STRINGIFY2(S)
#define STRINGIFY2(S) #S

#define PRINT_AND_EXECUTE(IMPL)\
std::cerr << __FILE__ << "/" << __LINE__ << ":   " << STRINGIFY(IMPL) << std::endl;\
IMPL

int main(int argc,char*argv[]){
  PRINT_AND_EXECUTE(size_t v = 330);
  PRINT_AND_EXECUTE(if(argc>1)v = atoi(argv[1]));
  PRINT_AND_EXECUTE(if(SDL_Init(SDL_INIT_EVERYTHING)<0)std::cerr << SDL_GetError() << std::endl);
  PRINT_AND_EXECUTE(auto window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN));
  PRINT_AND_EXECUTE(if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, v / 100) < 0)std::cerr << SDL_GetError() << std::endl);
  PRINT_AND_EXECUTE(if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (v % 100) / 10) < 0)std::cerr << SDL_GetError() << std::endl);
  PRINT_AND_EXECUTE(auto ctx = SDL_GL_CreateContext(window));
  PRINT_AND_EXECUTE(if(ctx == nullptr)std::cerr << SDL_GetError() << std::endl);
  PRINT_AND_EXECUTE(ge::gl::init());
  PRINT_AND_EXECUTE(std::cerr << "RENDERER: " << ge::gl::glGetString(GL_RENDERER) << std::endl);
  PRINT_AND_EXECUTE(std::cerr << "VENDOR  : " << ge::gl::glGetString(GL_VENDOR  ) << std::endl);
  PRINT_AND_EXECUTE(std::cerr << "VERSION : " << ge::gl::glGetString(GL_VERSION ) << std::endl);
  return 0;
}
