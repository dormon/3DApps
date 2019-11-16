#include <CL/cl.hpp>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Timer.h>
#include <iomanip>
#include <functional>
#include <cmath>

std::string err;

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
  protected:

};

using IsActive = std::function<bool(size_t)>;

std::string createName(IsActive const&isActive,size_t len = 80){
  std::string name;
  for(size_t i=0;i<len;++i)
    if(isActive(i))
      name += "*";
    else
      name += ".";
  return name;
}

std::vector<uint32_t>createActiveBufferData(IsActive const&isActive,size_t nofWorkgroups,size_t work){
  std::vector<uint32_t>data(nofWorkgroups);

  for(size_t i=0;i<nofWorkgroups;++i){
    if(isActive(i))data[i] = work;
    else data[i] = 0;
  }

  return data;
}

float computeActivePercentage(std::vector<uint32_t>const&d){
  size_t active = 0;
  for(auto const&x:d)
    if(x != 0)active++;
  return (float)active / (float)d.size();
}

void printMeasurement(float time,float full,float active,std::string const&name){
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

class CLContext{
  public:
    CLContext(Arguments const&a):args(a){
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
        err = ss.str();
        throw std::runtime_error(err);
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


    float measure(size_t wgs,std::function<bool(size_t)>const&isActive,float full=0.f){

      auto data = createActiveBufferData(isActive,args.nofWorkgroups,args.work);

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
    void printInfo()const{
      std::cout << "CL_PLATFORM_NAME: "<<platform.getInfo<CL_PLATFORM_NAME>()<< std::endl;
      std::cout << "CL_DEVICE_NAME  : "<<device  .getInfo<CL_DEVICE_NAME  >()<< std::endl;
    }
    Arguments const&args;
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
  auto context = CLContext(args);

  context.printInfo();

  context.measure(args.WGS,[](size_t){return true;},0.f);

  auto measureWGS = [&](size_t WGS){
    auto full = context.measure(WGS,[](size_t){return true;},0.f);
    context.measure(WGS,[](size_t w){return(w%4)     ==1;},full);
    for(size_t i=1;i<=40;++i)
      context.measure(WGS,[&](size_t w){return((w/i )%2)==0;},full);
    for(size_t i=1;i<=16;++i)
      context.measure(WGS,[&](size_t w){return(w%i )    ==0;},full);
  };

  measureWGS(args.WGS);

  return EXIT_SUCCESS;
}
