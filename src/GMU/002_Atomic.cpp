#include <Simple3DApp/Application.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>

class GMU: public simple3DApp::Application{
 public:
  GMU(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~GMU(){}
};

using namespace ge::gl;

int main(int argc,char*argv[]){
  GMU app{argc, argv};

  std::vector<uint32_t>data = {0};
  auto buffer = std::make_shared<Buffer>(data);
  buffer->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  std::string const nonAtomicSrc = R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer A{uint a[];};
  
  uniform uint vectorSize = 256;

  void main(){
    a[0] = a[0] + gl_GlobalInvocationID.x;
  }

  ).";

  auto nonAtomicPrg = std::make_shared<Program>(std::make_shared<Shader>(GL_COMPUTE_SHADER,nonAtomicSrc));

  data[0] = 0;
  buffer->setData(data);
  glFinish();

  nonAtomicPrg->use();
  nonAtomicPrg->dispatch(1,1,1);
  glFinish();

  data[0] = 0;
  buffer->getData(data);

  std::cerr << "non atomic: " << data[0] << std::endl;


  
  std::string const atomicSrc = R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer A{uint a[];};
  
  uniform uint vectorSize = 256;

  void main(){
    atomicAdd(a[0],gl_GlobalInvocationID.x);
  }

  ).";

  auto atomicPrg = std::make_shared<Program>(std::make_shared<Shader>(GL_COMPUTE_SHADER,atomicSrc));

  data[0] = 0;
  buffer->setData(data);
  glFinish();

  atomicPrg->use();
  atomicPrg->dispatch(1,1,1);
  glFinish();

  data[0] = 0;
  buffer->getData(data);

  std::cerr << "atomic: " << data[0] << std::endl;


  return EXIT_SUCCESS;
}
