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


#include <fstream>

void storeCamera(vars::Vars&vars){
  std::string name = "storedCamera.txt";
  auto&cam = *vars.get<basicCamera::FreeLookCamera>("view");

  auto pos = cam.getPosition();
  auto a0 = cam.getAngle(0);
  auto a1 = cam.getAngle(1);
  auto a2 = cam.getAngle(2);

  std::ofstream file (name);
  if(!file.is_open()){
    std::cerr << "cannot open camera file: " << name << std::endl;
    return;
  }

  file << *(uint32_t*)&pos.x << std::endl;
  file << *(uint32_t*)&pos.y << std::endl;
  file << *(uint32_t*)&pos.z << std::endl;
  file << *(uint32_t*)&a0 << std::endl;
  file << *(uint32_t*)&a1 << std::endl;
  file << *(uint32_t*)&a2 << std::endl;

  file.close();

}

void loadCamera(vars::Vars&vars){
  std::string name = "storedCamera.txt";
  auto&cam = *vars.get<basicCamera::FreeLookCamera>("view");

  std::ifstream file (name);
  if(!file.is_open()){
    std::cerr << "cannot open camera file: " << name << std::endl;
    return;
  }
  glm::vec3 pos;
  uint32_t data[6];
  file >> data[0];
  file >> data[1];
  file >> data[2];
  file >> data[3];
  file >> data[4];
  file >> data[5];

  pos.x = *(float*)(data+0);
  pos.y = *(float*)(data+1);
  pos.z = *(float*)(data+2);
  float a0,a1,a2;
  a0 = *(float*)(data+3);
  a1 = *(float*)(data+4);
  a2 = *(float*)(data+5);

  cam.setPosition(pos);
  cam.setAngle(0,a0);
  cam.setAngle(1,a1);
  cam.setAngle(2,a2);

  file.close();
}

std::string const geometry = R".(

uniform bool flipBase = false;
uniform bool flipCaster = false;

const vec3 vertices[8] = {
  vec3(-10,0,+10),
  vec3(+10,0,+10),
  vec3(-10,0,-10),
  vec3(+10,0,-10),
  vec3(-1,3,+1),
  vec3(+1,3,+1),
  vec3(-1,3,-1),
  vec3(+1,3,-1),
};

const uint indices[] = {
  0,1,2,
  2,1,3,
  4,5,6,
  6,5,7,
  3,1,2,
  7,5,6,
};

vec3 getPosition(uint v){
  if(flipBase){
    if(v>2 && v<6)v+=9;
  }
  if(flipCaster){
    if(v > 8 && v < 12)v+=6;
  }
  return vertices[indices[v]];
}

).";

std::string const trianglesVS = R".(
uniform mat4 mvp = mat4(1);

layout(location=0)out flat uint triangleID;
layout(location=1)out vec3 position;
layout(location=2)out vec3 normal;

void main(){


  if(gl_VertexID<12){
    position = getPosition(gl_VertexID);
    normal = vec3(0,1,0);
    gl_Position = mvp*vec4(position,1);
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
uniform mat4 mvp = mat4(1);
uniform bool drawAmbient = true;
uniform bool drawDiff    = true;

out vec4 fColor;
void main(){
  vec3 color;
  if(triangleID < 2)color = vec3(0,0.5,0);
  //if(triangleID == 0)color = vec3(0,0.5,0);
  //if(triangleID == 1)color = vec3(0,0,.5);
  if(triangleID >= 2) color = vec3(0.5,0.5,0);

  vec3 cam = vec3(inverse(mvp)*vec4(0,0,0,1));
  vec3 L = normalize(light.xyz-position);
  vec3 N = normalize(normal);
  vec3 V = normalize(cam-position);
  vec3 R = reflect(-L,N);
  float dF = clamp(dot(L,N),0,1);
  float sF = pow(clamp(dot(R,V),0,1),100);

  float aF = 0.1f;
  vec3 finalColor = vec3(0);

  if(drawAmbient)
    finalColor += color*aF;

  if(drawDiff)
    finalColor += color*dF + sF*vec3(1);

  fColor = vec4(finalColor,1);
}
).";


std::string const svVS = R".(
out flat uint vID;
void main(){
 vID = gl_VertexID;
}
).";

std::string const svGS = R".(

layout(points)in;
layout(triangle_strip,max_vertices=20)out;

in flat uint vID[];

uniform vec4 light = vec4(10,10,10,1);
uniform mat4 mvp = mat4(1);


bool flip(vec4 a,vec4 b,vec4 c,vec4 l){
  vec3 n = normalize(cross(b.xyz-a.xyz,c.xyz-a.xyz));
  vec4 p = vec4(n,-dot(n,a.xyz));
  return dot(p,l) < 0;
}


