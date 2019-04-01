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

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~EmptyProject(){}
  virtual void draw() override;

  vars::Vars vars;

  virtual void                init() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  layout(binding=0)uniform usampler2D canvas;
  uniform ivec2 offset = ivec2(0);
  uniform float scale = 1.f;
  uniform ivec2 screenSize = ivec2(512,512);
  out vec2 vCoord;
  void main(){
    ivec2 isize = textureSize(canvas,0);
    vec2 size = vec2(isize);
    float aspect = size.x/size.y;
    vCoord = vec2(size*vec2(gl_VertexID&1,gl_VertexID>>1));

    vec2 pos = vec2(-1+2*(gl_VertexID&1),-1+2*(gl_VertexID>>1));
    //pos *= vec2(1,aspect);
    pos *= size;
    pos /= scale;
    pos -= vec2(offset)*2;
    pos /= vec2(screenSize);
    
    if(scale == 1337.f){gl_Position = vec4(0);return;}

    gl_Position = vec4(pos,1,1);
  }
  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  layout(binding=0)uniform usampler2D canvas;
  in vec2 vCoord;
  void main(){
    uint data = texelFetch(canvas,ivec2(vCoord),0).x;
    fColor = vec4(data>0);
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

void createCanvas(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"canvasSize"}))return;

  glm::uvec2 const size = *vars.get<glm::uvec2>("canvasSize");

  auto canvas = vars.reCreate<ge::gl::Texture>("canvas",GL_TEXTURE_2D,GL_R8UI,1,size.x,size.y);
  canvas->texParameteri(GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  canvas->texParameteri(GL_TEXTURE_MIN_FILTER,GL_NEAREST);
}

void lineBres(std::vector<uint8_t>&data,glm::uvec2 const& size,glm::uvec2 const& A,glm::uvec2 const&B){
  glm::ivec2 d = B-A;
  int P = 2*d.y-d.x;
  int P1 = 2*d.y;
  int P2 = P1-2*d.x;
  int y=A.y;
  for(int x=A.x;x<=B.x;++x){
    data.at(y*size.x+x) = 255;
    if(P>=0){
      P+=P2;
      y++;
    }else{
      P+=P1;
    }
  }
}

void drawToCanvas(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"canvas","canvasSize"}))return;

  glm::uvec2 const size = *vars.get<glm::uvec2>("canvasSize");
  auto canvas = vars.get<ge::gl::Texture>("canvas");

  std::vector<uint8_t>data(size.x*size.y);

  lineBres(data,size,glm::uvec2(100,100),glm::uvec2(200,150));
  lineBres(data,size,glm::uvec2(100,100),glm::uvec2(150,200));
  

  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,size.x);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
  ge::gl::glTextureSubImage2D(canvas->getId(),0,0,0,size.x,size.y,GL_RED_INTEGER,GL_UNSIGNED_BYTE,data.data());

}

void EmptyProject::mouseMove(SDL_Event const& e) {
  auto const mState = e.motion.state;
  if (mState & SDL_BUTTON_LMASK) {
    auto offset = vars.get<glm::ivec2>("offset");
    offset->x -= e.motion.xrel;
    offset->y += e.motion.yrel;
  }
  if (mState & SDL_BUTTON_RMASK) {
    auto&scale = vars.getFloat("scale");
    scale += e.motion.yrel*0.01;
    if(scale<0.1)scale=0.1;
    if(scale>10 )scale=10;
  }

}

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.add<glm::uvec2>("canvasSize",512,512);
  vars.add<glm::ivec2>("offset",glm::ivec2(0));
  vars.addFloat("scale",1.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
}

void EmptyProject::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  createCanvas(vars);
  drawToCanvas(vars);
  createProgram(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  auto windowSize = vars.get<glm::uvec2>("windowSize");
  auto canvas = vars.get<ge::gl::Texture>("canvas");
  auto offset = vars.get<glm::ivec2>("offset");
  auto scale = vars.getFloat("scale");
  canvas->bind(0);
  vars.get<ge::gl::Program>("program")
    ->set2i("offset",offset->x,offset->y)
    ->set1f("scale",scale)
    ->set2i("screenSize",windowSize->x,windowSize->y)
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void EmptyProject::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
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
