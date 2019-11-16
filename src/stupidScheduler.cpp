#include <SDL.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>
#include <iomanip>
#include <functional>
#include <sstream>
#include <CL/cl.hpp>

using namespace ge::gl;

class Arguments{
  public:
    Arguments(int argc,char*argv[]){
      auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
      tests         = args->getu64("-N",10,"how many times are tests executed");
      nofWorkgroups = args->getu64("-W",1024*1024,"how many workgroups to launch");
      work          = args->getu64("-I",100,"how much work is performed by each thread");
      WGS           = args->getu64("-S",64,"size of a workgroup");
      platformId    = args->getu32("-p",0,"platform id");
      deviceId      = args->getu32("-d",0,"device id");
      oglVersion    = args->getu32("-v",430,"version of opengl context");
      apiName       = args->gets  ("-a","opengl","selects api, could be: opengl/opencl");
      bool printHelp = args->isPresent("-h", "prints this help");
      if (printHelp || !args->validate()) {
        std::cout << "This benchmark was created by Tomáš Milet" << std::endl;
        std::cout << "email: imilet@fit.vutbr.cz" << std::endl;
        std::cout << std::endl;
        std::cout << args->toStr();
        exit(0);
      }
    }
    uint64_t tests        ;
    uint64_t nofWorkgroups;
    uint64_t work         ;
    uint64_t WGS          ;
    uint32_t platformId   ;
    uint32_t deviceId     ;
    uint32_t oglVersion   ;
    std::string apiName   ;
  protected:

};

class API{
  public:
    using IsActive = std::function<bool(size_t)>;
    API(Arguments const&args):args(args){}
    virtual ~API(){}
    virtual void printInfo() const = 0;
    virtual float measure(IsActive const&isActive,float full=0.f) const = 0;
  protected:
    std::string createName(IsActive const&isActive,size_t len = 80) const{
      std::string name;
      for(size_t i=0;i<len;++i)
        if(isActive(i))
          name += "*";
        else
          name += ".";
      return name;
    }
    
    std::vector<uint32_t>createActiveBufferData(IsActive const&isActive) const{
      std::vector<uint32_t>data(args.nofWorkgroups);
    
      for(size_t i=0;i<args.nofWorkgroups;++i){
        if(isActive(i))data[i] = args.work;
        else data[i] = 0;
      }
    
      return data;
    }
    
    float computeActivePercentage(std::vector<uint32_t>const&d) const{
      size_t active = 0;
      for(auto const&x:d)
        if(x != 0)active++;
      return (float)active / (float)d.size();
    }
    
    void printMeasurement(float time,float full,float active,std::string const&name) const{
        size_t const alignLen = 60;
        size_t nofSpaces = alignLen >= name.size()? alignLen - name.size() : 0;
        std::cout << name;
        for(size_t i=0;i<nofSpaces;++i)
          std::cout << " ";
        std::cout << ": " << std::fixed << std::setprecision(5) << time;
        if(full != 0.f && active != 0.f){
            std::cout << " active : " << std::setfill(' ') << std::setw(6) << std::setprecision(2) << active*100 << "%";
            std::cout << " 1/" << std::setfill(' ') << std::setw(2) << (uint32_t)roundf(1.f/active);
            std::cout << " time   : " << std::setfill(' ') << std::setw(6) << std::setprecision(2) << (time/full)*100 << "%";
            float dist = (1-time/(full*active));
            if(dist < 0)dist*=-1;
            if(dist < 0.05)std::cout << " good";
            else std::cout << " " << time/(full*active) << " x slower";
        }
        std::cout << std::endl;
    }
    Arguments const&args;
};



