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
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  flat out uint vTriangleID;
  void main(){
    if((gl_VertexID%3) == 0)gl_Position = vec4(-1,-1,0,1);
    if((gl_VertexID%3) == 1)gl_Position = vec4(+1,-1,0,1);
    if((gl_VertexID%3) == 2)gl_Position = vec4(+1,+1,0,1);
    vTriangleID = gl_VertexID/3;
  }
  ).";
  std::string const fsSrc = R".(
  layout(binding=0,std430)buffer Counter{uint counter[];};
  uniform uint maxFragment = 0;
  uniform uint nofTriangles = 2;
  flat in uint vTriangleID;
  out vec4 fColor;
  void main(){
    uint fragmentId = atomicAdd(counter[0],1);
    if(fragmentId >= maxFragment){
      discard;
      return;
    }
    float t = float(vTriangleID)/float(nofTriangles-1);
    fColor = vec4(t,1-t,0,1);
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

  vars.reCreate<ge::gl::Buffer>("counter",sizeof(uint32_t));
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
  vars.get<ge::gl::Program>("program")
    ->set1ui("maxFragment",vars.getUint32("maxFragment"))
    ->set1ui("nofTriangles",vars.getUint32("nofTriangles"))
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLES,0,vars.getUint32("nofTriangles")*3);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

int main(int argc,char*argv[]){
  RasterizationOrder app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
