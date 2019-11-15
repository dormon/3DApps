#include <SDL.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>
#include <iomanip>
#include <functional>

using namespace ge::gl;

int main(int argc,char*argv[]){
  uint32_t version = 430;
  if(SDL_Init(SDL_INIT_EVERYTHING)<0)
    throw std::runtime_error(SDL_GetError());
  auto window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if(!window)
    throw std::runtime_error(SDL_GetError());
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, version / 100) < 0){
    throw std::runtime_error(SDL_GetError());
    std::cerr << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    return 0;
  }
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (version % 100) / 10) < 0){
    std::cerr << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    return 0;
  }
  auto ctx = SDL_GL_CreateContext(window);
  if(!ctx){
    std::cerr << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    return 0;
  }


  ge::gl::init();


  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto N = args->getu64("-N",10,"how many times are tests executed");
  auto WORKGROUPS = args->getu64("-W",1024*1024,"how many workgroups to launch");
  auto ITERATIONS = args->getu64("-I",100,"how much work is performed by each thread");
  auto WGS        = args->getu64("-S",64,"size of a workgroup");
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  std::vector<uint32_t>data(WORKGROUPS);
  for(auto&x:data)x = ITERATIONS;
  auto buf = std::make_shared<ge::gl::Buffer>(data.size()*sizeof(decltype(data)::value_type));
  buf->setData(data);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);


  auto const measure = [&](size_t wgs,std::function<bool(size_t)>const&isActive,float full=0.f){
    std::stringstream ss;
    ss << "#version 450" << std::endl;

    ss << "layout(local_size_x="<<wgs<<")in;\n";

    ss << "layout(binding=0)buffer Data{uint data[];};\n";
    ss << "void main(){\n";
    ss << "  uint wid = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;\n";
    ss << "  uint N = data[wid];\n";
    ss << "  uint counter = 0;\n";
    ss << "  for(uint i=0;i<N&&i<10000;++i)\n";
    ss << "    counter += N*counter;\n";
    ss << "  if(counter == 1337)\n";
    ss << "    data[wid] = 100;\n";
    ss << "}\n";
    auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

    std::vector<uint32_t>data(WORKGROUPS);

    size_t activeWG = 0;
    for(size_t i=0;i<WORKGROUPS;++i){
      if(isActive(i)){
        data[i] = ITERATIONS;
        activeWG++;
      }else
        data[i] = 0;
    }
    buf->setData(data);
    float active = (float)activeWG / (float)WORKGROUPS;

    std::string nname;
    for(size_t i=0;i<80;++i)
      if(isActive(i))
        nname += "*";
      else
        nname += ".";


    auto timer = Timer<float>();
    prg->use();
    glFinish();
    timer.reset();
    for(size_t i=0;i<N;++i){
      prg->dispatch(WORKGROUPS/1024,1024,1);
      glFinish();
    }

    //std::vector<uint32_t>aa;
    //buf->getData(aa);
    //for(int i=0;i<10;++i)
    //  std::cerr << aa[i] << " ";
    //std::cerr << std::endl;
    auto time = timer.elapsedFromStart()/N;
    size_t const alignLen = 60;
    size_t nofSpaces = alignLen >= nname.size()? alignLen - nname.size() : 0;
    std::cerr << wgs << " " << nname;
    for(size_t i=0;i<nofSpaces;++i)
      std::cerr << " ";
    std::cerr << ": " << std::fixed << std::setprecision(5) << time;
    if(full != 0.f && active != 0.f){
        std::cerr << " active : " << std::setfill(' ') << std::setw(6) << std::setprecision(2) << active*100 << "%";
        std::cerr << " 1/" << std::setfill(' ') << std::setw(2) << (uint32_t)roundf(1.f/active);
        std::cerr << " time   : " << std::setfill(' ') << std::setw(6) << std::setprecision(2) << (time/full)*100 << "%";
        float dist = (1-time/(full*active));
        if(dist < 0)dist*=-1;
        if(dist < 0.05)std::cerr << " good";
        else std::cerr << " " << time/(full*active) << " x slower";
    }
    std::cerr << std::endl;
    return time;
  };
#define MEASURE(WGS,...) measure(WGS,__VA_ARGS__)

  MEASURE(64,[](size_t){return true;},0.f);

  auto measureWGS = [&](size_t WGS){
    auto full = MEASURE(WGS,[](size_t){return true;},0.f);
    MEASURE(WGS,[](size_t w){return(w%4)     ==1;},full);
    for(size_t i=1;i<=40;++i)
      MEASURE(WGS,[&](size_t w){return((w/i )%2)==0;},full);
    for(size_t i=1;i<=16;++i)
      MEASURE(WGS,[&](size_t w){return(w%i )    ==0;},full);
  };

  measureWGS(WGS);


  return EXIT_SUCCESS;
}
