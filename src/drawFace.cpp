#include <drawBunny.h>
#include <Barrier.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <glm/gtc/type_ptr.hpp>
#include <BasicCamera/Camera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <bunny.h>
#include <faceDetect.h>


void createFaceProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  uniform mat4 projection;
  uniform mat4 view;
  layout(location=0)in vec3 position;
  layout(location=1)in vec3 normal;
  out vec3 vPosition;
  out vec3 vNormal;
  void main(){
    gl_Position = projection*view*vec4(position,1);
    vPosition = position;
    vNormal   = normal  ;
  }
  ).";


  std::string const fsSrc = R".(
  out vec4 fColor;
  in vec3 vPosition;
  in vec3 vNormal;
  uniform mat4 view     = mat4(1);
  uniform vec3 lightPos = vec3(0,0,10);
  uniform uvec2 size = uvec2(1024,512);

  void main(){
    vec3 cameraPos = vec3(inverse(view)*vec4(0,0,0,1));
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPos - vPosition);
    vec3 V = normalize(cameraPos - vPosition);
    vec3 R = -reflect(L,N);
    float df = max(dot(N,L),0);
    float sf = pow(max(dot(R,V),0)*float(df>0),100);
    fColor = vec4(df + sf);
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
  vars.reCreate<ge::gl::Program>("bunnyProgram",vs,fs);

}

void createFaceVBO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  vars.reCreate<ge::gl::Buffer>("bunnyVBO",sizeof(bunnyVertices),bunnyVertices);
  vars.reCreate<ge::gl::Buffer>("bunnyEBO",sizeof(bunnyIndices ),bunnyIndices );
}

void createBunnyVAO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;
  createFaceVBO(vars);
  auto vao = vars.reCreate<ge::gl::VertexArray>("bunnyVAO");
  auto vbo = vars.get<ge::gl::Buffer>("bunnyVBO");
  auto ebo = vars.get<ge::gl::Buffer>("bunnyEBO");
  vao->addAttrib(vbo,0,3,GL_FLOAT,sizeof(float)*6,0);
  vao->addAttrib(vbo,1,3,GL_FLOAT,sizeof(float)*6,sizeof(float)*3);
  vao->addElementBuffer(ebo);
}

void drawFace(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createFaceProgram(vars);
  createBunnyVAO(vars);
  ge::gl::glEnable(GL_DEPTH_TEST);
  auto vao = vars.get<ge::gl::VertexArray>("bunnyVAO");
  vao->bind();
  vars.get<ge::gl::Program>("bunnyProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->use();
  ge::gl::glDrawElements(GL_TRIANGLES,sizeof(bunnyIndices)/sizeof(uint32_t),GL_UNSIGNED_INT,0);
  vao->unbind();
}

void drawFace(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawBunny(vars,view->getView(),projection->getProjection());
}

