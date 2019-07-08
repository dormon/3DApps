#include <DrawGrid.h>
#include <Barrier.h>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>

void createGridProgram(vars::Vars&vars){
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
  void main(){
    vec3 start           = (inverse(view) * vec4(0,0,0,1)).xyz;
    vec3 pointOnFarPlane = (inverse(projection*view) * vec4(vCoord*far,far,far)).xyz;
    vec3 direction = normalize(pointOnFarPlane - start);
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

void drawGrid(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createGridProgram(vars);
  vars.get<ge::gl::Program>("gridProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->set1f("far",vars.getFloat("camera.far"))
    ->use();
  ge::gl::glDepthMask(GL_FALSE);
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  ge::gl::glDepthMask(GL_TRUE);
}

void drawGrid(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawGrid(vars,view->getView(),projection->getProjection());
}

