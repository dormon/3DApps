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

std::string skala = R".(
vec4 getClipPlaneSkala(in vec4 A,in vec4 B,in vec4 C){
  float x1 = A.x;
  float x2 = B.x;
  float x3 = C.x;
  float y1 = A.y;
  float y2 = B.y;
  float y3 = C.y;
  float z1 = A.z;
  float z2 = B.z;
  float z3 = C.z;
  float w1 = A.w;
  float w2 = B.w;
  float w3 = C.w;

  float a =  y1*(z2*w3-z3*w2) - y2*(z1*w3-z3*w1) + y3*(z1*w2-z2*w1);
  float b = -x1*(z2*w3-z3*w2) + x2*(z1*w3-z3*w1) - x3*(z1*w2-z2*w1);
  float c =  x1*(y2*w3-y3*w2) - x2*(y1*w3-y3*w1) + x3*(y1*w2-y2*w1);
  float d = -x1*(y2*z3-y3*z2) + x2*(y1*z3-y3*z1) - x3*(y1*z2-y2*z1);
  return vec4(a,b,c,d);
}
).";

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  uniform vec3 A = vec3(0,0,-1);
  uniform vec3 B = vec3(1,0,-1);
  uniform vec3 C = vec3(0,1,-1);
  uniform mat4 projection = mat4(1);
  uniform mat4 view       = mat4(1);
  uniform int doA = 1;
  uniform int doB = 1;
  uniform int doC = 1;
  uniform float f = 1.f;
  uniform uvec2 windowSize;
  uniform int clipSpace = 1;
  out vec3 vColor;

  vec4 plane(vec3 a,vec3 b,vec3 c){
    vec3 n = cross(b-a,c-a);
    return vec4(n,-dot(a,n));
  }

  void com(out vec3 af,out vec3 bf,out vec3 cf,in vec3 a,in vec3 b,in vec3 c,in vec3 mask,in vec2 hPixel){
    vec2 A =af.xy/af.z;
    vec2 B = bf.xy/bf.z;
    vec2 C = cf.xy/cf.z;
    float ee = hPixel.x*hPixel.y*4*10000;
    if(length(cross(vec3(C-A,0),vec3(B-A,0))) < ee){
      af = a;
      bf = a;
      cf = a;
      return;
    }

    vec3 pa = cross(a,c);
    vec3 pb = cross(b,a);
    vec3 pc = cross(c,b);

    pa.z += dot(hPixel,abs(pa.xy))*mask.x;
    pb.z += dot(hPixel,abs(pb.xy))*mask.y;
    pc.z += dot(hPixel,abs(pc.xy))*mask.z;

    af = cross(pa,pb);
    bf = cross(pb,pc);
    cf = cross(pc,pa);


    vec4 pp = plane(a,b,c);

    af *= abs(-pp.w/dot(pp.xyz,af));
    bf *= abs(-pp.w/dot(pp.xyz,bf));
    cf *= abs(-pp.w/dot(pp.xyz,cf));
    
  }
  void computeConservative(out vec4 af,out vec4 bf,out vec4 cf,in vec4 a,in vec4 b,in vec4 c,in vec3 mask,in vec2 hPixel){
    vec4 plane = getClipPlaneSkala(a,b,c);

    //vec3 pa = cross(a.xyw-c.xyw,c.xyw);
    //vec3 pb = cross(b.xyw-a.xyw,a.xyw);
    //vec3 pc = cross(c.xyw-b.xyw,b.xyw);
    vec3 pa = cross(a.xyw,c.xyw);
    vec3 pb = cross(b.xyw,a.xyw);
    vec3 pc = cross(c.xyw,b.xyw);


    pa.z += dot(hPixel,abs(pa.xy))*mask.x;
    pb.z += dot(hPixel,abs(pb.xy))*mask.y;
    pc.z += dot(hPixel,abs(pc.xy))*mask.z;

    af.xyw = cross(pb,pa);
    bf.xyw = cross(pc,pb);
    cf.xyw = cross(pa,pc);

    af.z = -dot(af.xyw,plane.xyw) / plane.z;
    bf.z = -dot(bf.xyw,plane.xyw) / plane.z;
    cf.z = -dot(cf.xyw,plane.xyw) / plane.z;

  }

  void main(){
    if(f == 1337)return;
    mat4 m = projection*view;
    vec4 a = m*vec4(A,1);
    vec4 b = m*vec4(B,1);
    vec4 c = m*vec4(C,1);

    vec4 af;
    vec4 bf;
    vec4 cf;

    vec2 hPixel = vec2(1.f/vec2(windowSize));
    vec3 mask = vec3(doA,doB,doC)*f;
    if(clipSpace == 0){

      vec3 aaf,bbf,ccf;
      com(aaf,bbf,ccf,vec3(view*vec4(A,1)),vec3(view*vec4(B,1)),vec3(view*vec4(C,1)),mask,hPixel);

      af = projection*vec4(aaf,1);
      bf = projection*vec4(bbf,1);
      cf = projection*vec4(ccf,1);
    }else
      computeConservative(af,bf,cf,a,b,c,mask,hPixel);
  

    uint tri = gl_VertexID/3;
    uint ver = gl_VertexID%3;
    if(tri == 0){
      vColor = vec3(0,.5,0);

      vec3 u = vec3(view*vec4(A,1));
      vec3 v = vec3(view*vec4(B,1));
      vec3 w = vec3(view*vec4(C,1));

      vec2 AA = u.xy/u.z;
      vec2 BB = v.xy/v.z;
      vec2 CC = w.xy/w.z;
      float ee = hPixel.x*hPixel.y*4*1000;
      if(length(cross(vec3(CC-AA,0),vec3(BB-AA,0))) < ee){
        vColor = vec3(0,0,1);
      }


      if(ver == 0)gl_Position = a;
      if(ver == 1)gl_Position = b;
      if(ver == 2)gl_Position = c;
    }
    if(tri == 1){
      vColor = vec3(1,0,0);
      if(ver == 0)gl_Position = af;
      if(ver == 1)gl_Position = bf;
      if(ver == 2)gl_Position = cf;
    }
  }
  ).";
  std::string const fsSrc = R".(
  out vec4 fColor;
  in vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      skala,
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("program",vs,fs);
  vars.reCreate<ge::gl::VertexArray>("vao");

}

