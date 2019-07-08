#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <DrawGrid.h>
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
  virtual void                mouseMove(SDL_Event const& event) override;
};

void createView(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"useOrbitCamera"}))return;

  if(vars.getBool("useOrbitCamera"))
    vars.reCreate<basicCamera::OrbitCamera>("view");
  else
    vars.reCreate<basicCamera::FreeLookCamera>("view");
}

void createProjection(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize","camera.fovy","camera.near","camera.far"}))return;

  auto windowSize = vars.get<glm::uvec2>("windowSize");
  auto width = windowSize->x;
  auto height = windowSize->y;
  auto aspect = (float)width/(float)height;
  auto nearv = vars.getFloat("camera.near");
  auto farv  = vars.getFloat("camera.far" );
  auto fovy = vars.getFloat("camera.fovy");

  vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,nearv,farv);
}

void createCamera(vars::Vars&vars){
  createProjection(vars);
  createView(vars);
}


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
      "quiltTex",GL_TEXTURE_2D,GL_RGB8,1,width,height);
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
  
  uniform int showQuilt = 0;
  uniform int showAsSequence = 0;
  uniform uint selectedView = 0;
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
  
      if(showQuilt == 0)
        fragColor = vec4(rgb[ri].r, rgb[1].g, rgb[bi].b, 1.0);
      else{
        if(showAsSequence == 0)
          fragColor = texture(screenTex, texCoords.xy);
        else{
          uint sel = min(selectedView,uint(tile.x*tile.y-1));
          fragColor = texture(screenTex, texCoords.xy/vec2(tile.xy) + vec2(vec2(1.f)/tile.xy)*vec2(sel%uint(tile.x),sel/uint(tile.x)));
          
        }
      }
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      fsSrc
      );
  auto prg = vars.reCreate<ge::gl::Program>("holoProgram",vs,fs);
}

class Quilt{
  public:
    glm::uvec2 counts = glm::uvec2(5,9);
    glm::uvec2 res = glm::uvec2(512,256);
    std::shared_ptr<ge::gl::Framebuffer>fbo;
    std::shared_ptr<ge::gl::Texture>color;
    std::shared_ptr<ge::gl::Texture>depth;
    vars::Vars&vars;
    Quilt(vars::Vars&vars):vars(vars){
      fbo = std::make_shared<ge::gl::Framebuffer>();
      color = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D,GL_RGB8,1,res.x*counts.x,res.y*counts.y);
      depth = std::make_shared<ge::gl::Texture>(GL_TEXTURE_RECTANGLE,GL_DEPTH24_STENCIL8,1,res.x*counts.x,res.y*counts.y);
      fbo->attachTexture(GL_COLOR_ATTACHMENT0,color);
      fbo->attachTexture(GL_DEPTH_ATTACHMENT,depth);
      GLenum buffers[] = {GL_COLOR_ATTACHMENT0};
      fbo->drawBuffers(1,buffers);
      std::cerr << "fbo: "<<int(fbo->check()) << std::endl;
    }
    void draw(std::function<void(glm::mat4 const&view,glm::mat4 const&proj)>const&fce,glm::mat4 const&centerView,glm::mat4 const&centerProj){
      GLint origViewport[4];
      ge::gl::glGetIntegerv(GL_VIEWPORT,origViewport);

      fbo->bind();
      ge::gl::glClearColor(1,0,0,1);
      ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      size_t counter = 0;
      glm::mat4 view = centerView;
      glm::mat4 proj = centerProj;

      for(size_t j=0;j<counts.y;++j)
        for(size_t i=0;i<counts.x;++i){
          ge::gl::glViewport(i*(res.x),j*(res.y),res.x,res.y);


          float const fov = glm::radians<float>(vars.getFloat("quiltRender.fov"));
          float const size = vars.getFloat("quiltRender.size");
          float const camDist  =  size / glm::tan(fov * 0.5f); /// ?
          float const viewCone = glm::radians<float>(vars.getFloat("quiltRender.viewCone")); /// ?
          float const aspect = static_cast<float>(res.x) / static_cast<float>(res.y);
          float const viewConeSweep = -camDist * glm::tan(viewCone);
			    float const projModifier = 1.f / (size * aspect);
          auto const numViews = counts.x * counts.y;
          float currentViewLerp = 0.f; // if numviews is 1, take center view
          if (numViews > 1)
            currentViewLerp = (float)counter / (numViews - 1) - 0.5f;

          //std::cerr <<currentViewLerp * viewConeSweep << std::endl;
          view[3][0] += currentViewLerp * viewConeSweep;
          proj[2][0] += currentViewLerp * viewConeSweep * projModifier;

          fce(view,proj);
          counter++;
        }
      fbo->unbind();

      ge::gl::glViewport(origViewport[0],origViewport[1],origViewport[2],origViewport[3]);
    }
};

