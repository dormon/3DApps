#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <glm/glm.hpp>
#include <chrono>
#include <StringAligner/StringAligner.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <sstream>

class SortProject: public simple3DApp::Application{
 public:
  SortProject(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~SortProject(){}
};

//nofElements elementSize

class Sample{
  public:
    Sample(std::string const&n,size_t nofE,size_t eS,float t):name(n),nofElements(nofE),elementSize(eS),time(t){}
    std::string name       ;
    size_t      nofElements;
    size_t      elementSize;
    float       time       ;
};

class Measurements{
  public:
    void add(Sample const&s){
      samples[s.name][s.nofElements][s.elementSize].push_back(s.time);
      nofElements.insert(s.nofElements);
      elementSizes.insert(s.elementSize);
      names.insert(s.name);
    }
    void print(){
      for(auto const&s:samples){
        std::cout << s.first << std::endl;

      }
    }
    std::set<size_t     >nofElements ;
    std::set<size_t     >elementSizes;
    std::set<std::string>names       ;
    std::map<std::string,std::map<size_t,std::map<size_t,std::vector<float>>>>samples;
};

class MeanVariance{
  public:
    MeanVariance(float m = 0.f,float v = 0.f):mean(m),variance(v){}
    float mean;
    float variance;
};

class StrTable{
  public:
  protected:
    std::vector<std::vector<std::string>>table;
};

class MeanVarianceTable{
  public:
    MeanVarianceTable(Measurements const&m):nofElements(m.nofElements),elementSizes(m.elementSizes),names(m.names){
      for(auto const&i:m.samples)
        for(auto const&j:i.second)
          for(auto const&k:j.second){
            float mean = 0.f;
            for(auto const&v:k.second)
              mean += v;
            mean /= k.second.size();
            float variance = 0.f;
            for(auto const&v:k.second)
              variance += (mean-v)*(mean-v);
            variance /= k.second.size();
            samples[i.first][j.first][k.first] = MeanVariance(mean,variance);
          }
    }
    void print(){
      for(auto const&n:samples){
        auto table = std::make_shared<stringAligner::Table>();
        for(size_t i=0;i<=nofElements.size();++i)
          table->addRow();
        for(size_t i=0;i<=elementSizes.size();++i)
          table->addColumn();

        table->setCell(0,0,std::make_shared<stringAligner::Text>(n.first,10));


        size_t column=1;
        for(auto const&es:elementSizes){
          std::stringstream ss;
          ss << es;
          table->setCell(0,column++,std::make_shared<stringAligner::Text>(ss.str(),14));
        }
        size_t row=1;
        for(auto const&es:nofElements){
          std::stringstream ss;
          ss << es;
          table->setCell(row++,0,std::make_shared<stringAligner::Text>(ss.str(),14));
        }


        row = 1;
        for(auto const&ne:n.second){
          column = 1;
          for(auto const&es:ne.second){
            std::stringstream ss;
            ss << es.second.mean;
            table->setCell(row,column,std::make_shared<stringAligner::Text>(ss.str(),14));
            column++;
          }
          row++;
        }

        std::cout << table->getData() << std::endl;
      }
      return;
      for(auto const&n:samples){
        std::cerr << n.first << std::endl;
        bool first = true;
        for(auto const&j:n.second){
          if(first){
            std::cerr << "    ";
            for(auto const&j:n.second)
              std::cerr << j.first << " ";
            std::cerr << std::endl;
            first = false;
          }
          for(auto const&k:j.second){
            std::cerr << k.second.mean << " ";
          }
          std::cerr << std::endl;
        }
      }
    }
  protected:
    std::set<size_t     >nofElements ;
    std::set<size_t     >elementSizes;
    std::set<std::string>names       ;
    std::map<std::string,std::map<size_t,std::map<size_t,MeanVariance>>>samples;
};

using SORT_FUNC = float(*)(std::vector<uint8_t>&,size_t,size_t);

class Method{
  public:
    Method(SORT_FUNC const&f,std::string n):fce(f),name(n){}
    SORT_FUNC fce;
    std::string name;
    bool operator<(Method const&a)const{
      return name < a.name;
    }
};

size_t qsort_element_size;
int qsort_comp(void const*a,void const*b){
  auto aa = reinterpret_cast<uint8_t const*>(a);
  auto bb = reinterpret_cast<uint8_t const*>(b);
  for(size_t i=0;i<qsort_element_size;++i){
    if(aa[i] < bb[i])return -1;
    if(aa[i] > bb[i])return +1;
  }
  return 0;
}

float cpu_qsort(std::vector<uint8_t>&d,size_t nofElements,size_t elementSize){
  qsort_element_size = elementSize;
  auto start = std::chrono::high_resolution_clock::now();
  qsort(d.data(),nofElements,elementSize,qsort_comp);
  auto end   = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsed = end - start;
  return elapsed.count();
}

float cpu_bubble(std::vector<uint8_t>&d,size_t nofElements,size_t elementSize){
  auto const cmp = [&](uint8_t*a,uint8_t*b){
    for(size_t i=0;i<elementSize;++i){
      if(a[i] < b[i])return -1;
      if(a[i] > b[i])return +1;
    }
    return 0;
  };
  auto const swap = [&](uint8_t*a,uint8_t*b){
    for(size_t i=0;i<elementSize;++i){
      auto c = a[i];
      a[i] = b[i];
      b[i] = c;
    }
  };
  auto const ifGreaterSwap = [&](uint8_t*a,uint8_t*b){
    if(cmp(a,b)>0)swap(a,b);
  };
  auto const run = [&](uint8_t*ptr,size_t offset){
    for(size_t i=offset;i+1<nofElements;i+=2)
      ifGreaterSwap(ptr+i*elementSize,ptr+(i+1)*elementSize);
  };
  auto const oneRun = [&](uint8_t*ptr){
    run(ptr,0);
    run(ptr,1);
  };


  auto start = std::chrono::high_resolution_clock::now();
  for(size_t i=0;i<nofElements-1;++i)
    oneRun(d.data());
  auto end   = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsed = end - start;
  return elapsed.count();
}

float gpu_merge(std::vector<uint8_t>&d,size_t nofElements,size_t elementSize){
  if(elementSize != 4)return std::numeric_limits<float>::infinity();
  auto buffer = std::make_shared<ge::gl::Buffer>(d);
  std::string const src = R".(
  #version 450
  
  layout(local_size_x=WORKGROUP_SIZE)in;

  #define UINT_SIZE             4
  #define ELEMENT_SIZE_IN_UINTS (ELEMENT_SIZE/UINT_SIZE)
  #define LOCAL_SIZE_IN_UINTS   (LOCAL_ELEMENTS*ELEMENT_SIZE_IN_UINTS)
  #define UINT_LOADS_PER_THREAD (LOCAL_SIZE_IN_UINTS / WORKGROUP_SIZE)
  #define ELEMENTS_PER_THREAD   (LOCAL_ELEMENTS / WORKGROUP_SIZE)

  shared uint local[LOCAL_SIZE_IN_UINTS];

  layout(binding=0,std430)buffer Data{uint data[];};

  uint loadUint(uint i){
    if(i>=NOF_ELEMENTS)return 0xffffffff;
    return data[i];
  }

  void storeUint(uint i,uint d){
    if(i>=NOF_ELEMENTS)return;
    data[i] = d;
  }

  #define GLOBAL_INDEX(i) (gl_LocalInvocationID.x + i*gl_WorkGroupID.x + gl_WorkGroupID.x*gl_WorkGroupSize.x*UINT_LOADS_PER_THREAD)
  #define LOCAL_INDEX(i)  (gl_LocalInvocationID.x + i*WORKGROUP_SIZE)

  void loadToLocal(){
    for(uint i=0;i<UINT_LOADS_PER_THREAD;++i)
      local[LOCAL_INDEX(i)] = loadUint(GLOBAL_INDEX(i));
    barrier();
  }

  void storeToGlobal(){
    for(uint i=0;i<UINT_LOADS_PER_THREAD;++i)
      storeUint(GLOBAL_INDEX(i),local[LOCAL_INDEX(i)]);
  }

  bool less(uint A,uint B){
    if(A >= LOCAL_ELEMENTS || B >= LOCAL_ELEMENTS)return true;
    for(uint i=0;i<ELEMENT_SIZE_IN_UINTS;++i){
      if(local[A*ELEMENT_SIZE_IN_UINTS+i] > local[B*ELEMENT_SIZE_IN_UINTS+i])return false;
      if(local[A*ELEMENT_SIZE_IN_UINTS+i] < local[B*ELEMENT_SIZE_IN_UINTS+i])return true ;
    }
    return false;
  }

  void swap(uint A,uint B){
    if(A >= LOCAL_ELEMENTS || B >= LOCAL_ELEMENTS)return;
    for(uint i=0;i<ELEMENT_SIZE_IN_UINTS;++i){
      uint a = local[A*ELEMENT_SIZE_IN_UINTS+i];
      local[A*ELEMENT_SIZE_IN_UINTS+i] = local[B*ELEMENT_SIZE_IN_UINTS+i];
      local[B*ELEMENT_SIZE_IN_UINTS+i] = a;
    }
  }

  void bubbleSortEvenRun(){
    for(uint i=0;i<ELEMENTS_PER_THREAD;++i)
      if(!less(gl_LocalInvocationID.x*2+i*gl_WorkGroupSize.x,gl_LocalInvocationID.x*2+1+i*gl_WorkGroupSize.x)){
        swap(gl_LocalInvocationID.x*2+i*gl_WorkGroupSize.x,gl_LocalInvocationID.x*2+1+i*gl_WorkGroupSize.x);
      }
  }
  void bubbleSortOddRun(){
    for(uint i=0;i<ELEMENTS_PER_THREAD;++i)
      if(!less(gl_LocalInvocationID.x*2+1+i*gl_WorkGroupSize.x,gl_LocalInvocationID.x*2+2+i*gl_WorkGroupSize.x)){
        swap(gl_LocalInvocationID.x*2+1+i*gl_WorkGroupSize.x,gl_LocalInvocationID.x*2+2+i*gl_WorkGroupSize.x);
      }
  }

  void bubbleOneRun(){
    bubbleSortEvenRun();
    barrier();
    bubbleSortOddRun();
    barrier();
  }



  void merge(){
  }

  void bubbleSort(){
    for(uint i=0;i<LOCAL_ELEMENTS;++i)
      bubbleOneRun();
  }

  void main(){
    loadToLocal();

    bubbleSort();

    storeToGlobal();
  }

  ).";
  auto prg = std::make_shared<ge::gl::Program>(
      std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src)
      );

  ge::gl::glFinish();
  auto start = std::chrono::high_resolution_clock::now();
  //TODO
  ge::gl::glFinish();
  auto end   = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsed = end - start;
  return elapsed.count();
}