void drawLIH(vars::Vars&vars){
  createProgram(vars);

  auto vao = vars.get<ge::gl::VertexArray>("vao");
  vao->bind();
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  auto prg =  vars.get<ge::gl::Program>("program");
  prg
    ->setMatrix4fv("view"      ,glm::value_ptr(view->getView()))
    ->setMatrix4fv("projection",glm::value_ptr(projection->getProjection()))
    ->use();
  prg->set3fv("A",(float*)vars.addOrGet<glm::vec3>("A",0.f,0.f,-1.f));
  prg->set3fv("B",(float*)vars.addOrGet<glm::vec3>("B",0.f,1.f,-1.f));
  prg->set3fv("C",(float*)vars.addOrGet<glm::vec3>("C",1.f,0.f,-1.f));
  prg->set1f("f",vars.addOrGetFloat("f",1));
  prg->set2uiv("windowSize",(uint32_t*)vars.get<glm::uvec2>("windowSize"));
  prg->set1i("doA",(int)vars.addOrGetBool("doA",true));
  prg->set1i("doB",(int)vars.addOrGetBool("doB",true));
  prg->set1i("doC",(int)vars.addOrGetBool("doC",true));
  prg->set1i("clipSpace",(int)vars.addOrGetBool("clipSpace",true));

  ge::gl::glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  ge::gl::glDrawArrays(GL_TRIANGLES,0,3);
  ge::gl::glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  ge::gl::glDrawArrays(GL_TRIANGLES,3,6);
  ge::gl::glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  vao->unbind();
}

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv) {}
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
  createCamera(vars);
  window->setSize(1024,1024);
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
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawGrid(vars);
  drawLIH(vars);

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
