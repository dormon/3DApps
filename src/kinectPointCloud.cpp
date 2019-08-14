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
#include <k4a/k4a.hpp>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

class KinectPointCloud: public simple3DApp::Application{
 public:
  KinectPointCloud(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~KinectPointCloud(){}
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

void createPointCloudProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  uniform mat4 projection;
  uniform mat4 view;

  out vec3 vColor;

  layout(binding=0)uniform sampler2D colorTexture;
  layout(binding=1)uniform isampler2D depthTexture;

  uniform vec2 range;

  void main(){
    ivec2 coord;
    ivec2 size = textureSize(colorTexture,0);
    coord.x = gl_VertexID%size.x;
    coord.y = gl_VertexID/size.x;
    vColor = texelFetch(colorTexture,coord,0).xyz;
    float depth = texelFetch(depthTexture,coord,0).x;
    if(depth == 0)
        depth = 2500;
    depth = (clamp(depth, range.s, range.t)-range.s)/(range.t-range.s);
    float z = -depth;
    vec2 ndc = vec2(coord)/vec2(size)*2-1;
    gl_Position = projection * view * vec4(-ndc,z,1);
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
  vars.reCreate<ge::gl::Program>("pointCloudProgram",vs,fs)->setNonexistingUniformWarning(false);
}


void drawPointCloud(vars::Vars&vars){
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
    ->set2fv("range",reinterpret_cast<float*>(vars.get<glm::vec2>("range")))
    ->use();

  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glPointSize(vars.getFloat("image.pointSize"));
  ge::gl::glDrawArrays(GL_POINTS,0,colorTex->getWidth(0)*colorTex->getHeight(0));
  ge::gl::glDisable(GL_DEPTH_TEST);
}

glm::ivec2 getColorResolution(const k4a_color_resolution_t resolution)
{
    switch (resolution)
    {
    case K4A_COLOR_RESOLUTION_720P:
        return { 1280, 720 };
    case K4A_COLOR_RESOLUTION_2160P:
        return { 3840, 2160 };
    case K4A_COLOR_RESOLUTION_1440P:
        return { 2560, 1440 };
    case K4A_COLOR_RESOLUTION_1080P:
        return { 1920, 1080 };
    case K4A_COLOR_RESOLUTION_3072P:
        return { 4096, 3072 };
    case K4A_COLOR_RESOLUTION_1536P:
        return { 2048, 1536 };

    default:
        throw std::logic_error("Invalid color dimensions value!");
    }
}

glm::ivec2 getDepthResolution(const k4a_depth_mode_t depthMode)
{
    switch (depthMode)
    {
    case K4A_DEPTH_MODE_NFOV_2X2BINNED:
        return { 320, 288 };
    case K4A_DEPTH_MODE_NFOV_UNBINNED:
        return { 640, 576 };
    case K4A_DEPTH_MODE_WFOV_2X2BINNED:
        return { 512, 512 };
    case K4A_DEPTH_MODE_WFOV_UNBINNED:
        return { 1024, 1024 };
    case K4A_DEPTH_MODE_PASSIVE_IR:
        return { 1024, 1024 };

    default:
        throw std::logic_error("Invalid depth dimensions value!");
    }
}

glm::ivec2 getDepthRange(const k4a_depth_mode_t depthMode)
{
    switch (depthMode)
    {
    case K4A_DEPTH_MODE_NFOV_2X2BINNED:
        return { 500, 5800 };
    case K4A_DEPTH_MODE_NFOV_UNBINNED:
        return { 500, 4000 };
    case K4A_DEPTH_MODE_WFOV_2X2BINNED:
        return { 250, 3000 };
    case K4A_DEPTH_MODE_WFOV_UNBINNED:
        return { 250, 2500 };

    case K4A_DEPTH_MODE_PASSIVE_IR:
    default:
        throw std::logic_error("Invalid depth mode!");
    }
}



void KinectPointCloud::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }
  
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.addFloat("input.sensitivity",0.01f);
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addBool("useOrbitCamera",false);
  
  vars.addFloat("image.near"     ,6.f                  );
  vars.addFloat("image.far"      ,1000.f               );
  vars.addFloat("image.fovy"     ,glm::half_pi<float>());
  vars.addFloat("image.pointSize",5                    );
  createCamera(vars);

  const uint32_t deviceCount = k4a::device::get_installed_count();
  if (deviceCount == 0)
    throw std::runtime_error("No Azure Kinect connected");

  k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
  config.depth_mode = K4A_DEPTH_MODE_WFOV_UNBINNED;
  config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
  config.color_resolution = K4A_COLOR_RESOLUTION_2160P;
  config.camera_fps = (config.color_resolution == K4A_COLOR_RESOLUTION_2160P) ? K4A_FRAMES_PER_SECOND_15 : K4A_FRAMES_PER_SECOND_30;
  config.synchronized_images_only = true;
  auto size = vars.add<glm::ivec2>("colorSize"); 
  *size = getColorResolution(config.color_resolution);
  vars.add<glm::vec2>("range", getDepthRange(config.depth_mode));
  auto dev = vars.add<k4a::device>("kinectDevice");
  *dev = k4a::device::open(K4A_DEVICE_DEFAULT);
  dev->start_cameras(&config); 
  auto depthImageTrans = vars.add<k4a::image>("depthImageTrans");
  *depthImageTrans = k4a::image::create(K4A_IMAGE_FORMAT_DEPTH16, size->x, size->y, size->x * (int)sizeof(uint16_t));
  //auto pcImage = vars.add<k4a::image>("pcImage");
  //*pcImage = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM, size->x, size->y, 3*size->x * (int)sizeof(uint16_t));
  auto trans = vars.add<k4a::transformation>("transformation",dev->get_calibration(config.depth_mode, config.color_resolution)); 
  vars.reCreate<ge::gl::Texture>("colorTexture",GL_TEXTURE_2D,GL_RGBA32F,1,size->x,size->y); 
  vars.reCreate<ge::gl::Texture>("depthTexture",GL_TEXTURE_2D,GL_R16UI,1,size->x,size->y); 
}

void KinectPointCloud::draw(){
    k4a::capture capture;
    auto dev = vars.get<k4a::device>("kinectDevice");
    if (dev->get_capture(&capture, std::chrono::milliseconds(0)))
    {
        const k4a::image depthImage = capture.get_depth_image();
        const k4a::image colorImage = capture.get_color_image();
        auto depthImageTrans = vars.get<k4a::image>("depthImageTrans");
        auto trans = vars.get<k4a::transformation>("transformation");
        trans->depth_image_to_color_camera(depthImage, depthImageTrans);
        //auto pcImage = vars.get<k4a::image>("pcImage");
        //trans->depth_image_to_point_cloud(*depthImageTrans, K4A_CALIBRATION_TYPE_COLOR, pcImage);

        auto size = vars.get<glm::ivec2>("colorSize");
        ge::gl::glTextureSubImage2D(vars.get<ge::gl::Texture>("depthTexture")->getId(),0,0,0,size->x, size->y,GL_RED_INTEGER,GL_UNSIGNED_SHORT,depthImageTrans->get_buffer());
        auto colorTexture = vars.get<ge::gl::Texture>("colorTexture");
        ge::gl::glTextureSubImage2D(colorTexture->getId(),0,0,0,size->x, size->y,GL_BGRA,GL_UNSIGNED_BYTE,colorImage.get_buffer()); 
    }

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

void KinectPointCloud::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
}

void KinectPointCloud::mouseMove(SDL_Event const& e) {
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

void KinectPointCloud::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  KinectPointCloud app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
