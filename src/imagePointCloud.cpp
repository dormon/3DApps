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
#include <DrawGrid.h>
#include <FreeImagePlus.h>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

class ImagePointCloud: public simple3DApp::Application{
 public:
  ImagePointCloud(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~ImagePointCloud(){}
  virtual void draw() override;

  vars::Vars vars;

  virtual void                init() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                resize(uint32_t x,uint32_t y) override;
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
  if(notChanged(vars,"all",__FUNCTION__,{"colorFileName"}))return;
  fipImage colorImg;
  colorImg.load(vars.getString("colorFileName").c_str());
  auto const width   = colorImg.getWidth();
  auto const height  = colorImg.getHeight();
  auto const BPP     = colorImg.getBitsPerPixel();
  auto const imgType = colorImg.getImageType();
  auto const data    = colorImg.accessPixels();

  std::cerr << "color BPP : " << BPP << std::endl;
  std::cerr << "color type: " << imgType << std::endl;

  vars.add<glm::uvec2>("colorTextureSize",glm::uvec2(width,height));

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
      "colorTexture",GL_TEXTURE_2D,GL_RGB8,1,width,height);
  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
  ge::gl::glTextureSubImage2D(colorTex->getId(),0,0,0,width,height,format,type,data);
  //colorTex->setData2D(colorImg.accessPixels(),GL_BGRA,GL_UNSIGNED_BYTE);
}

void loadDepthTexture(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"depthFileName"}))return;
  fipImage colorImg;
  colorImg.load(vars.getString("depthFileName").c_str());
  auto const width   = colorImg.getWidth();
  auto const height  = colorImg.getHeight();
  auto const BPP     = colorImg.getBitsPerPixel();
  auto const imgType = colorImg.getImageType();
  auto const data    = colorImg.accessPixels();

  std::cerr << "depth BPP : " << BPP << std::endl;
  std::cerr << "depth type: " << imgType << std::endl;

  vars.add<glm::uvec2>("depthTextureSize",glm::uvec2(width,height));

  GLenum format;
  GLenum type;
  if(imgType == FIT_BITMAP){
    if(BPP == 24)format = GL_BGR;
    if(BPP == 32)format = GL_BGRA;
    type = GL_UNSIGNED_BYTE;
  }
  if(imgType == FIT_FLOAT){
    format = GL_RED;
    type = GL_FLOAT;
  }
  if(imgType == FIT_RGBF){
    format = GL_RGB;
    type = GL_FLOAT;
  }


  auto depthTex = vars.reCreate<ge::gl::Texture>(
      "depthTexture",GL_TEXTURE_2D,GL_R32F,1,width,height);
  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
  ge::gl::glTextureSubImage2D(depthTex->getId(),0,0,0,width,height,format,type,data);
  vars.addFloat("image.near"     ,6.f                  );
  vars.addFloat("image.far"      ,1000.f               );
  vars.addFloat("image.fovy"     ,glm::half_pi<float>());
  vars.addFloat("image.pointSize",5                    );
  //colorTex->setData2D(colorImg.accessPixels(),GL_BGRA,GL_UNSIGNED_BYTE);
}

void loadTextures(vars::Vars&vars){
  loadColorTexture(vars);
  loadDepthTexture(vars);
}

void createPointCloudProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  uniform mat4 projection;
  uniform mat4 view;

  out vec3 vColor;

  layout(binding=0)uniform sampler2D colorTexture;
  layout(binding=1)uniform sampler2D depthTexture;

  float depthToZ(float d,float n,float f){
    return (2*f*n) / (d*f-d*n-f-n);
  }

  uniform float near = 6.f;
  uniform float far  = 1000.f;
  uniform float fovy = 3.141592f/2.f;

  vec4 compute3DPoint(vec2 ndc,float d,float n,float f,float fovy){
    float z      = depthToZ(d,n,f);
    vec4 post    = vec4(ndc*(-z),d*(-z),(-z));
    ivec2 size   = textureSize(colorTexture,0);
    float aspect = float(size.x)/float(size.y);
    float R = n*tan(fovy/2);
    float L = -R;
    float T = R/aspect;
    float B = -T;
    mat4 invP;
    invP[0] = vec4((R-L)/(2*n),0          ,0           , 0            );
    invP[1] = vec4(0          ,(T-B)/(2*n),0           , 0            );
    invP[2] = vec4(0          ,0          ,0           ,-(f-n)/(2*f*n));
    invP[3] = vec4((R+L)/(2*n),(T+B)/(2*n),-1          , (n+f)/(2*f*n));
    
    return vec4(normalize(vec3(ndc*vec2(R,T),-n))*d,1);
    //return invP * post;

  }


  void main(){
    ivec2 coord;
    ivec2 size = textureSize(colorTexture,0);
    coord.x = gl_VertexID%size.x;
    coord.y = gl_VertexID/size.x;
    vColor = texelFetch(colorTexture,coord,0).xyz;
    float depth = texelFetch(depthTexture,coord,0).x;
    float z = depthToZ(depth,near,far);
    vec2 ndc = vec2(coord)/vec2(size)*2-1;
    //ndc *= vec2(float(size.x)/float(size.y),1);
    gl_Position = projection * view * vec4(ndc,z,1);
    gl_Position = projection * view * compute3DPoint(ndc,depth,near,far,fovy);
  }
  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  in  vec3 vColor;
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
  vars.reCreate<ge::gl::Program>("pointCloudProgram",vs,fs);
}


void drawPointCloud(vars::Vars&vars){
  loadTextures(vars);
  createPointCloudProgram(vars);

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

  auto colorTex     = vars.get<ge::gl::Texture>("colorTexture");
  auto depthTex     = vars.get<ge::gl::Texture>("depthTexture");
  auto const width  = colorTex->getWidth (0);
  auto const height = colorTex->getHeight(1);
  colorTex->bind(0);
  depthTex->bind(1);
  vars.get<ge::gl::Program>("pointCloudProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view->getView()))
    ->setMatrix4fv("projection",glm::value_ptr(projection->getProjection()))
    ->set1f("near",vars.getFloat("image.near"))
    ->set1f("far" ,vars.getFloat("image.far" ))
    ->set1f("fovy",vars.getFloat("image.fovy"))
    ->use();

  auto size = vars.get<glm::uvec2>("colorTextureSize");
  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glPointSize(vars.getFloat("image.pointSize"));
  ge::gl::glDrawArrays(GL_POINTS,0,size->x*size->y);
  ge::gl::glDisable(GL_DEPTH_TEST);
}

void ImagePointCloud::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  auto const colorFile = args->gets("--color","","color image");
  auto const depthFile = args->gets("--depth","","depth image");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  vars.addString("colorFileName",colorFile);
  vars.addString("depthFileName",depthFile);
  
  
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.addFloat("input.sensitivity",0.01f);
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addBool("useOrbitCamera",false);
  createCamera(vars);
}

void ImagePointCloud::draw(){
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

  drawGrid(vars);
  drawPointCloud(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void ImagePointCloud::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
}

void ImagePointCloud::mouseMove(SDL_Event const& e) {
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

void ImagePointCloud::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  ImagePointCloud app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