float gpu_bubble(std::vector<uint8_t>&d,size_t nofElements,size_t elementSize){
  auto buffer = std::make_shared<ge::gl::Buffer>(d);
  std::string const src = R".(
  #version 450
  
  #ifndef ELEMENT_SIZE
  #define ELEMENT_SIZE 4
  #endif

  #ifndef NOF_ELEMENTS
  #define NOF_ELEMENTS 0
  #endif

  layout(binding=0,std430)buffer Data{uint data[];}

  void evenRun1byte(){
    uint d = data[gl_GlobalInvocationID.x];
    d = (((d>>0)&0x000000ff)>((d>>8)&0x000000ff))*( (d&0xffff0000) + ((d<<8)&0x0000ff00) + ((d>>8)&0x000000ff) ) + (((d>>0)&0x000000ff)<=((d>>8)&0x000000ff))*d;
    d = (((d>>0)&0x00ff0000)>((d>>8)&0x00ff0000))*( (d&0x0000ffff) + ((d<<8)&0xff000000) + ((d>>8)&0x00ff0000) ) + (((d>>0)&0x00ff0000)<=((d>>8)&0x00ff0000))*d;

    data[gl_GlobalInvocationID.x] = d;
  }

  void sortSmall(){
  }

  void main(){
    if(ELEMENT_SIZE < 4){
      sortSmall();
    }else{
    }
  }

  ).";
  auto prg = std::make_shared<ge::gl::Program>(
      std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,src)
      );

  ge::gl::glFinish();
  auto start = std::chrono::high_resolution_clock::now();
  //TODO
  ge::gl::glFinish();
  auto end   = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsed = end - start;
  return elapsed.count();
}

