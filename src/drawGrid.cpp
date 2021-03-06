#include <drawGrid.h>
#include <Barrier.h>
#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <makeShader.h>

void createGridProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  auto const vsSrc = R".(
  out vec2 vCoord;
  void main(){
    vCoord = vec2(-1+2*(gl_VertexID&1),-1+2*(gl_VertexID>>1));
    gl_Position = vec4(vCoord,0,1);
  }
  ).";
  auto const fsSrc = R".(
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
    vec2 saw = abs(mod(gridPosition.xz,1)*2-1);

    float v = pow(saw.x*saw.y,.1);
    if(t < 0 || length(gridPosition) > 30){
      fColor = vec4(0,0,0,1);
      return;
    }
    fColor = vec4(1-v) * (1-max(length(gridPosition)-10,0)/20.f);
  }
  ).";

  auto vs = makeVertexShader(330,vsSrc);
  auto fs = makeFragmentShader(330,fsSrc);
  vars.reCreate<ge::gl::Program>("gridProgram",vs,fs);

}

void createGridVAO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  vars.reCreate<ge::gl::VertexArray>("gridVAO");
}

void drawGrid(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createGridProgram(vars);
  createGridVAO(vars);
  vars.get<ge::gl::VertexArray>("gridVAO")->bind();
  vars.get<ge::gl::Program>("gridProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->set1f("far",vars.getFloat("camera.far"))
    ->use();
  ge::gl::glDepthMask(GL_FALSE);
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  ge::gl::glDepthMask(GL_TRUE);
  vars.get<ge::gl::VertexArray>("gridVAO")->unbind();
}

void drawGrid(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawGrid(vars,view->getView(),projection->getProjection());
}

