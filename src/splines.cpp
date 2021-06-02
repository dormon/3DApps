#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <drawGrid.h>
#include <imguiVars/addVarsLimits.h>
#include <VarsGLMDecorator/VarsGLMDecorator.h>


using DVars = VarsGLMDecorator<vars::Vars>;

#include<bunny.h>

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #line 24
  uniform mat4 projection;
  uniform mat4 view;
  uniform uint nofP = 100;

  uniform float alpha = 0.f;
  uniform vec4  w     = vec4(.25f);

  vec4 bernstein(float t){
    return vec4(
      (1-t)*(1-t)*(1-t),
    3*t    *(1-t)*(1-t),
    3*t    *t    *(1-t),
      t    *t    *t    );
  }

  vec3 control[4] = vec3[4](
    vec3(0  ,0  ,0  ),
    vec3(1  ,0  ,0.1),
    vec3(0  ,0.5,0.9),
    vec3(1  ,0.5,0.7)
  );

  vec3 bezier(float t){
    vec4 b = bernstein(t);

    vec3 P = w[0]*control[0]*b[0] + w[1]*control[1]*b[1] + w[2]*control[2]*b[2] + w[3]*control[3]*b[3];
    return P / dot(w,b);
  }

  void main(){
    mat4 mvp = projection*view;

    float t = gl_VertexID/float(nofP-1);



    vec3 B = vec3(bezier(t));



    gl_Position = projection*view*vec4(B,1);
  }
  ).";


  std::string const fsSrc = R".(
  out vec4 fColor;

  void main(){
    fColor = vec4(1.f);
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
  auto prg = vars.reCreate<ge::gl::Program>("program",vs,fs);
  prg->setNonexistingUniformWarning(false);

}

void createVAO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;
  auto vao = vars.reCreate<ge::gl::VertexArray>("vao");
}

void drawSpline(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createProgram(vars);
  createVAO(vars);
  ge::gl::glEnable(GL_DEPTH_TEST);
  auto vao = vars.get<ge::gl::VertexArray>("vao");
  vao->bind();
  auto nofP = vars.addOrGetUint32("nofP",100);
  bool asPoints = vars.addOrGetBool("asPoints",false);
  vars.get<ge::gl::Program>("program")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->set1ui      ("nofP"      ,nofP                )
    ->set1f       ("alpha"     ,vars.addOrGetFloat("alpha",0.f))
    ->set4fv      ("w"         ,(float*)vars.addOrGet<glm::vec4>("w",.25f,.25f,.25f,.25f))
    ->use();

  //vars.addOrGetFloat("alpha")+=10.f;

  ge::gl::glPointSize(5);
  if(asPoints)
    ge::gl::glDrawArrays(GL_POINTS,0,nofP);
  else
  ge::gl::glDrawArrays(GL_LINE_STRIP,0,nofP);
  vao->unbind();
}

void drawSpline(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawSpline(vars,view->getView(),projection->getProjection());
}


class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv,330) {}
  virtual ~EmptyProject(){}
  virtual void draw() override;

  DVars vars;

  virtual void                init() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};

void createView(DVars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"useOrbitCamera"}))return;

  if(vars.getBool("useOrbitCamera"))
    vars.reCreate<basicCamera::OrbitCamera>("view");
  else
    vars.reCreate<basicCamera::FreeLookCamera>("view");
}

void createProjection(DVars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize","camera.fovy","camera.near","camera.far"}))return;

  auto windowSize = vars.getUVec2("windowSize");
  auto width = windowSize.x;
  auto height = windowSize.y;
  auto aspect = (float)width/(float)height;
  auto nearv = vars.getFloat("camera.near");
  auto farv  = vars.getFloat("camera.far" );
  auto fovy = vars.getFloat("camera.fovy");

  vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,nearv,farv);
}

void createCamera(DVars&vars){
  createProjection(vars);
  createView(vars);
}

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.addFloat("input.sensitivity",0.01f);
  vars.addUVec2("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.addMap<SDL_Keycode, bool>("input.keyDown");
  vars.addBool("useOrbitCamera",false);
  createCamera(vars);
  window->setSize(1920,1080);
}

void EmptyProject::draw(){
  createCamera(vars);
  basicCamera::CameraTransform*view;

  if(vars.getBool("useOrbitCamera"))
    view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  else{
    auto freeView = vars.get<basicCamera::FreeLookCamera>("view");
    float freeCameraSpeed = 0.01f;
    auto& keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float(keys["d s"[a]] - keys["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawGrid(vars);

  drawSpline(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void EmptyProject::key(SDL_Event const& event, bool DOWN) {
  auto&keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
  keys[event.key.keysym.sym] = DOWN;
}

void orbitManipulator(DVars&vars,SDL_Event const&e){
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.getUVec2("windowSize");
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
                                float(windowSize.x) * 2.f);
      orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                float(windowSize.y) * 2.f);
    }
}

void freeLookManipulator(DVars&vars,SDL_Event const&e){
  auto const xrel = static_cast<float>(e.motion.xrel);
  auto const yrel = static_cast<float>(e.motion.yrel);
  auto view = vars.get<basicCamera::FreeLookCamera>("view");
  auto sensitivity = vars.getFloat("input.sensitivity");
  if (e.motion.state & SDL_BUTTON_LMASK) {
    view->setAngle(
        1, view->getAngle(1) + xrel * sensitivity);
    view->setAngle(
        0, view->getAngle(0) + yrel * sensitivity);
  }
}

void EmptyProject::mouseMove(SDL_Event const& e) {
  if(vars.getBool("useOrbitCamera"))
    orbitManipulator(vars,e);
  else
    freeLookManipulator(vars,e);
}

void EmptyProject::resize(uint32_t x,uint32_t y){
  auto&windowSize = vars.getUVec2("windowSize");
  windowSize.x = x;
  windowSize.y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  EmptyProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
