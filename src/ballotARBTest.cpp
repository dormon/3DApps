#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <TxtUtils/TxtUtils.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>
#include <bitset>

class CSCompiler: public simple3DApp::Application{
 public:
  CSCompiler(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~CSCompiler(){}
};

using namespace ge::gl;

size_t getWavefrontSize(size_t w) {
  if (w != 0) return w;
  std::string renderer = std::string((char*)ge::gl::glGetString(GL_RENDERER));
  std::string vendor   = std::string((char*)ge::gl::glGetString(GL_VENDOR));
  std::cout << renderer << std::endl;
  std::cout << vendor << std::endl;
  if (vendor.find("AMD") != std::string::npos ||
      renderer.find("AMD") != std::string::npos || 
	  vendor.find("ATI") != std::string::npos ||
	  renderer.find("ATI") != std::string::npos)
    return 64;
  else if (vendor.find("NVIDIA") != std::string::npos ||
           renderer.find("NVIDIA") != std::string::npos)
    return 32;
  else {
    std::cerr << "WARNING: renderer is not NVIDIA or AMD, setting "
                 "wavefrontSize to 32"
              << std::endl;
    return 32;
  }
}

int main(int argc,char*argv[]){
  CSCompiler app{argc, argv};

  auto warp = getWavefrontSize(0);

  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*2*20);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  buf->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);


  std::stringstream ss;
  ss << "#version 450" << std::endl;
  ss << "#line " << __LINE__ << std::endl;
  ss << "#define WARP " << warp << std::endl;
  ss << R".(

  #extension GL_ARB_gpu_shader_int64 : enable
  #if !defined(GL_ARB_gpu_shader_int64)
    #extension GL_AMD_gpu_shader_int64 : enable
  #endif

  #extension GL_ARB_shader_ballot : enable

  layout(local_size_x=WARP)in;

  layout(binding=0)buffer Data{uint data[];};

  //gl_SubGroupSizeARB
  void main(){
    uint64_t d = ballotARB(gl_LocalInvocationID.x%2 == 0);
    if(gl_LocalInvocationID.x==0){
      data[0] = unpackUint2x32(d)[0];
      data[1] = unpackUint2x32(d)[1];
      d <<= 1u;
      data[2] = unpackUint2x32(d)[0];
      data[3] = unpackUint2x32(d)[1];
      d <<= 1u;
      data[4] = unpackUint2x32(d)[0];
      data[5] = unpackUint2x32(d)[1];
      d &= uint64_t(0xFF00FF00FF00FF00ul);
      data[6] = unpackUint2x32(d)[0];
      data[7] = unpackUint2x32(d)[1];
      d >>= 40;
      data[8] = unpackUint2x32(d)[0];
      data[9] = unpackUint2x32(d)[1];
      //data[10] = findLSB(d);
      //data[11] = findMSB(d);
      //data[12] = bitCount(d);
      //data[13] = bitCount(d);
    }
  }

  ).";
  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

  prg->use();
  prg->dispatch(1);
  glFinish();

  std::vector<uint32_t>data;
  buf->getData(data);

  for(size_t i=0;i<data.size();i+=2){
    std::cerr << std::bitset<32>(data[i+1]) << " ";
    std::cerr << std::bitset<32>(data[i+0]) << std::endl;
  }

  return EXIT_SUCCESS;
}
