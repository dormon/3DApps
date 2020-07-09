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
  out vec3 vColor;
  uniform float z = 0.f;
  void main(){
    if(gl_VertexID<3)vColor = vec3(1,0,0);
    if(gl_VertexID == 0)gl_Position =vec4(-1,-1,0,1);
    if(gl_VertexID == 1)gl_Position =vec4(+1,-1,0,1);
    if(gl_VertexID == 2)gl_Position =vec4(+1,+1,z,1);
    if(gl_VertexID>2)vColor = vec3(0,1,0);
    if(gl_VertexID == 3)gl_Position =vec4(+1,+1,z,1);
    if(gl_VertexID == 4)gl_Position =vec4(+1,-1,0,1);
    if(gl_VertexID == 5)gl_Position =vec4(-1,-1,0,1);

  }
  ).";
  std::string const fsSrc = R".(
  in vec3 vColor;
  out vec4 fColor;
  void main(){
    fColor = vec4(vColor,1);
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
  auto prg = vars.reCreate<ge::gl::Program>("program",vs,fs);
  prg->setNonexistingUniformWarning(false);
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
  vars.addFloat("z",0.f);
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
    ->set1f ("z"           ,vars.getFloat ("z"           ))
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