void main(){
  vec4 P[6];
  P[0] = vec4(getPosition(vID[0]*3+0),1);
  P[1] = vec4(getPosition(vID[0]*3+1),1);
  P[2] = vec4(getPosition(vID[0]*3+2),1);
  P[3] = vec4(P[0].xyz*light.w-light.xyz,0);
  P[4] = vec4(P[1].xyz*light.w-light.xyz,0);
  P[5] = vec4(P[2].xyz*light.w-light.xyz,0);

  if(flip(P[0],P[1],P[2],light)){
    vec4 a;
    vec4 b;
    vec4 c;
    a=P[0];
    b=P[1];
    c=P[2];
    P[0]=c;
    P[1]=b;
    P[2]=a;

    a=P[3];
    b=P[4];
    c=P[5];
    P[3]=c;
    P[4]=b;
    P[5]=a;
    //a=P[0];P[0]=P[1];P[1]=a;
    //a=P[3];P[3]=P[4];P[4]=a;
  }

  gl_Position = mvp*P[3];EmitVertex();
  gl_Position = mvp*P[4];EmitVertex();
  gl_Position = mvp*P[0];EmitVertex();
  gl_Position = mvp*P[1];EmitVertex();
  gl_Position = mvp*P[2];EmitVertex();
  gl_Position = mvp*P[4];EmitVertex();
  gl_Position = mvp*P[5];EmitVertex();
  gl_Position = mvp*P[3];EmitVertex();
  gl_Position = mvp*P[2];EmitVertex();
  gl_Position = mvp*P[0];EmitVertex();
  
}


).";

std::string const svFS = R".(
out vec4 fColor;
void main(){
  if(gl_FrontFacing)
    fColor = vec4(1,0,0,1);
  else
    fColor = vec4(0,0,1,1);
}
).";

using DVars = VarsGLMDecorator<vars::Vars>;


void prepareDrawSV(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  
  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      svVS);
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,
      "#version 450\n",
      geometry,
      svGS);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      svFS);

  auto prg = vars.reCreate<ge::gl::Program>("drawSVProgram",vs,gs,fs);
  prg->setNonexistingUniformWarning(false);
  
  vars.reCreate<ge::gl::VertexArray>("drawSVVAO");
}

void drawSV(vars::Vars&vars){

  prepareDrawSV(vars);

  auto prg = vars.get<ge::gl::Program>("drawSVProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawSVVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  auto mvp = proj*view;
  prg->setMatrix4fv("mvp",glm::value_ptr(mvp));

  prg->set4fv("light",(float*)vars.get<glm::vec4>("light"));

  prg->set1i("flipBase",vars.addOrGetBool("flipBase",false));
  prg->set1i("flipCaster",vars.addOrGetBool("flipCaster",false));

  ge::gl::glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
  //ge::gl::glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glEnable(GL_STENCIL_TEST);
  ge::gl::glStencilFunc(GL_ALWAYS,0,0xff);
  ge::gl::glEnable(GL_DEPTH_CLAMP);

  ge::gl::glDepthFunc(GL_LESS);
  ge::gl::glDepthMask(GL_FALSE);
  //ge::gl::glStencilMask(0xff);
  ge::gl::glStencilOpSeparate(GL_FRONT,GL_KEEP,GL_INCR_WRAP,GL_KEEP);
  ge::gl::glStencilOpSeparate(GL_BACK,GL_KEEP,GL_DECR_WRAP,GL_KEEP);


  ge::gl::glDrawArrays(GL_POINTS,0,4);

  ge::gl::glDepthMask(GL_TRUE);
  ge::gl::glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  //ge::gl::glStencilMask(0x00);


  vao->unbind();
}

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

  auto&drawAmbient = vars.getBool("drawAmbient");
  auto&drawDiff    = vars.getBool("drawDiff");

  auto prg = vars.get<ge::gl::Program>("drawTriangleProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawTriangleVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  auto mvp = proj*view;
  prg->setMatrix4fv("mvp",glm::value_ptr(mvp));


  prg->set4fv("light",(float*)vars.get<glm::vec4>("light"));
  prg->set1i("drawAmbient",drawAmbient);
  prg->set1i("drawDiff",drawDiff);
  prg->set1i("flipBase",vars.addOrGetBool("flipBase",false));
  prg->set1i("flipCaster",vars.addOrGetBool("flipCaster",false));

  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glDepthFunc(GL_LEQUAL);
  ge::gl::glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

  ge::gl::glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);


  if(drawAmbient){
    ge::gl::glDisable(GL_STENCIL_TEST);
  }

  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glDepthMask(GL_TRUE);

  if(drawDiff){
    ge::gl::glEnable(GL_BLEND);
    ge::gl::glBlendFunc(GL_ONE,GL_ONE);
    ge::gl::glEnable(GL_STENCIL_TEST);
    ge::gl::glStencilFunc(GL_EQUAL,0,0xff);
  }

  ge::gl::glDrawArrays(GL_TRIANGLES,0,12);


  ge::gl::glDisable(GL_BLEND);
  ge::gl::glDisable(GL_STENCIL_TEST);
  ge::gl::glBlendFunc(GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA);

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

  vars.add<glm::vec4>("light",glm::vec4(0,500,0,1));
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
    float freeCameraSpeed = vars.addOrGetFloat("speed",0.1f);
    auto& keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float(keys["d s"[a]] - keys["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);


  auto&drawAmbient = vars.addOrGetBool("drawAmbient",true);
  auto&drawDiff    = vars.addOrGetBool("drawDiff",false);

  drawAmbient = true;
  drawDiff    = false ;
  drawTriangle  (vars);

  drawSV        (vars);

  drawAmbient = false;
  drawDiff    = true ;
  drawTriangle  (vars);

  if(ImGui::Button("save camera"))
    storeCamera(vars);
  if(ImGui::Button("load camera"))
    loadCamera(vars);

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
