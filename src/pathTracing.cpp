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
#include <Barrier.h>

class PathTracing: public simple3DApp::Application{
 public:
  PathTracing(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~PathTracing(){}
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

void PathTracing::init(){
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

void createPathTracingProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  out vec2 vCoord;
  void main(){
    vCoord = vec2(-1+2*(gl_VertexID&1),-1+2*(gl_VertexID>>1));
    gl_Position = vec4(vCoord,0,1);
  }
  ).";
  std::string const fsSrc = R".(
  uniform mat4 projection;
  uniform mat4 view;
  uniform float far;
  out vec4 fColor;
  in vec2 vCoord;
  struct Ray{
    vec3 start;
    vec3 direction;
  }

  struct Triangle{
    vec3 vertex0;
    vec3 vertex1;
    vec3 vertex2;
  };

  bool RayIntersectsTriangle(in vec3     rayOrigin           ,
                             in vec3     rayVector           ,
                             in Triangle inTriangle          ,
                             out vec3    outIntersectionPoint)
  {
      const float EPSILON = 0.0000001;
      vec3 vertex0 = inTriangle.vertex0;
      vec3 vertex1 = inTriangle.vertex1;
      vec3 vertex2 = inTriangle.vertex2;
      vec3 edge1, edge2, h, s, q;
      float a,f,u,v;
      edge1 = vertex1 - vertex0;
      edge2 = vertex2 - vertex0;
      h = rayVector.crossProduct(edge2);
      a = edge1.dotProduct(h);
      if (a > -EPSILON && a < EPSILON)
          return false;    // This ray is parallel to this triangle.
      f = 1.0/a;
      s = rayOrigin - vertex0;
      u = f * s.dotProduct(h);
      if (u < 0.0 || u > 1.0)
          return false;
      q = s.crossProduct(edge1);
      v = f * rayVector.dotProduct(q);
      if (v < 0.0 || u + v > 1.0)
          return false;
      // At this stage we can compute t to find out where the intersection point is on the line.
      float t = f * edge2.dotProduct(q);
      if (t > EPSILON) // ray intersection
      {
          outIntersectionPoint = rayOrigin + rayVector * t;
          return true;
      }
      else // This means that there is a line intersection but not a ray intersection.
          return false;
  }

  void main(){
    Ray ray;
    ray.start            = (inverse(view) * vec4(0,0,0,1)).xyz;
    vec3 pointOnFarPlane = (inverse(projection*view) * vec4(vCoord*far,far,far)).xyz;
    ray.direction        = normalize(pointOnFarPlane - start);

    float t = (-3-start.y)/direction.y;
    vec3 gridPosition = start + direction*t;
    float v = pow((1-cos(mod(gridPosition.x,1.f)*3.141592*2)) * (1-cos(mod(gridPosition.z,1.f)*3.141592*2)),.1);
    if(t < 0 || length(gridPosition) > 30){
      fColor = vec4(0,0,0,1);
      return;
    }
    fColor = vec4(1-v) * (1-max(length(gridPosition)-10,0)/20.f);
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
  vars.reCreate<ge::gl::Program>("gridProgram",vs,fs);


}

void drawPathTracing(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createPathTracingProgram(vars);
  vars.get<ge::gl::Program>("gridProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->set1f("far",vars.getFloat("camera.far"))
    ->use();
  ge::gl::glDepthMask(GL_FALSE);
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  ge::gl::glDepthMask(GL_TRUE);
}

void drawPathTracing(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawGrid(vars,view->getView(),projection->getProjection());
}


void PathTracing::draw(){
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
  //drawGrid(vars);
  drawPathTracing(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void PathTracing::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
}

void PathTracing::mouseMove(SDL_Event const& e) {
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

void PathTracing::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  PathTracing app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
