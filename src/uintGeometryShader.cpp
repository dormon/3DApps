#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <geGL/GLSLNoise.h>
#include <Barrier.h>
#include <imguiVars.h>
#include <addVarsLimits.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Noise: public simple3DApp::Application{
 public:
  Noise(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~Noise(){}
  virtual void draw() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void resize(uint32_t x,uint32_t y) override;
  vars::Vars vars;
  glm::ivec2 offset = glm::ivec2(0,0);

  virtual void                init() override;
};

void createDrawProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  out uint vNeco;
  void main(){
    vec2 coord = -1+2*vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(coord,0,1);
    vNeco = gl_VertexID;
  }
  ).";

  std::string const gsSrc = R".(

  layout(triangles)in;
  layout(triangle_strip,max_vertices=4)out;

  in uint vNeco[];
  //out uint gNeco;

  out vec4 gColor;

  void main(){
    for(int i=0;i<3;++i){ 
      if(vNeco[i]>0)gColor = vec4(1,0,0,0);
      else gColor = vec4(0);
      gl_Position = gl_in[i].gl_Position;
      EmitVertex();
    }
  }
  ).";

  std::string const fsSrc = R".(
  layout(location = 0)out vec4 fColor;
  in vec4 gColor;
  void main(){
    fColor = gColor;
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      vsSrc
      );
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,
      "#version 450\n",
      gsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("drawProgram",vs,gs,fs);
}

void Noise::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  vars.addUint32("M",8);
  vars.addUint32("N",8);
  vars.addFloat("p",2.f);
  vars.addBool("useSimplex");
  addVarsLimitsU(vars,"M",1,32,1);
  addVarsLimitsU(vars,"N",1,32,1);
  addVarsLimitsF(vars,"p",0.f,10.f,0.01f);

  createDrawProgram(vars);
}

void Noise::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  vars.get<ge::gl::Program>("drawProgram")
    ->use();
  

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();


  drawImguiVars(vars);
  ImGui::ShowMetricsWindow();

  swap();
}

void Noise::mouseMove(SDL_Event const& e) {
  auto const mState         = e.motion.state;
  if (mState & SDL_BUTTON_LMASK) {
    offset.x -= e.motion.xrel;
    offset.y += e.motion.yrel;
  }
}

void Noise::resize(uint32_t x,uint32_t y){
  ge::gl::glViewport(0,0,x,y);
}

int main(int argc,char*argv[]){
  Noise app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
