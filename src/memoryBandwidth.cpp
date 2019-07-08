#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <sstream>
#include <Timer.h>
#include <glm/glm.hpp>

using namespace std;
using namespace ge::gl;

class MemoryBandwidth: public simple3DApp::Application{
 public:
  MemoryBandwidth(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~MemoryBandwidth(){}
  virtual void draw() override;

  vars::Vars vars;
  vars::Vars limits;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};

void createReadProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"floatsPerThread","registersPerThread","workGroupSize","useReadProgram"}))return;

  std::cerr << "need to recompile program" << std::endl;

  auto const floatsPerThread    = vars.getSizeT("floatsPerThread"   );
  auto const registersPerThread = vars.getSizeT("registersPerThread");
  auto const workGroupSize      = vars.getSizeT("workGroupSize"     );

  stringstream ss;
  ss << "#version                     " << 450                << endl;
  ss << "#line                        " << __LINE__           << endl;
  ss << "#define FLOATS_PER_THREAD    " << floatsPerThread    << endl;
  ss << "#define REGISTERS_PER_THREAD " << registersPerThread << endl;
  ss << "#define WORKGROUP_SIZE       " << workGroupSize      << endl;
  ss << R".(

  #define FLOAT_CHUNKS (FLOATS_PER_THREAD / REGISTERS_PER_THREAD)

  layout(local_size_x=WORKGROUP_SIZE)in;
  layout(binding=0,std430)buffer Data{float data[];};

  void main(){
    uint lid  = gl_LocalInvocationID .x;
    uint gid  = gl_GlobalInvocationID.x;
    uint wgs  = gl_WorkGroupSize     .x;
    uint wid  = gl_WorkGroupID       .x;
    uint nwgs = gl_NumWorkGroups     .x;
    const uint workGroupOffset = wid*FLOATS_PER_THREAD*WORKGROUP_SIZE;
    float accumulator = 0.f;

    #if REGISTERS_PER_THREAD != 0

      float registers[REGISTERS_PER_THREAD];
      for(uint r=0;r<REGISTERS_PER_THREAD;++r)
        registers[r] = 0.f;

      for(uint f=0;f<FLOAT_CHUNKS;++f)
        for(uint r=0;r<REGISTERS_PER_THREAD;++r)
          registers[r] += data[lid + (f*REGISTERS_PER_THREAD+r)*wgs + workGroupOffset];
      for(uint r=0;r<REGISTERS_PER_THREAD;++r)
        accumulator += registers[r];

    #else

      for(uint f=0;f<FLOATS_PER_THREAD;++f)
        accumulator += data[lid + f*wgs + workGroupOffset];

    #endif

    if(accumulator == 1.337f)
      data[gid] = 0.f;
  }
  ).";

  vars.reCreate<Program>("program",make_shared<Shader>(GL_COMPUTE_SHADER,ss.str()));
}

void createWriteProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"floatsPerThread","workGroupSize","useReadProgram"}))return;
  std::cerr << "need to recompile program" << std::endl;

  auto const floatsPerThread    = vars.getSizeT("floatsPerThread"   );
  auto const workGroupSize      = vars.getSizeT("workGroupSize"     );

  stringstream ss;
  ss << "#version                  " << 450             << endl;
  ss << "#line                     " << __LINE__        << endl;
  ss << "#define FLOATS_PER_THREAD " << floatsPerThread << endl;
  ss << "#define WORKGROUP_SIZE    " << workGroupSize   << endl;
  ss << R".(

  layout(local_size_x=WORKGROUP_SIZE)in;
  layout(binding=0,std430)buffer Data{float data[];};

  void main(){
    uint lid  = gl_LocalInvocationID .x;
    uint gid  = gl_GlobalInvocationID.x;
    uint wgs  = gl_WorkGroupSize     .x;
    uint wid  = gl_WorkGroupID       .x;
    uint nwgs = gl_NumWorkGroups     .x;
    const uint workGroupOffset = wid*FLOATS_PER_THREAD*WORKGROUP_SIZE;

    for(uint f=0;f<FLOATS_PER_THREAD;++f)
      data[lid + f*wgs + workGroupOffset] = lid + f*wgs + workGroupOffset;
  }
  ).";
  vars.reCreate<Program>("program",make_shared<Shader>(GL_COMPUTE_SHADER,ss.str()));
}

