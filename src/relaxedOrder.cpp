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

std::string const geometry = R".(
const vec3 vertices[8] = {
  vec3(-1,0,+1),
  vec3(+1,0,+1),
  vec3(-1,0,-1),
  //vec3(+10,0,-10),
  vec3(-1,0.0,+1),
  vec3(-2,3,+2),
  vec3(+2,3,+2),
  vec3(-2,3,-2),
  vec3(+2,3,-2),
};

const uint indices[12] = {
  0,1,2,
//  3,1,2,
  2,1,3,
  4,5,6,
  6,5,7,
};

vec3 getPosition(uint v){
  return vertices[indices[v]];
}

).";

std::string const trianglesVS = R".(
uniform mat4 view = mat4(1);
uniform mat4 proj = mat4(1);

layout(location=0)out flat uint triangleID;
layout(location=1)out vec3 position;
layout(location=2)out vec3 normal;

void main(){


  if(gl_VertexID<12){
    position = getPosition(gl_VertexID);
    normal = vec3(0,1,0);
    gl_Position = proj*view*vec4(position,1);
  }

  triangleID = gl_VertexID/3;
  
}

).";


std::string const trianglesFS = R".(
#version 450
layout(location=0)in flat uint triangleID;
layout(location=1)in vec3 position;
layout(location=2)in vec3 normal;

uniform vec4 light = vec4(10,10,10,1);
uniform mat4 view = mat4(1);
uniform mat4 proj = mat4(1);

out vec4 fColor;
void main(){
  vec3 color;
  if(triangleID == 0)color = vec3(1,0,0);
  if(triangleID == 1)color = vec3(0,1,0);
  if(triangleID >= 2) color = vec3(0.5,0.5,0);

  vec3 cam = vec3(inverse(proj*view)*vec4(0,0,0,1));
  vec3 L = normalize(light.xyz-position);
  vec3 N = normalize(normal);
  vec3 V = normalize(cam-position);
  vec3 R = reflect(-L,N);
  float dF = clamp(dot(L,N),0,1);
  float sF = pow(clamp(dot(R,V),0,1),100);

  vec3 finalColor = color*dF + sF*vec3(1);

  fColor = vec4(finalColor,1);
}
).";


//std::string const svVS = R".(
//
//).";
//
//std::string const svGS = R".(
//
//layout(points)in;
//layout(triangle_strip,max_vertices=20)out;
//
//void main(){
//
//}
//
//
//).";


using DVars = VarsGLMDecorator<vars::Vars>;

void prepareDrawTriangle(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  
  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      geometry,
      trianglesVS);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,trianglesFS);

  auto prg = vars.reCreate<ge::gl::Program>("drawTriangleProgram",vs,fs);
  prg->setNonexistingUniformWarning(false);
  
  vars.reCreate<ge::gl::VertexArray>("drawTriangleVAO");
}

void drawTriangle(vars::Vars&vars){

  prepareDrawTriangle(vars);

  auto prg = vars.get<ge::gl::Program>("drawTriangleProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawTriangleVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));


  prg->set4fv("light",(float*)vars.get<glm::vec4>("light"));

  ge::gl::glEnable(GL_DEPTH_TEST);
  //ge::gl::glEnable(GL_BLEND);
  //ge::gl::glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  ge::gl::glDrawArrays(GL_TRIANGLES,0,6);
  //ge::gl::glDisable(GL_BLEND);

  vao->unbind();
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
  else{
    auto cam = vars.reCreate<basicCamera::FreeLookCamera>("view");
    cam->setPosition(glm::vec3(0,1,0));
  }
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

  vars.add<glm::vec4>("light",glm::vec4(10,30,0,1));
  addVarsLimits3F(vars,"light",-100,100,0.1);

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
    float freeCameraSpeed = 0.1f;
    auto& keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float(keys["d s"[a]] - keys["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  //drawGrid(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();


  if(vars.addOrGetBool("drawTriangle"  ,true))drawTriangle  (vars);

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
