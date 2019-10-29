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


  auto const measure = [&](std::string const&name,size_t wgs,std::string const&src,size_t line,float full=0.f,float active = 0.f){
    std::stringstream ss;
    ss << "#version 450" << std::endl;
    ss << "#line " << line << std::endl;

    ss << "layout(local_size_x="<<wgs<<")in;\n";

    ss << "layout(binding=0)buffer Data{uint data[];};\n";
    ss << "void main(){\n";
    ss << "  uint wid = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;\n";
    ss << "  if("<< src <<")return;\n";
    ss << "  uint N = data[gl_WorkGroupID.x];\n";
    ss << "  uint counter = 0;\n";
    ss << "  for(uint i=0;i<N&&i<1000;++i)\n";
    ss << "    counter += i;\n";
    ss << "  if(counter == 1337)\n";
    ss << "    data[gl_GlobalInvocationID.x] = 100;\n";
    ss << "}\n";
    auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

    auto timer = Timer<float>();
    glFinish();
    timer.reset();
    prg->use();
    for(size_t i=0;i<N;++i)
      prg->dispatch(WORKGROUPS/1024,1024,1);
    glFinish();
    auto time = timer.elapsedFromStart()/N;
    size_t const alignLen = 60;
    size_t nofSpaces = alignLen >= name.size()? alignLen - name.size() : 0;
    std::cerr << wgs << " " << name;
    for(size_t i=0;i<nofSpaces;++i)
      std::cerr << " ";
    std::cerr << ": " << time;
    if(full != 0.f && active != 0.f){
      std::cerr << " - " << (time/full)*100 << "% - " << time/(full*active) << " times slower";
    }
    std::cerr << std::endl;
    return time;
  };
#define MEASURE(name,WGS,src,...) measure(name,WGS,src,__LINE__,__VA_ARGS__)

  MEASURE("warm up",64,"1==0",0.f);

  auto measureWGS = [&](size_t WGS){
    auto full = MEASURE("|****|",WGS,"1==0",0.f);
    MEASURE("|*.|"                                     ,WGS,"(wid%2)!=0"     ,full,1/2.f);
    MEASURE("|*...|"                                   ,WGS,"(wid%4)!=0"     ,full,1/4.f);
    MEASURE("|*...|....|"                              ,WGS,"(wid%8)!=0"     ,full,1/8.f);
    MEASURE("|*...|....|....|....|"                    ,WGS,"(wid%16)!=0"    ,full,1/16.f);
    MEASURE("|*...|....|....|....|....|....|....|....|",WGS,"(wid%32)!=0"    ,full,1/32.f);
    MEASURE("|.*..|"                                   ,WGS,"(wid%4)!=1"     ,full,1/4.f);
    MEASURE("|****|....|****|....|"                    ,WGS,"((wid/4)%2)!=0" ,full,1/2.f);
    MEASURE("|****|****|....|....|"                    ,WGS,"((wid/8)%2)!=0" ,full,1/2.f);
    MEASURE("|****|****|****|****|....|....|....|...|" ,WGS,"((wid/16)%2)!=0",full,1/2.f);
    MEASURE("40 out of 80"                             ,WGS,"((wid/40)%2)!=0",full,1/2.f);
    MEASURE("20 out of 40"                             ,WGS,"((wid/20)%2)!=0",full,1/2.f);
    MEASURE("10 out of 20"                             ,WGS,"((wid/10)%2)!=0",full,1/2.f);
    MEASURE("5 out of 10"                              ,WGS,"((wid/5)%2)!=0" ,full,1/2.f);
    MEASURE("1 out of 10"                              ,WGS,"(wid%10)!=0"    ,full,1/10.f);
    MEASURE("1 out of 5"                               ,WGS,"(wid%5)!=0"     ,full,1/5.f);
    MEASURE("1 out of 3"                               ,WGS,"(wid%3)!=0"     ,full,1/3.f);
  };

  measureWGS(WGS);


  return EXIT_SUCCESS;
}
