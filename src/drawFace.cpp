#include <Barrier.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <glm/gtc/type_ptr.hpp>
#include <BasicCamera/Camera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <bunny.h>
#include <faceDetect.h>

#include <k4a/k4a.hpp>

FaceDetector detector("/usr/share/opencv-cuda/haarcascades/haarcascade_frontalface_default.xml");

void initFace(vars::Vars&vars)
{
  const uint32_t deviceCount = k4a::device::get_installed_count();
  if (deviceCount == 0)
    throw std::runtime_error("No Azure Kinect connected");

  k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
  config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
  config.color_resolution = K4A_COLOR_RESOLUTION_720P;
  config.camera_fps = K4A_FRAMES_PER_SECOND_30;
  auto dev = vars.reCreate<k4a::device>("kinectDevice");
  *dev = k4a::device::open(K4A_DEVICE_DEFAULT);
  dev->start_cameras(&config); 
}

glm::vec3 getCoords(vars::Vars&vars)
{ 
    k4a::capture capture;
    auto dev = vars.get<k4a::device>("kinectDevice");
    if (dev->get_capture(&capture, std::chrono::milliseconds(0)))
    {
        const k4a::image colorImage = capture.get_color_image();
        const uint8_t* buffer = colorImage.get_buffer();
        int rows = colorImage.get_height_pixels();
        int cols = colorImage.get_width_pixels();
        cv::Mat colorMat(rows , cols, CV_8UC4, (void*)buffer, cv::Mat::AUTO_STEP);
        return detector.FaceDetector::getFaceCoords(colorMat, 1.0);
    }
    return glm::vec3();
}


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
  vars.reCreate<ge::gl::Program>("faceProgram",vs,fs);

}

void createFaceVBO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  vars.reCreate<ge::gl::Buffer>("faceVBO",sizeof(bunnyVertices),bunnyVertices);
  vars.reCreate<ge::gl::Buffer>("faceEBO",sizeof(bunnyIndices ),bunnyIndices );
}

void createFaceVAO(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;
  createFaceVBO(vars);
  auto vao = vars.reCreate<ge::gl::VertexArray>("faceVAO");
  auto vbo = vars.get<ge::gl::Buffer>("faceVBO");
  auto ebo = vars.get<ge::gl::Buffer>("faceEBO");
  vao->addAttrib(vbo,0,3,GL_FLOAT,sizeof(float)*6,0);
  vao->addAttrib(vbo,1,3,GL_FLOAT,sizeof(float)*6,sizeof(float)*3);
  vao->addElementBuffer(ebo);
}

void updateFace(vars::Vars&vars)
{
    vars.reCreate<glm::mat4>("rotationMat" ,glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0*(-0.5+getCoords(vars).y), 0.0f)));
}

void drawFace(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createFaceProgram(vars);
  createFaceVAO(vars);
  ge::gl::glEnable(GL_DEPTH_TEST);
  auto vao = vars.get<ge::gl::VertexArray>("faceVAO");
  vao->bind();
  vars.get<ge::gl::Program>("faceProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view**vars.get<glm::mat4>("rotationMat")))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->use();
  ge::gl::glDrawElements(GL_TRIANGLES,sizeof(bunnyIndices)/sizeof(uint32_t),GL_UNSIGNED_INT,0);
  vao->unbind();
}

void drawFace(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawFace(vars,view->getView(),projection->getProjection());
}

