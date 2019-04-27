#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <geGL/GLSLNoise.h>
#include <Barrier.h>
#include <imguiVars.h>
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
  int32_t M=8;
  int32_t N=8;
  float   p=2.f;
  glm::ivec2 offset = glm::ivec2(0,0);

  virtual void                init() override;
};

void createDrawProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  void main(){
    vec2 coord = -1+2*vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(coord,0,1);
  }
  ).";
  std::string const fsSrc = R".(

  BEGINGRADIENT(terrain)
  vec4(0,0,.5,1),
  vec4(0,0,1,1),
  vec4(0,1,1,1),
  vec4(1,1,0,1),
  vec4(0,1,0,1),
  vec4(0,.5,0,1),
  vec4(0.3,.5,0.1,1),
  vec4(1,1,1,1),
  ENDGRADIENT
  #line 56
  out vec4 fColor;
  uniform uint M=8;
  uniform uint N=8;
  uniform float p=2.f;
  uniform ivec2 offset = ivec2(0);
  void main(){
    fColor = vec4(terrain(noise(ivec2(gl_FragCoord.xy+offset),M,N,p)*.5f+.5f));
    fColor = vec4(noise(ivec2(gl_FragCoord.xy+offset),M,N,p));
    uvec2 cc = uvec2(gl_FragCoord.xy + offset);
    //fColor = vec4(simplexNoise(cc+uvec2(1/sqrt(3)*length(cc/100)),M,N,p));
    //fColor = vec4(noise(uvec4(cc*10,0,0),M,N,p));
    //fColor = vec4(1-pow(abs(sin(100*noise(cc,M,N,p))),20));
    fColor = vec4(noise(cc,M,N,p));
    //fColor = vec4(noise2(cc*10,M,N));
    //fColor = vec4(noise3(cc*10));
    //if((M+N)!=100)fColor = vec4(smoothNoise(5,cc));
    //if(M!=100||N!=100)fColor = vec4(baseIntegerNoise(cc>>5));
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      ge::gl::getNoiseSource(),
      ge::gl::getGradientSource(),
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("drawProgram",vs,fs);
}

void Noise::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  createDrawProgram(vars);
}

void Noise::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  vars.get<ge::gl::Program>("drawProgram")
    ->set1ui("M",M)
    ->set1ui("N",N)
    ->set1f ("p",p)
    ->set2iv("offset",glm::value_ptr(offset))
    ->use();

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  ImGui::DragFloat("p",&p,0.01f,0.f,10.f);
  ImGui::DragInt("M",&M,1,1,32);
  ImGui::DragInt("N",&N,1,1,32);

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
