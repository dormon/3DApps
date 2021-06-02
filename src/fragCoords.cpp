#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <glm/glm.hpp>

class RasterizationOrder: public simple3DApp::Application{
 public:
  RasterizationOrder(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~RasterizationOrder(){}
  virtual void draw() override;

  vars::Vars vars;

  virtual void                init() override;
};

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize"}))return;

  std::string const vsSrc = R".(
  void main(){
    if((gl_VertexID%3) == 0)gl_Position = vec4(-1,-1,0,1);
    if((gl_VertexID%3) == 1)gl_Position = vec4(+1,-1,0,1);
    if((gl_VertexID%3) == 2)gl_Position = vec4(-1,+1,0,1);
  }
  ).";
  std::string const fsSrc = R".(
  layout(binding=0,std430)buffer Counter{uint counter[];};
  layout(binding=1,std430)buffer Coords{float coords[];};
  out vec4 fColor;
  void main(){
    uint fragmentId = atomicAdd(counter[0],1);
    coords[fragmentId*2+0]=gl_FragCoord.x;
    coords[fragmentId*2+1]=gl_FragCoord.y;
    fColor = vec4(1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("program",vs,fs);
}

void createBuffer(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  auto ws=*vars.get<glm::uvec2>("windowSize");
  vars.reCreate<ge::gl::Buffer>("counter",sizeof(uint32_t));
  vars.reCreate<ge::gl::Buffer>("coord"  ,sizeof(float)*2*ws.x*ws.y);
}

void RasterizationOrder::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addUint32("nofTriangles",2);
  vars.addUint32("maxFragment",1000);
  createBuffer(vars);
  createProgram(vars);
}

void RasterizationOrder::draw(){

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  vars.get<ge::gl::Buffer>("counter")->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  ge::gl::glMemoryBarrier(GL_ALL_BARRIER_BITS);
  vars.get<ge::gl::Buffer>("counter")->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  vars.get<ge::gl::Buffer>("coord"  )->bindBase(GL_SHADER_STORAGE_BUFFER,1);
  vars.get<ge::gl::Program>("program")
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLES,0,3);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  ge::gl::glMemoryBarrier(GL_ALL_BARRIER_BITS);

  std::vector<glm::vec2>coords;
  vars.get<ge::gl::Buffer>("coord")->getData(coords);

  std::vector<uint32_t>nofFrags;
  vars.get<ge::gl::Buffer>("counter")->getData(nofFrags);

  for(uint32_t i=0;i<nofFrags.at(0);++i){
    auto const&c=coords.at(i);
    std::cerr << c.x << " " << c.y << std::endl;
  }

  exit(1);

  swap();
}

int main(int argc,char*argv[]){
  RasterizationOrder app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
