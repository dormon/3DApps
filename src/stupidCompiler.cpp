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

int main(int argc,char*argv[]){
  CSCompiler app{argc, argv};

  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto N = args->getu64("-N",10,"how many times are tests executed");
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  size_t const DATA = 1024*1024*4;
  auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*DATA);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto const measure = [&](std::string const&name,std::string const&src,size_t line){
    std::stringstream ss;
    ss << "#version 450" << std::endl;
    ss << "#line " << line << std::endl;
    ss << src;
    auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

    auto timer = Timer<float>();
    glFinish();
    timer.reset();
    prg->use();
    for(size_t i=0;i<N;++i)
      prg->dispatch(DATA/256);
    glFinish();
    auto time = timer.elapsedFromStart()/N;
    std::cerr << name << ": " << time << std::endl;
  };
#define MEASURE(name,src) measure(name,src,__LINE__)

  {MEASURE("warm up",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");}

  //std::vector<uint32_t>d(10);
  //buf->getData(d);
  //for(auto x:d)
  //  std::cerr << x << std::endl;

  {MEASURE("simple",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");}

  {MEASURE("constLoop",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a = uint(32*sin(a*32));
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("constLoop2",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    
    for(uint i=0;i<1000;++i)
      a += i;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("constLoop3",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a += uint(sin(i*32)*32);
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("if(false) a lot",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    uint a  = 32;
    DUP4096(if(0>1)a += 1;)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("for(uint i=0;i<10000;++i){if(0>1)a+=1;}",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;

    for(uint i=0;i<10000;++i)
      if(0>1)a += 1;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("for(uint i=0;i<10000;++i){if(i>100000)a+=1;}",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;

    for(uint i=0;i<10000;++i)
      if(i>100000)a += 1;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("a lot of addition",R".(
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

  ).");}

  {MEASURE("function",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    return a*2;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("function loop",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    for(uint i=0;i<1000;++i)
      a += uint(sin(i*32)*32);
    return a;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("function",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    uint b = a*2;
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b += 1;
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    return b;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("variables",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a = 32;
    a *= 32;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    data[gl_GlobalInvocationID.x] = a;
  }

  ).");}

  {MEASURE("const <",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = uint(23*sin(21*float(gl_WorkGroupSize.x < 300)));
  }

  ).");}

  return EXIT_SUCCESS;
}
