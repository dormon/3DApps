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
#include <addVarsLimits.h>

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv,330) {}
  virtual ~EmptyProject(){}
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

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.addFloat("input.sensitivity",0.01f);
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addBool("useOrbitCamera",false);
  vars.addUint32("nStripes",40);
  vars.addUint32("nCells",40);
  createCamera(vars);
}

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #line 74
  out vec3 vNormal;
  out vec3 vPosition;
  out vec3 vCamera;
  uniform uint nCells = 20u;
  uniform uint nStripes = 20u;
  uniform mat4 projection = mat4(1.f);
  uniform mat4 view = mat4(1.f);
  void main(){

    uint stripe     = uint(gl_VertexID) / (nCells*6u);
    uint cell       = (uint(gl_VertexID) % (nCells*6u)) / 6u;
    uint cellVertex = uint(gl_VertexID) % 6u;
    float xAngle = float(cell   + uint(cellVertex == 1u || cellVertex == 4u || cellVertex == 5u)) / float(nCells  ) * 3.141592f*2.f;
    float yAngle = float(stripe + uint(cellVertex == 2u || cellVertex == 3u || cellVertex == 5u)) / float(nStripes) * 3.141592f    ;
    vNormal = vec3(cos(xAngle)*sin(yAngle),sin(xAngle)*sin(yAngle),-cos(yAngle));
    vPosition = vNormal;
    gl_Position = projection*view*vec4(vNormal,1.f);
    vCamera = vec3(inverse(view)*vec4(0,0,0,1));
  }
  ).";
  std::string const fsSrc = R".(
  #line 96
  out vec4 fColor;
  in vec3 vNormal;
  in vec3 vPosition;
  in vec3 vCamera;
  uniform vec3 lightPosition = vec3(10,10,10);

  #define PI 3.141592

  float dotc(vec3 A,vec3 B){
    return clamp(dot(A,B),0.f,1.f);
  }

  vec3 schlick(vec3 r0,float ca){
    return r0 + (1.f-r0)*pow(1-ca,5.f);
  }

  float D(vec3 N,vec3 H,float s){
    return pow(dotc(N,H),s);
  }



  vec3 computeLight(vec3 r0,vec3 L,vec3 N,vec3 H,float s,vec3 ks,vec3 LC){
    float kn = (s+2.f)*(s+4.f)/(8.f*PI*(pow(2.f,-s/2.f)+s));
    return (schlick(r0,dotc(L,H))*D(N,H,s)*kn+ks/PI)*dotc(N,L)*PI*LC;
  }

  void main(){

    vec3 L = normalize(lightPosition - vPosition);
    vec3 V = normalize(vCamera - vPosition);
    vec3 N = normalize(vNormal);
    vec3 H = (L+V)/2.f;
    vec3 r0 = vec3(0.98f,0.82f,0.76f);
    vec3 ks = vec3(0.f,0.f,0.f);
    vec3 LC = vec3(1.f);
    fColor = vec4(computeLight(r0,L,N,H,30,ks,LC),1.f);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 330\n",
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 330\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("sphereProgram",vs,fs);

}

void createSphereVao(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;
  vars.reCreate<ge::gl::VertexArray>("sphereVAO");
}

void drawSphere(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createSphereVao(vars);
  createProgram(vars);
  ge::gl::glEnable(GL_DEPTH_TEST);
  vars.get<ge::gl::VertexArray>("sphereVAO")->bind();
  vars.get<ge::gl::Program>("sphereProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->set1ui("nStripes",vars.getUint32("nStripes"))
    ->set1ui("nCells",vars.getUint32("nCells"))
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLES,0,vars.getUint32("nStripes")*vars.getUint32("nCells")*2*3);
  vars.get<ge::gl::VertexArray>("sphereVAO")->unbind();
}

void drawSphere(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawSphere(vars,view->getView(),projection->getProjection());
}

void EmptyProject::draw(){
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
  ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawGrid(vars);
  drawSphere(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void EmptyProject::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
}

void EmptyProject::mouseMove(SDL_Event const& e) {
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
