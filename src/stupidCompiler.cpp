#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <TxtUtils/TxtUtils.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>

class CSCompiler: public simple3DApp::Application{
 public:
  CSCompiler(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~CSCompiler(){}
};

using namespace ge::gl;

class Prg{
  public:
    Prg(std::string n,std::string const&src){
      prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src));
      name = n;
    }
  protected:
    std::shared_ptr<Program>prg;
    std::string name;
};

int main(int argc,char*argv[]){
  CSCompiler app{argc, argv};

  size_t const DATA = 1024*1024*4;
  size_t const N = 10;
  auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*DATA);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto const measure = [&](std::string const&name,std::string const&src){
    auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src));

    auto timer = Timer<float>();
    glFinish();
    timer.reset();
    for(size_t i=0;i<N;++i)
      prg->dispatch(DATA/256);
    glFinish();
    auto time = timer.elapsedFromStart()/N;
    std::cerr << name << ": " << time << std::endl;
  };

  measure("warm up",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");

  measure("simple",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");

  measure("constLoop",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a = uint(32*sin(a*32));
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");

  measure("constLoop2",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a += i;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");

  measure("constLoop3",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a += sin(i*32)*32;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");


  measure("complex",R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define C1(x,y)  (x      +y      )
  #define C2(x,y)  (C1(x,y)+C1(x,y))
  #define C4(x,y)  (C2(x,y)+C2(x,y))
  #define C8(x,y)  (C4(x,y)+C4(x,y))
  #define C16(x,y) (C8(x,y)+C8(x,y))
  #define C32(x,y) (C16(x,y)+C16(x,y))
  #define C64(x,y) (C32(x,y)+C32(x,y))
  #define C128(x,y) (C64(x,y)+C64(x,y))
  #define C256(x,y) (C128(x,y)+C128(x,y))
  #define C512(x,y) (C256(x,y)+C256(x,y))
  #define C1024(x,y) (C512(x,y)+C512(x,y))

  void main(){
    data[gl_GlobalInvocationID.x] = C1024(23,10);
  }

  ).");

  //auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  //auto file = args->gets("--file","","file that contains compute shader source");
  //bool printHelp = args->isPresent("-h", "prints this help");
  //if (printHelp || !args->validate()) {
  //  std::cerr << args->toStr();
  //  exit(0);
  //}

  //auto const src = txtUtils::loadTextFile(file);

  //auto cs = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src);
  //auto prg = std::make_shared<ge::gl::Program>(cs);

  return EXIT_SUCCESS;
}