class OpenGLContext: public API{
  public:
    OpenGLContext(Arguments const&args):API(args){
      if(SDL_Init(SDL_INIT_EVERYTHING)<0)
        throw std::runtime_error(SDL_GetError());
      window = SDL_CreateWindow("",0,0,1,1,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
      if(!window)
        throw std::runtime_error(SDL_GetError());
      if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, args.oglVersion / 100) < 0)
        throw std::runtime_error(SDL_GetError());
      if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (args.oglVersion % 100) / 10) < 0)
        throw std::runtime_error(SDL_GetError());
      context = SDL_GL_CreateContext(window);
      if(!context)
        throw std::runtime_error(SDL_GetError());

      ge::gl::init();

      buf = std::make_shared<ge::gl::Buffer>(args.nofWorkgroups*sizeof(uint32_t));
      buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);

      atomicCounter = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t));
      atomicCounter->bindBase(GL_SHADER_STORAGE_BUFFER,1);
    }


    float measure(IsActive const&isActive,float full=0.f)const override{
      std::stringstream ss;
      ss << "#version " << args.oglVersion << std::endl;
      ss << "layout(local_size_x="<<args.WGS<<")in;" << std::endl;
      ss << R".(
      layout(binding=0)buffer Data{uint data[];};
      layout(binding=1)buffer Counter{uint atomicCounter;};
      void main(){
        uint wid = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
        uint N = data[wid];
        uint counter = 0;
        for(uint i=0;i<N&&i<10000;++i)
          counter += N*counter;
        if(counter == 1337)
          data[wid] = 100;
        if(gl_LocalInvocationID.x == 0)
          atomicAdd(atomicCounter,1);
      }
      ).";
      auto prg = std::make_shared<Program>(std::make_shared<Shader>(GL_COMPUTE_SHADER,ss.str()));

      auto data = createActiveBufferData(isActive);

      buf->setData(data);

      float const active = computeActivePercentage(data);

      auto const name = createName(isActive);

      uint32_t aaa = 0;
      atomicCounter->setData(&aaa,sizeof(aaa));
      glFinish();

      auto timer = Timer<float>();
      prg->use();
      glFinish();
      timer.reset();
      for(size_t i=0;i<args.tests;++i){
        prg->dispatch(args.nofWorkgroups/1024,1024,1);
        glFinish();
      }
      glFinish();

      auto time = timer.elapsedFromStart()/args.tests;

      std::vector<uint32_t>at(1);
      atomicCounter->getData(at);
      if(at.at(0) != args.nofWorkgroups*args.tests)
        std::cout << "SOMETHING FISHY IS GOING ON! " << at.at(0) << " - " << args.nofWorkgroups*args.tests << std::endl;

      printMeasurement(time,full,active,name);

      return time;
    }
    void printInfo()const override{
      std::cout << "GL_RENDERER                : " << glGetString(GL_RENDERER                ) << std::endl;
      std::cout << "GL_VENDOR                  : " << glGetString(GL_VENDOR                  ) << std::endl;
      std::cout << "GL_VERSION                 : " << glGetString(GL_VERSION                 ) << std::endl;
      std::cout << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    }
    SDL_Window*window;
    SDL_GLContext context;
    std::shared_ptr<ge::gl::Buffer>buf;
    std::shared_ptr<ge::gl::Buffer>atomicCounter;
};

class OpenCLContext: public API{
  public:
    OpenCLContext(Arguments const&a):API(a){
      //get all platforms (drivers)
      std::vector<cl::Platform> all_platforms;
      cl::Platform::get(&all_platforms);
      if(all_platforms.size()==0)
        throw std::runtime_error("No platforms found. Check OpenCL installation!");


      if(args.platformId >= all_platforms.size()){
        std::stringstream ss;
        ss << "Cannot select platfrom: " << args.platformId << " - out of range" << std::endl;
        ss << "available platforms: " << std::endl;
        
        for(size_t i=0;i<all_platforms.size();++i)
         ss << i << " - " << all_platforms.at(i).getInfo<CL_PLATFORM_NAME>() << std::endl;
        ss << "#####" << std::endl;
        throw std::runtime_error(ss.str());
      }

      platform=all_platforms[args.platformId];

      //get default device of the default platform
      std::vector<cl::Device> all_devices;
      platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
      if(all_devices.size()==0)
        throw std::runtime_error("No devices found. Check OpenCL installation!");


      if(args.deviceId >= all_devices.size()){
        std::stringstream ss;
        ss << "Cannot select device: " << args.platformId << " - out of range" << std::endl;
        ss << "available devices: " << std::endl;
        
        for(size_t i=0;i<all_devices.size();++i)
         ss << i << " - " << all_devices.at(i).getInfo<CL_DEVICE_NAME>() << std::endl;
        throw std::runtime_error(ss.str());
      }


      device=all_devices[args.deviceId];

      context = cl::Context({device});


      cl::Program::Sources sources;

      // kernel calculates for each element C=A+B
      std::string kernel_code=
      R".(
      void kernel compute(global uint* data,global uint* atomicCounter){
        uint wid = get_global_id(1) * get_local_size(0) + get_global_id(0);
        uint N = data[wid];
        uint counter = 0;
        for(uint i=0;i<N&&i<10000;++i)
          counter += N*counter;
        if(counter == 1337)
          data[wid] = 100;
        if(get_local_id(0) == 0)
          atomic_add(atomicCounter,1);
      }
      ).";

