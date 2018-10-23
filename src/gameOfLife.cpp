#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <DrawGrid.h>
#include <FreeImagePlus.h>
#include<assimp/cimport.h>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

class GameOfLife: public simple3DApp::Application{
 public:
  GameOfLife(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~GameOfLife(){}
  virtual void draw() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  vars::Vars vars;

  virtual void                init() override;
};

void createLifeProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const mpSrc = R".(
  layout(local_size_x=8,local_size_y=8)in;

  layout(r32ui,binding=0)uniform uimage2D inTex;
  layout(r32ui,binding=1)uniform uimage2D outTex;

  uint nofAlive(ivec2 coord){
    uint result = 0;
    for(int y=-1;y<2;++y)
      for(int x=-1;x<2;++x){
        if(y == 0 && x == 0)continue;
          result += imageLoad(inTex,coord+ivec2(x,y)).x;
      }
    return result;
  }

  void main(){
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    uint n = nofAlive(coord);
    if(n > 3 && n < 5)
      imageStore(outTex,coord,ivec4(1));
    if(n > 5)
      imageStore(outTex,coord,ivec4(0));
  }
  ).";

  auto mp = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,
      "#version 450\n",
      mpSrc
      );
  vars.reCreate<ge::gl::Program>("lifeProgram",mp);
}

void createDrawProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  void main(){
    vec2 coord = -1+2*vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(coord,0,1);
  }
  ).";
  std::string const fsSrc = R".(
  in vec2 vCoord;
  out vec4 fColor;
  layout(r32ui,binding=0)uniform uimage2D texture;
  void main(){
    fColor = vec4(imageLoad(texture,ivec2(gl_FragCoord.xy)).x);
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
  vars.reCreate<ge::gl::Program>("drawProgram",vs,fs);
}

void createImages(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize"}))return;

  auto windowSize = vars.get<glm::uvec2>("windowSize");

  vars.reCreate<ge::gl::Texture>("image0",GL_TEXTURE_2D,GL_R32UI,1,windowSize->x,windowSize->y);
  vars.reCreate<ge::gl::Texture>("image1",GL_TEXTURE_2D,GL_R32UI,1,windowSize->x,windowSize->y);
}

void GameOfLife::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addUint32("frameCounter",0);
  vars.addBool("simulate",false);
  vars.addBool("clear",false);
  vars.addBool("initGrid",false);

  createImages(vars);
  createDrawProgram(vars);
  createLifeProgram(vars);
}

void GameOfLife::draw(){
  createLifeProgram(vars);
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  ge::gl::glMemoryBarrier(GL_ALL_BARRIER_BITS);

  if(vars.getBool("simulate")){
    auto windowSize = vars.get<glm::uvec2>("windowSize");

    vars.get<ge::gl::Texture>("image0")->bindImage(vars.getUint32("frameCounter")%2 == 0);
    vars.get<ge::gl::Texture>("image1")->bindImage(vars.getUint32("frameCounter")%2 != 0);

    vars.get<ge::gl::Program>("lifeProgram")->use();
    vars.get<ge::gl::Program>("lifeProgram")
      ->dispatch(windowSize->x / 8,windowSize->y /8,1);
    
    ge::gl::glMemoryBarrier(GL_ALL_BARRIER_BITS);
  }

  if(vars.getBool("initGrid")){

  }

  if(vars.getBool("clear")){
    vars.get<ge::gl::Texture>("image0")->clear(0,GL_RED_INTEGER,GL_UNSIGNED_INT);
    vars.get<ge::gl::Texture>("image1")->clear(0,GL_RED_INTEGER,GL_UNSIGNED_INT);
  }

  if(vars.getUint32("frameCounter")%2 == 0)
    vars.get<ge::gl::Texture>("image0")->bindImage(0);
  else
    vars.get<ge::gl::Texture>("image1")->bindImage(0);

  vars.get<ge::gl::Program>("drawProgram")
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();

  vars.getUint32("frameCounter")+=1;
  vars.updateTicks("frameCounter");
}

void GameOfLife::mouseMove(SDL_Event const& e) {
  auto const mState         = e.motion.state;
  if (mState & SDL_BUTTON_LMASK) {

    auto windowSize = vars.get<glm::uvec2>("windowSize");
    uint32_t data = 1;
    vars.get<ge::gl::Texture>("image0")->setData2D(
        &data,GL_RED_INTEGER,GL_UNSIGNED_INT,0,GL_TEXTURE_2D,e.motion.x,windowSize->y-e.motion.y-1,1,1);

    //vars.get<ge::gl::Texture>("image1")->setData2D(
    //    &data,GL_RED_INTEGER,GL_UNSIGNED_INT,0,GL_TEXTURE_2D,e.motion.x,windowSize->y-e.motion.y-1,1,1);
  }
}

int main(int argc,char*argv[]){
  GameOfLife app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
