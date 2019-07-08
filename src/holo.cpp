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

class Holo: public simple3DApp::Application{
 public:
  Holo(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~Holo(){}
  virtual void draw() override;

  vars::Vars vars;
  bool fullscreen = false;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
  virtual void                key(SDL_Event const& e, bool down) override;
};

void loadColorTexture(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"quiltFileName"}))return;
  fipImage colorImg;
  colorImg.load(vars.getString("quiltFileName").c_str());
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

  auto colorTex = vars.reCreate<ge::gl::Texture>(
      "quilt",GL_TEXTURE_2D,GL_RGB8,1,width,height);
  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
  ge::gl::glTextureSubImage2D(colorTex->getId(),0,0,0,width,height,format,type,data);
}


void loadTextures(vars::Vars&vars){
  loadColorTexture(vars);
}

void createHoloProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450 core

  out vec2 texCoords;

  void main(){
    texCoords = vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(texCoords*2-1,0,1);
  }
  ).";

  std::string const fsSrc = R".(
  #version 450 core
  
  in vec2 texCoords;
  
  layout(location=0)out vec4 fragColor;
  
  // HoloPlay values
  uniform float pitch;
  uniform float tilt;
  uniform float center;
  uniform float invView;
  uniform float flipX;
  uniform float flipY;
  uniform float subp;
  uniform int ri;
  uniform int bi;
  uniform vec4 tile;
  uniform vec4 viewPortion;
  uniform vec4 aspect;
  
  layout(binding=0)uniform sampler2D screenTex;
  
  vec2 texArr(vec3 uvz)
  {
      // decide which section to take from based on the z.
      float z = floor(uvz.z * tile.z);
      float x = (mod(z, tile.x) + uvz.x) / tile.x;
      float y = (floor(z / tile.x) + uvz.y) / tile.y;
      return vec2(x, y) * viewPortion.xy;
  }
  
  void main()
  {
  	vec3 nuv = vec3(texCoords.xy, 0.0);
  
  	vec4 rgb[3];
  	for (int i=0; i < 3; i++) 
  	{
  		nuv.z = (texCoords.x + i * subp + texCoords.y * tilt) * pitch - center;
  		nuv.z = mod(nuv.z + ceil(abs(nuv.z)), 1.0);
  		nuv.z = (1.0 - invView) * nuv.z + invView * (1.0 - nuv.z);
  		rgb[i] = texture(screenTex, texArr(nuv));
  		//rgb[i] = vec4(nuv.z, nuv.z, nuv.z, 1.0);
  	}
  
      fragColor = vec4(rgb[ri].r, rgb[1].g, rgb[bi].b, 1.0);
      //fragColor = texture(screenTex, texCoords.xy);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("holoProgram",vs,fs);
}


void drawHolo(vars::Vars&vars){
  loadTextures(vars);
  createHoloProgram(vars);


  auto colorTex     = vars.get<ge::gl::Texture>("quilt");
  colorTex->bind(0);
  vars.get<ge::gl::Program>("holoProgram")
    ->set1f ("pitch"      ,vars.getFloat("pitch"))
    ->set1f ("tilt"       ,vars.getFloat("tilt"))
    ->set1f ("center"     ,vars.getFloat("center"))
    ->set1f ("invView"    ,vars.getFloat("invView"))
    ->set1f ("subp"       ,vars.getFloat("subp"))
    ->set1i ("ri"         ,vars.getInt32("ri"))
    ->set1i ("bi"         ,vars.getInt32("bi"))
    ->set4fv("tile"       ,glm::value_ptr(*vars.get<glm::vec4>("tile")))
    ->set4fv("viewPortion",glm::value_ptr(*vars.get<glm::vec4>("viewPortion")))
    ->use();

  auto tile = *vars.get<glm::vec4>("viewPortion");
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void Holo::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  auto const quiltFile = args->gets("--quilt","","quilt image 5x9");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  vars.addString("quiltFileName",quiltFile);

  
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  vars.addFloat("pitch",354.42108f);
  vars.addFloat("tilt",-0.1153f);
  vars.addFloat("center",0.04239f);
  vars.addFloat("invView",1.00f);
  vars.addFloat("subp",0.00013f);
  vars.addInt32("ri",0);
  vars.addInt32("bi",2);
  vars.add<glm::vec4>("tile",5.00f, 9.00f, 45.00f, 45.00f);
  vars.add<glm::vec4>("viewPortion",0.99976f, 0.99976f, 0.00f, 0.00f);

  std::cerr << SDL_GetNumVideoDisplays() << std::endl;
}

void Holo::key(SDL_Event const& event, bool DOWN) {
  if(event.key.keysym.sym == SDLK_f && DOWN){
    fullscreen = !fullscreen;
    if(fullscreen)
      window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
    else
      window->setFullscreen(sdl2cpp::Window::WINDOW);
  }
}


void Holo::draw(){


  ge::gl::glClear(GL_DEPTH_BUFFER_BIT);

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawHolo(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void Holo::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  Holo app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