void createProgram(vars::Vars&vars){
  if(vars.getBool("useReadProgram"))
    createReadProgram(vars);
  else
    createWriteProgram(vars);
}

void createBuffer(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"floatsPerThread","workGroupSize","nofWorkGroups"}))return;
  std::cerr << "need to reallocate buffer" << std::endl;

  auto const workGroupSize   = vars.getSizeT("workGroupSize"  );
  auto const nofWorkGroups   = vars.getSizeT("nofWorkGroups"  );
  auto const floatsPerThread = vars.getSizeT("floatsPerThread");

  size_t const bufferSize = workGroupSize * nofWorkGroups * floatsPerThread;
  vars.reCreate<Buffer>("buffer",bufferSize);
}

void MemoryBandwidth::init(){
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addBool("useReadProgram"     ,false);
  vars.addSizeT("workGroupSize"     ,128  );
  vars.addSizeT("nofWorkGroups"     ,28   );
  vars.addSizeT("floatsPerThread"   ,1024 );
  vars.addSizeT("registersPerThread",0    );
}

void MemoryBandwidth::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  createProgram(vars);
  createBuffer(vars);

  auto buffer  = vars.get<Buffer >("buffer" );
  auto program = vars.get<Program>("program");

  //auto qr = make_shared<AsynchronousQuery>(GL_TIME_ELAPSED,GL_QUERY_RESULT,AsynchronousQuery::UINT64);

  buffer->clear(GL_R32F,GL_RED,GL_FLOAT);
  program->use();
  program->bindBuffer("Data",buffer);
  glFinish();
  auto timer = Timer<double>();
  timer.reset();
  program->dispatch(vars.getSizeT("nofWorkGroups"));
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
  glFinish();
  auto time = timer.elapsedFromStart();

  auto&nofWorkGroups      = vars.getSizeT("nofWorkGroups");
  auto&workGroupSize      = vars.getSizeT("workGroupSize");
  auto&floatsPerThread    = vars.getSizeT("floatsPerThread");
  auto&registersPerThread = vars.getSizeT("registersPerThread");
  auto&useReadProgram     = vars.getBool("useReadProgram");

  uint64_t const nanoSecondsInSecond = 1e9;
  uint64_t const gigabyte = 1024*1024*1024;
  size_t minWorkGroupSize      = 1        ;
  size_t maxWorkGroupSize      = 1024     ;
  size_t minFloatsPerThread    = 1        ;
  size_t maxFloatsPerThread    = 100000   ;
  size_t minNofWorkGroups      = 1        ;
  size_t maxNofWorkGroups      = 1024*1024;
  size_t minRegistersPerThread = 0        ;
  size_t maxRegistersPerThread = 4096     ;


  double bandwidthInGigabytes = 0.;

  //time = static_cast<double>(timeInNanoseconds) / static_cast<double>(nanoSecondsInSecond);
  auto const bufferSize = nofWorkGroups * workGroupSize * floatsPerThread * sizeof(float);
  auto const bandwidth = bufferSize / time;
  bandwidthInGigabytes = bandwidth / static_cast<double>(gigabyte);

  /*
  ImGui::Checkbox("use read program",&useReadProgram);
  ImGui::DragScalar("floatsPerThread"     ,ImGuiDataType_U64,&floatsPerThread   ,1,&minFloatsPerThread   ,&maxFloatsPerThread   );
  ImGui::DragScalar("workGroupSize"       ,ImGuiDataType_U64,&workGroupSize     ,1,&minWorkGroupSize     ,&maxWorkGroupSize     );
  ImGui::DragScalar("number of workgroups",ImGuiDataType_U64,&nofWorkGroups     ,1,&minNofWorkGroups     ,&maxNofWorkGroups     );
  if(useReadProgram)
    ImGui::DragScalar("registers per thread",ImGuiDataType_U64,&registersPerThread,1,&minRegistersPerThread,&maxRegistersPerThread);
  // */

  ImGui::Begin("perf");
  ImGui::Text("buffer Size : %f [GB]"   ,static_cast<float>(workGroupSize * nofWorkGroups * floatsPerThread * sizeof(float))/static_cast<float>(gigabyte));
  ImGui::Text("time        : %lf [s]"   ,time);
  ImGui::Text("bandwidth   : %lf [GB/s]",bandwidthInGigabytes);
  ImGui::End();

  drawImguiVars(vars);//,std::move(limits));

  swap();
}

void MemoryBandwidth::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  MemoryBandwidth app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
