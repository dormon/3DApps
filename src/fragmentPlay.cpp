#include <fstream>
#include <sstream>
#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <FreeImagePlus.h>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

class FragmentPlay: public simple3DApp::Application{
 public:
  FragmentPlay(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~FragmentPlay(){}
  virtual void draw() override;

  vars::Vars vars;
  bool fullscreen = false;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
  virtual void                key(SDL_Event const& e, bool down) override;
};

void loadColorTexture(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"updateTexture"}))return;
    if(!vars.getBool("updateTexture")) return;

  fipImage colorImg;
  colorImg.load(vars.getString("textureFile").c_str());
  auto const width   = colorImg.getWidth();
  auto const height  = colorImg.getHeight();
  auto const BPP     = colorImg.getBitsPerPixel();
  auto const imgType = colorImg.getImageType();
  auto const data    = colorImg.accessPixels();

  std::cerr << "color BPP : " << BPP << std::endl;
  std::cerr << "color type: " << imgType << std::endl;

  GLenum format;
  GLenum type;
  if(imgType == FIT_BITMAP){
    std::cerr << "color imgType: FIT_BITMAP" << std::endl;
    if(BPP == 24)format = GL_BGR;
    if(BPP == 32)format = GL_BGRA;
    type = GL_UNSIGNED_BYTE;
  }
  if(imgType == FIT_RGBAF){
    std::cerr << "color imgType: FIT_RGBAF" << std::endl;
    if(BPP == 32*4)format = GL_RGBA;
    if(BPP == 32*3)format = GL_RGB;
    type = GL_FLOAT;
  }
  if(imgType == FIT_RGBA16){
    std::cerr << "color imgType: FIT_RGBA16" << std::endl;
    if(BPP == 48)format = GL_RGB ;
    if(BPP == 64)format = GL_RGBA;
    type = GL_UNSIGNED_SHORT;
  }
 
  auto texture = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D,GL_RGB8,1,width,height);
  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1);
  ge::gl::glTextureSubImage2D(texture->getId(),0,0,0,width,height,format,type,data);
  auto map = vars.get<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures")->insert({vars.getUint32("textureBindingPoint"), texture});
  vars.reCreate<bool>("updateTexture", false);
}


void loadTextures(vars::Vars&vars){
  loadColorTexture(vars);
}

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"updateShader"}))return;
    if(!vars.getBool("updateShader")) return;

  std::string const vsSrc = R".(
  #version 450 core

  out vec2 texCoords;

  void main(){
    texCoords = vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(texCoords*2-1,0,1);
  }
  ).";

  std::ifstream file;
  file.open(vars.getString("shaderFile"));
  std::string fsSrc;
  
   if(file.fail()) 
      fsSrc = R".(
      #version 450 core
      layout(location=0)out vec4 fragColor; 
      void main()
      {
          fragColor = vec4(1.0f);
      }
      ).";
  else
  {
      std::stringstream stream;
      stream << file.rdbuf();
      fsSrc = stream.str();
  }

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("program",vs,fs);
  vars.reCreate<bool>("updateShader", false);
}


void drawFragmentPlay(vars::Vars&vars){
  loadTextures(vars);
   
    for( auto const& [point, texture] : *vars.get<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures"))
        texture->bind(point);
  
    vars.get<ge::gl::Program>("program")
    //->set1f ("baseline"   ,vars.getFloat("baseline"))
    ->use();

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void FragmentPlay::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  //auto const shaderFile = args->gets("--quilt","","quilt image 5x9");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }
  vars.add<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures");
  vars.addUint32("textureBindingPoint", 0);
  vars.addBool("updateTexture", false);
  vars.addBool("updateShader", true);
  vars.addString("shaderFile", std::string("none"));
  vars.addString("textureFile", std::string("none"));
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  std::cerr << SDL_GetNumVideoDisplays() << std::endl;
  
  createProgram(vars);
}

void FragmentPlay::key(SDL_Event const& event, bool DOWN) {
  if(event.key.keysym.sym == SDLK_f && DOWN){
    fullscreen = !fullscreen;
    if(fullscreen)
      window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
    else
      window->setFullscreen(sdl2cpp::Window::WINDOW);
  }
}

void FragmentPlay::draw(){
  createProgram(vars);

  ge::gl::glClear(GL_DEPTH_BUFFER_BIT);

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawFragmentPlay(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);
  ImGui::Begin("vars");           
  
  if(ImGui::Button("Reload shader"))
    vars.reCreate<bool>("updateShader", true);
   
  if(ImGui::Button("Add texture"))
    vars.reCreate<bool>("updateTexture", true);

  ImGui::End();

  swap();
}

void FragmentPlay::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  FragmentPlay app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