void drawHolo(vars::Vars&vars){
  loadTextures(vars);
  createHoloProgram(vars);



  if(vars.getBool("renderQuilt")){
    vars.get<Quilt>("quilt")->color->bind(0);
  }else{
    vars.get<ge::gl::Texture>("quiltTex")->bind(0);
  }
  vars.get<ge::gl::Program>("holoProgram")
    ->set1i ("showQuilt"     ,                vars.getBool       ("showQuilt"            ))
    ->set1i ("showAsSequence",                vars.getBool       ("showAsSequence"       ))
    ->set1ui("selectedView"  ,                vars.getUint32     ("selectedView"         ))
    ->set1i ("showQuilt"     ,                vars.getBool       ("showQuilt"            ))
    ->set1f ("pitch"         ,                vars.getFloat      ("quiltView.pitch"      ))
    ->set1f ("tilt"          ,                vars.getFloat      ("quiltView.tilt"       ))
    ->set1f ("center"        ,                vars.getFloat      ("quiltView.center"     ))
    ->set1f ("invView"       ,                vars.getFloat      ("quiltView.invView"    ))
    ->set1f ("subp"          ,                vars.getFloat      ("quiltView.subp"       ))
    ->set1i ("ri"            ,                vars.getInt32      ("quiltView.ri"         ))
    ->set1i ("bi"            ,                vars.getInt32      ("quiltView.bi"         ))
    ->set4fv("tile"          ,glm::value_ptr(*vars.get<glm::vec4>("quiltView.tile"       )))
    ->set4fv("viewPortion"   ,glm::value_ptr(*vars.get<glm::vec4>("quiltView.viewPortion")))
    ->use();

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void Holo::draw(){
  ge::gl::glClear(GL_DEPTH_BUFFER_BIT);
  createCamera(vars);
  basicCamera::CameraTransform*view;

  if(vars.getBool("useOrbitCamera"))
    view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  else{
    auto freeView = vars.get<basicCamera::FreeLookCamera>("view");
    float freeCameraSpeed = 0.01f;
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float((*keys)["d s"[a]] - (*keys)["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }


  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  if(vars.getBool("renderScene"))
    drawGrid(vars);
  else{
    auto quilt = vars.get<Quilt>("quilt");
    quilt->draw(
        [&](glm::mat4 const&view,glm::mat4 const&proj){drawGrid(vars,view,proj);},
        vars.getReinterpret<basicCamera::CameraTransform>("view")->getView(),
        vars.getReinterpret<basicCamera::CameraProjection>("projection")->getProjection()
        );
    drawHolo(vars);
  }

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();



  drawImguiVars(vars);

  swap();
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
  vars.addFloat("input.sensitivity",0.01f);
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addBool("useOrbitCamera",false);

  vars.addFloat      ("quiltView.pitch"      ,354.42108f);
  vars.addFloat      ("quiltView.tilt"       ,-0.1153f);
  vars.addFloat      ("quiltView.center"     ,0.04239f);
  vars.addFloat      ("quiltView.invView"    ,1.00f);
  vars.addFloat      ("quiltView.subp"       ,0.00013f);
  vars.addInt32      ("quiltView.ri"         ,0);
  vars.addInt32      ("quiltView.bi"         ,2);
  vars.add<glm::vec4>("quiltView.tile"       ,5.00f, 9.00f, 45.00f, 45.00f);
  vars.add<glm::vec4>("quiltView.viewPortion",0.99976f, 0.99976f, 0.00f, 0.00f);
  vars.addBool ("showQuilt");
  vars.addBool ("renderQuilt");
  vars.addBool ("renderScene",true);
  vars.addBool ("showAsSequence",false);
  vars.addUint32("selectedView",0);

  vars.addFloat("quiltRender.size",5.f);
  vars.addFloat("quiltRender.fov",90.f);
  vars.addFloat("quiltRender.viewCone",0.f);

  vars.add<Quilt>("quilt",vars);

  createCamera(vars);

}

void Holo::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
  if(event.key.keysym.sym == SDLK_f && DOWN){
    fullscreen = !fullscreen;
    if(fullscreen)
      window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
    else
      window->setFullscreen(sdl2cpp::Window::WINDOW);
  }
}

void Holo::mouseMove(SDL_Event const& e) {
  if(vars.getBool("useOrbitCamera")){
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.get<glm::uvec2>("windowSize");
    auto const orbitZoomSpeed = 0.1f;//vars.getFloat("args.camera.orbitZoomSpeed");
    auto const xrel           = static_cast<float>(e.motion.xrel);
    auto const yrel           = static_cast<float>(e.motion.yrel);
    auto const mState         = e.motion.state;
    if (mState & SDL_BUTTON_LMASK) {
      if (orbitCamera) {
        orbitCamera->addXAngle(yrel * sensitivity);
        orbitCamera->addYAngle(xrel * sensitivity);
      }
    }
    if (mState & SDL_BUTTON_RMASK) {
      if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
    }
    if (mState & SDL_BUTTON_MMASK) {
      orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
                                float(windowSize->x) * 2.f);
      orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                float(windowSize->y) * 2.f);
    }
  }else{
    auto const xrel           = static_cast<float>(e.motion.xrel);
    auto const yrel           = static_cast<float>(e.motion.yrel);
    auto view = vars.get<basicCamera::FreeLookCamera>("view");
    auto sensitivity = vars.getFloat("input.sensitivity");
    if (e.motion.state & SDL_BUTTON_LMASK) {
      view->setAngle(
          1, view->getAngle(1) + xrel * sensitivity);
      view->setAngle(
          0, view->getAngle(0) + yrel * sensitivity);
    }
  }
}


void Holo::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
  std::cerr << "resize(" << x << "," << y << ")" << std::endl;
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  Holo app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}