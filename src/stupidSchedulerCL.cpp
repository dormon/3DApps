#include <CL/cl.hpp>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Timer.h>
#include <iomanip>
#include <functional>
#include <cmath>

int main(int argc,char*argv[]){
  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto N = args->getu64("-N",10,"how many times are tests executed");
  auto WORKGROUPS = args->getu64("-W",1024*1024,"how many workgroups to launch");
  auto ITERATIONS = args->getu64("-I",100,"how much work is performed by each thread");
  auto WGS        = args->getu64("-S",64,"size of a workgroup");
  bool printHelp  = args->isPresent("-h", "prints this help");
  auto platformId = args->getu32("-p",0,"platform id");
  auto deviceId   = args->getu32("-d",0,"device id");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  //get all platforms (drivers)
  std::vector<cl::Platform> all_platforms;
  cl::Platform::get(&all_platforms);
  if(all_platforms.size()==0){
      std::cout<<" No platforms found. Check OpenCL installation!\n";
      exit(1);
  }

  std::cout << "platforms: " << std::endl;
  for(auto const&p:all_platforms)
    std::cout << p.getInfo<CL_PLATFORM_NAME>() << std::endl;
  std::cout << std::endl;


  cl::Platform default_platform=all_platforms[platformId];
  std::cout << "Using platform: "<<default_platform.getInfo<CL_PLATFORM_NAME>()<<"\n";

  //get default device of the default platform
  std::vector<cl::Device> all_devices;
  default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
  if(all_devices.size()==0){
      std::cout<<" No devices found. Check OpenCL installation!\n";
      exit(1);
  }

  std::cout << "devices: " << std::endl;
  for(auto const&d:all_devices)
    std::cout << d.getInfo<CL_DEVICE_NAME>() << std::endl;
  std::cout << std::endl;


  cl::Device default_device=all_devices[deviceId];
  std::cout<< "Using device: "<<default_device.getInfo<CL_DEVICE_NAME>()<<"\n";


  cl::Context context({default_device});

  cl::Program::Sources sources;

  // kernel calculates for each element C=A+B
  std::string kernel_code=
  R".(
  void kernel compute(global uint* data){
    uint wid = get_global_id(1) * get_local_size(0) + get_global_id(0);
    uint N = data[wid];
    uint counter = 0;
    for(uint i=0;i<N&&i<10000;++i)
      counter += N*counter;
    if(counter == 1337)
      data[wid] = 100;
  }
  ).";

  sources.push_back({kernel_code.c_str(),kernel_code.length()});

  cl::Program program(context,sources);

  if(program.build({default_device})!=CL_SUCCESS){
      std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device)<<"\n";
      exit(1);
  }


  // create buffers on the device
  cl::Buffer buffer(context,CL_MEM_READ_WRITE,sizeof(uint32_t)*WORKGROUPS);

  //create queue to which we will push commands for the device.
  cl::CommandQueue queue(context,default_device);

  //alternative way to run the kernel
  cl::Kernel kernel_add=cl::Kernel(program,"compute");
  kernel_add.setArg(0,buffer);


  auto const measure = [&](size_t wgs,std::function<bool(size_t)>const&isActive,float full=0.f){

    std::vector<uint32_t>data(WORKGROUPS);

    size_t activeWG = 0;
    for(size_t i=0;i<WORKGROUPS;++i){
      if(isActive(i)){
        data[i] = ITERATIONS;
        activeWG++;
      }else
        data[i] = 0;
    }

    //write arrays A and B to the device
    queue.enqueueWriteBuffer(buffer,CL_TRUE,0,sizeof(decltype(data)::value_type)*data.size(),data.data());
    float active = (float)activeWG / (float)WORKGROUPS;

    std::string nname;
    for(size_t i=0;i<80;++i)
      if(isActive(i))
        nname += "*";
      else
        nname += ".";


    auto timer = Timer<float>();
    queue.finish();
    timer.reset();
    auto globalSize = cl::NDRange(WORKGROUPS*WGS/1024,1024,1);
    auto localSize = cl::NDRange(WGS,1,1);
    for(size_t i=0;i<N;++i)
      queue.enqueueNDRangeKernel(kernel_add,cl::NullRange,globalSize,localSize);
    queue.finish();
    auto time = timer.elapsedFromStart()/N;
    //uint32_t dd[10];
    //queue.enqueueReadBuffer(buffer,CL_TRUE,0,sizeof(uint32_t)*10,dd);
    //for(auto x:dd)
    //  std::cerr << x << " ";
    //std::cerr << std::endl;
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

  MEASURE(WGS,[](size_t){return true;},0.f);

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