      sources.push_back({kernel_code.c_str(),kernel_code.length()});

      program = cl::Program(context,sources);

      if(program.build({device})!=CL_SUCCESS){
          std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device)<<"\n";
          exit(1);
      }


      // create buffers on the device
      buffer = cl::Buffer(context,CL_MEM_READ_WRITE,sizeof(uint32_t)*args.nofWorkgroups);
      atomicCounter = cl::Buffer(context,CL_MEM_READ_WRITE,sizeof(uint32_t));

      //create queue to which we will push commands for the device.
      queue = cl::CommandQueue(context,device);

      //alternative way to run the kernel
      kernel=cl::Kernel(program,"compute");
      kernel.setArg(0,buffer);
      kernel.setArg(1,atomicCounter);
    }


    float measure(std::function<bool(size_t)>const&isActive,float full=0.f) const override{

      auto data = createActiveBufferData(isActive);

      auto active = computeActivePercentage(data);

      //write arrays A and B to the device
      queue.enqueueWriteBuffer(buffer,CL_TRUE,0,sizeof(decltype(data)::value_type)*data.size(),data.data());

      auto const name = createName(isActive);

      uint32_t aa=0;
      queue.enqueueWriteBuffer(atomicCounter,CL_TRUE,0,sizeof(uint32_t),&aa);

      auto timer = Timer<float>();
      queue.finish();
      timer.reset();
      auto globalSize = cl::NDRange(args.nofWorkgroups*args.WGS/1024,1024,1);
      auto localSize = cl::NDRange(args.WGS,1,1);
      for(size_t i=0;i<args.tests;++i)
        queue.enqueueNDRangeKernel(kernel,cl::NullRange,globalSize,localSize);
      queue.finish();
      auto time = timer.elapsedFromStart()/args.tests;

      uint32_t bb=0;
      queue.enqueueReadBuffer(atomicCounter,CL_TRUE,0,sizeof(uint32_t),&bb);
      if(bb != args.nofWorkgroups*args.tests)
        std::cerr << "SOMETHING FISHY IS GOING ABOUT! " << bb << " - " << args.nofWorkgroups * args.tests << std::endl;

      printMeasurement(time,full,active,name);
      return time;
    };
    void printInfo()const override{
      std::cout << "CL_PLATFORM_NAME: "<<platform.getInfo<CL_PLATFORM_NAME>()<< std::endl;
      std::cout << "CL_DEVICE_NAME  : "<<device  .getInfo<CL_DEVICE_NAME  >()<< std::endl;
    }
    cl::Platform     platform     ;
    cl::Device       device       ;
    cl::Context      context      ;
    cl::Buffer       buffer       ;
    cl::Buffer       atomicCounter;
    cl::Program      program      ;
    cl::Kernel       kernel       ;
    cl::CommandQueue queue        ;
};



int main(int argc,char*argv[]){
  auto args    = Arguments(argc,argv);
  std::shared_ptr<API>api;

  if(args.apiName == "opengl")
    api = std::make_shared<OpenGLContext>(args);
  if(args.apiName == "opencl")
    api = std::make_shared<OpenCLContext>(args);


  api->printInfo();

  //warm up
  api->measure([](size_t){return true;},0.f);

  auto measureWGS = [&](std::shared_ptr<API>const&api){
    auto full = api->measure([](size_t){return true;},0.f);
    for(size_t i=1;i<=40;++i)
      api->measure([&](size_t w){return((w/i)%2)==0;},full);
    for(size_t i=1;i<=40;++i)
      api->measure([&](size_t w){return (w%i)   ==0;},full);
  };

  measureWGS(api);


  return EXIT_SUCCESS;
}
