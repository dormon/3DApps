#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <glm/glm.hpp>
#include <chrono>

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

void measure(Measurements&measurements,std::set<size_t>const&nofElements,std::set<size_t>const&elementSizes,std::set<Method>methods,size_t N){
    for(auto const&ne:nofElements)
      for(auto const&es:elementSizes){
        std::vector<uint8_t>data(ne*es);
        for(size_t e=0;e<ne;++e)
          for(size_t s=0;s<es;++s)
            data[e*es+s] = (static_cast<size_t>(ne-e-1)>>8*(es-s-1))&0xff;
        for(auto const&m:methods){
          for(size_t i=0;i<N;++i){
            auto toSort = data;
            auto time = m.fce(toSort,ne,es);
            measurements.add(Sample(m.name,ne,es,time));
          }
        }
      }
}

int main(int argc,char*argv[]){
  SortProject app{argc, argv};
  Measurements measurements;
  measure(
      measurements,
      {1000,1024,2000,2048,3000,4000,4096,8192,10000,20000},
      {1,2,4,8,16,32,64,128},
      {Method(cpu_qsort,"cpu_qsort")},
      10);
  auto mv = MeanVarianceTable(measurements);
  mv.print();

  return EXIT_SUCCESS;
}