void measure(Measurements&measurements,std::set<size_t>const&nofElements,std::set<size_t>const&elementSizes,std::set<Method>methods,size_t N){
    for(auto const&ne:nofElements)
      for(auto const&es:elementSizes){
        std::vector<uint8_t>data(ne*es);
        for(size_t e=0;e<ne;++e)
          for(size_t s=0;s<es;++s)
            data[e*es+s] = (static_cast<size_t>(ne-e-1)>>8*(es-s-1))&0xff;
        for(auto const&m:methods){
          std::cerr << "nofElements: " << ne << ", elementSIze: " << es << "method: " << m.name << std::endl;
          for(size_t i=0;i<N;++i){
            auto toSort = data;
            auto time = m.fce(toSort,ne,es);
            measurements.add(Sample(m.name,ne,es,time));
          }
        }
      }
}

template<typename OUT,typename IN>
std::set<OUT>convert(std::vector<IN>const&d){
  std::set<OUT>result;
  for(auto const&x:d)
    result.insert(x);
  return result;
}

int main(int argc,char*argv[]){
  SortProject app{argc, argv};

  std::map<std::string,SORT_FUNC> const functions = {
    {"cpu_qsort" ,cpu_qsort },
    {"cpu_bubble",cpu_bubble},
  };

  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto const nofElements  = args->getu64v  ("--nofElements" ,{1000,1024,2000,2048,3000,4000,4096,8192,10000,20000},"these number of elemets will be tested");
  auto const elementSizes = args->getu64v  ("--elementSizes",{1,2,4,8,16,32,64,128}                               ,"these element sizes will be tested"    );
  auto const methods      = args->getsv    ("--methods"     ,{"cpu_qsort"}                                        ,"these methods will be tested"          );
  auto const samples      = args->getu64   ("--samples"     ,10                                                   ,"number of measurements per one setting");
  auto const printMethods = args->isPresent("--printMethods"                                                      ,"print sorting methods names"           );
  auto const printHelp    = args->isPresent("-h"                                                                  ,"print this help"                       );

  if(printHelp || !args->validate()){
    std::cerr << args->toStr();
    return 0;
  }

  if(printMethods){
    for(auto const&f:functions)
      std::cout << f.first << std::endl;
    return 0;
  }


  auto const nofElementsSet  = convert<size_t>(nofElements );
  auto const elementSizesSet = convert<size_t>(elementSizes);

  std::set<Method>methodsSet;
  for(auto const&m:methods)
    methodsSet.insert(Method(functions.at(m),m));


  Measurements measurements;
  measure(
      measurements,
      nofElementsSet,
      elementSizesSet,
      methodsSet,
      samples);
  auto mv = MeanVarianceTable(measurements);
  mv.print();

  return EXIT_SUCCESS;
}
