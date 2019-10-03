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


  //size_t const DATA = 1024*1024*4;
  //auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*DATA);
  //buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);


  std::string const src = R".(
  #version 450

  layout(local_size_x=256)in;

  layout(binding=0)buffer A{float a[];};
  layout(binding=1)buffer B{float b[];};
  layout(binding=2)buffer C{float c[];};

  void main(){
    c[gl_GlobalInvocationID.x] = a[gl_GlobalInvocationID.x] + b[gl_GlobalInvocationID.x];
  }

  ).";

  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src));

  size_t N = 1024*1024;

  auto aData = std::vector<float>(N);
  auto bData = std::vector<float>(N);
  auto cData = std::vector<float>(N);

  for(size_t i=0;i<N;++i){
    aData[i] = i*2;
    bData[i] = i*7;
  }


  auto A = std::make_shared<Buffer>(aData);
  auto B = std::make_shared<Buffer>(bData);
  auto C = std::make_shared<Buffer>(N*sizeof(float));

  A->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  B->bindBase(GL_SHADER_STORAGE_BUFFER,1);
  C->bindBase(GL_SHADER_STORAGE_BUFFER,2);

  prg->use();
  glFinish();

  auto timer = Timer<float>();
  timer.reset();

  size_t M=100;

  for(size_t i=0;i<M;++i)
    prg->dispatch(N/256,1,1);
  glFinish();

  auto time = timer.elapsedFromStart();
  std::cerr << "gpu: " << time/M << std::endl;


  C->getData(cData);

  timer.reset();
  for(size_t i=0;i<N;++i)
    cData[i] = aData[i] + bData[i];
  auto cpuTime = timer.elapsedFromStart();
  std::cerr << "cpu: " << cpuTime << std::endl;



  /*
  size_t N= 10;

  glFinish();
  auto timer = Timer<float>();
  timer.reset();
  prg->use();
  for(size_t i=0;i<N;++i)
    prg->dispatch(DATA/256);
  glFinish();
  auto time = timer.elapsedFromStart()/N;
  std::cerr << "cas: " << time << std::endl;


  */

  return EXIT_SUCCESS;
}
