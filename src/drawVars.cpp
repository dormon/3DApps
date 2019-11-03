#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <imguiVars/addVarsLimits.h>
#include <glm/glm.hpp>

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv,330) {}
  virtual ~EmptyProject(){}
  virtual void draw() override;

  vars::Vars vars;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};

enum TestEnum{
  E_VALUE_A = 32 ,
  E_VALUE_B = 7  ,
  E_VALUE_C = 123,
};

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  vars.addFloat("limitedFloat");
  addVarsLimitsF(vars,"limitedFloat",1,2,.1f);
  vars.addFloat("hidden");
  hide(vars,"hidden");

  vars.addEnum<TestEnum>("enum");
  addEnumValues<TestEnum>(vars,{E_VALUE_A,E_VALUE_B,E_VALUE_C},{"E_VALUE_A","E_VALUE_B","E_VALUE_C"});
}

void EmptyProject::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void EmptyProject::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  EmptyProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
