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

void prepareDrawSide(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  uniform vec3 A = vec3(0,0,0);
  uniform vec3 B = vec3(1,0,0);
  uniform vec4 light = vec4(10,10,10,1);
  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);
  void main(){
    if(gl_VertexID == 0)gl_Position = proj*view*vec4(A,1);
    if(gl_VertexID == 1)gl_Position = proj*view*vec4(B,1);
    if(gl_VertexID == 2)gl_Position = proj*view*vec4(A.xyz*light.w-light.xyz,0);
    if(gl_VertexID == 3)gl_Position = proj*view*vec4(B.xyz*light.w-light.xyz,0);
  }
  ).";

  std::string const fsSrc = R".(

  #version 450
  layout(location=0)out vec4 fColor;
  void main(){
    fColor = vec4(0,0,1,0);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawSideProgram",vs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawSideVAO");
}

void drawSide(vars::Vars&vars){
  prepareDrawSide(vars);

  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glDepthMask(GL_TRUE);
  auto prg = vars.get<ge::gl::Program>("drawSideProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawSideVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();
  auto light = *vars.get<glm::vec4>("light");
  auto A = *vars.get<glm::vec3>("A");
  auto B = *vars.get<glm::vec3>("B");

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));
  prg->set4fv("light",glm::value_ptr(light));
  prg->set3fv("A",glm::value_ptr(A));
  prg->set3fv("B",glm::value_ptr(B));

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vao->unbind();
}

void prepareDrawFrustum(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  void main(){
  }
  ).";

  std::string const gsSrc = R".(
  #version 450

  layout(points)in;
  layout(line_strip,max_vertices=100)out;

  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  uniform float ax = 0.f;
  uniform float bx = 1.f;
  uniform float ay = 0.f;
  uniform float by = 1.f;
  uniform float az = 1.f;
  uniform float bz = 10.f;

  layout(location=13)out vec3 gColor;
 
  void drawLine(vec3 a,vec3 b){
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    gl_Position = proj*view*vec4(b,1);EmitVertex();
    EndPrimitive();
  }

  void drawLoop(vec3 a,vec3 b,vec3 c,vec3 d){
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    gl_Position = proj*view*vec4(b,1);EmitVertex();
    gl_Position = proj*view*vec4(c,1);EmitVertex();
    gl_Position = proj*view*vec4(d,1);EmitVertex();
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    EndPrimitive();
  }


  void main(){
    vec3 fp[8];
    fp[0+0] = vec3(LL,BB,-NN);
    fp[0+1] = vec3(RR,BB,-NN);
    fp[0+2] = vec3(LL,TT,-NN);
    fp[0+3] = vec3(RR,TT,-NN);
    fp[4+0] = vec3(LL*FF/NN,BB*FF/NN,-FF);
    fp[4+1] = vec3(RR*FF/NN,BB*FF/NN,-FF);
    fp[4+2] = vec3(LL*FF/NN,TT*FF/NN,-FF);
    fp[4+3] = vec3(RR*FF/NN,TT*FF/NN,-FF);

    gColor = vec3(1,1,1);
    drawLoop(fp[0],fp[1],fp[3],fp[2]);
    drawLoop(fp[4],fp[5],fp[7],fp[6]);
    drawLine(fp[0],fp[4]);
    drawLine(fp[1],fp[5]);
    drawLine(fp[2],fp[6]);
    drawLine(fp[3],fp[7]);

    fp[0+0] = vec3(mix(LL,RR,ax),mix(BB,TT,ay),-az);
    fp[0+1] = vec3(mix(LL,RR,bx),mix(BB,TT,ay),-az);
    fp[0+2] = vec3(mix(LL,RR,ax),mix(BB,TT,by),-az);
    fp[0+3] = vec3(mix(LL,RR,bx),mix(BB,TT,by),-az);
    fp[4+0] = vec3(mix(LL,RR,ax)*bz/az,mix(BB,TT,ay)*bz/az,-bz);
    fp[4+1] = vec3(mix(LL,RR,bx)*bz/az,mix(BB,TT,ay)*bz/az,-bz);
    fp[4+2] = vec3(mix(LL,RR,ax)*bz/az,mix(BB,TT,by)*bz/az,-bz);
    fp[4+3] = vec3(mix(LL,RR,bx)*bz/az,mix(BB,TT,by)*bz/az,-bz);

    gColor = vec3(0,1,0);
    drawLoop(fp[0],fp[1],fp[3],fp[2]);
    drawLoop(fp[4],fp[5],fp[7],fp[6]);
    drawLine(fp[0],fp[4]);
    drawLine(fp[1],fp[5]);
    drawLine(fp[2],fp[6]);
    drawLine(fp[3],fp[7]);

  }

  ).";

  std::string const fsSrc = R".(

  #version 450
  layout(location=0)out vec4 fColor;
  layout(location=13)in vec3 gColor;
  void main(){
    fColor = vec4(gColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,gsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawFrustumProgram",vs,gs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawFrustumVAO");
}

void drawFrustum(vars::Vars&vars){
  prepareDrawFrustum(vars);

  auto prg = vars.get<ge::gl::Program>("drawFrustumProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawFrustumVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));

  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set1f("ax",vars.getFloat("ax"));
  prg->set1f("bx",vars.getFloat("bx"));
  prg->set1f("ay",vars.getFloat("ay"));
  prg->set1f("by",vars.getFloat("by"));
  prg->set1f("az",vars.getFloat("az"));
  prg->set1f("bz",vars.getFloat("bz"));

  ge::gl::glDrawArrays(GL_POINTS,0,1);

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

  vars.add<glm::vec4>("light",glm::vec4(10,30,10,1));
  vars.add<glm::vec3>("A",glm::vec3(0,2,-3));
  vars.add<glm::vec3>("B",glm::vec3(1,2,-3));
  addVarsLimits3F(vars,"A"    ,-10,10,0.1);
  addVarsLimits3F(vars,"B"    ,-10,10,0.1);
  addVarsLimits3F(vars,"light",-100,100,0.1);


  vars.addFloat("LL",-1.f);
  vars.addFloat("RR",+1.f);
  vars.addFloat("BB",-1.f);
  vars.addFloat("TT",+1.f);
  vars.addFloat("NN",1.f);
  vars.addFloat("FF",10.f);

  vars.addFloat("ax",0.3);
  vars.addFloat("bx",0.5);
  vars.addFloat("ay",0.3);
  vars.addFloat("by",0.5);
  vars.addFloat("az",3);
  vars.addFloat("bz",6.f);

  addVarsLimitsF(vars,"ax",0,1,0.01);
  addVarsLimitsF(vars,"bx",0,1,0.01);
  addVarsLimitsF(vars,"ay",0,1,0.01);
  addVarsLimitsF(vars,"by",0,1,0.01);
  addVarsLimitsF(vars,"az",1,10,0.01);
  addVarsLimitsF(vars,"bz",1,10,0.01);

  createCamera(vars);
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

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawSide(vars);
  drawFrustum(vars);

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
