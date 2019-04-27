#include <Simple3DApp/Application.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>

//glClearColor
//glClear
//
//glCreateShader
//glShaderSource
//glCompileShader
//glGetShaderiv
//glGetShaderInfoLog
//glDeleteShader
//
//glCreateProgram
//glAttachShader
//glLinkProgram
//glGetProgramiv
//glGetProgramInfoLog
//glDeleteProgram
//glUseProgram
//
//glCreateVertexArrays
//glDeleteVertexArrays
//glBindVertexArray
//
//glDrawArrays
//

using namespace ge::gl;

class TriangleProject: public simple3DApp::Application{
 public:
  TriangleProject(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~TriangleProject(){}
  virtual void draw() override;

  GLuint vertexShaderId;
  GLuint fragmentShaderId;
  GLuint programId;

  GLuint vertexArray;

  void initShaderProgram();
  void initVertexArray();

  void deinitShaderProgram();
  void deinitVertexArray();

  GLuint createVertexShader();
  GLuint createFragmentShader();

  virtual void init() override;
  virtual void deinit() override;
};

void TriangleProject::init(){
  initShaderProgram();
  initVertexArray();
}

void TriangleProject::draw(){
  glClearColor(0.1f,0.1f,0.1f,1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(programId);
  glBindVertexArray(vertexArray);
  glDrawArrays(GL_TRIANGLES,0,3);

  swap();
}

void TriangleProject::deinit(){
  deinitShaderProgram();
  deinitVertexArray();
}

GLuint createProgram(GLuint s0,GLuint s1);

void TriangleProject::initShaderProgram(){
  vertexShaderId   = createVertexShader();
  fragmentShaderId = createFragmentShader();
  programId        = createProgram(vertexShaderId,fragmentShaderId);
}

void TriangleProject::initVertexArray(){
  glCreateVertexArrays(1,&vertexArray);
}

GLuint createShader(GLenum type,GLchar const*src);

GLuint TriangleProject::createVertexShader(){
  GLchar const* vertexShaderSource = R".(
  #version 150
  void main(){
    if(gl_VertexID==0)gl_Position = vec4(0,0,0,1);
    if(gl_VertexID==1)gl_Position = vec4(1,0,0,1);
    if(gl_VertexID==2)gl_Position = vec4(0,1,0,1);
  }
  ).";
  return createShader(GL_VERTEX_SHADER,vertexShaderSource);
}

GLuint TriangleProject::createFragmentShader(){
  GLchar const* fragmentShaderSource = R".(
  #version 150
  out vec4 fColor;
  void main(){
    fColor = vec4(1);
  }
  ).";
  return createShader(GL_FRAGMENT_SHADER,fragmentShaderSource);
}

void TriangleProject::deinitShaderProgram(){
  glDeleteProgram(programId);
  glDeleteShader(vertexShaderId);
  glDeleteShader(fragmentShaderId);
}

void TriangleProject::deinitVertexArray(){
  glDeleteVertexArrays(1,&vertexArray);
}

using Getiv      = void(*)(GLuint,GLenum,GLint*);
using GetInfoLog = void(*)(GLuint,GLsizei,GLsizei*,GLchar*);

void printInfoLog(Getiv getiv,GetInfoLog getInfoLog,GLuint id){
  GLint infoLen;
  getiv(id,GL_INFO_LOG_LENGTH,&infoLen);
  if(infoLen==0)return;
  char*info = new char[infoLen];
  getInfoLog(id,infoLen,nullptr,info);
  std::cerr << info << std::endl;
  delete[]info;
}

void printShaderInfoLog(GLuint id){
  printInfoLog(glGetShaderiv,glGetShaderInfoLog,id);
}

void printShaderProgramInfoLog(GLuint id){
  printInfoLog(glGetProgramiv,glGetProgramInfoLog,id);
}

GLuint createShader(GLenum type,GLchar const*src){
  GLuint id = glCreateShader(type);
  GLchar const* sources[] = {
    src
  };
  glShaderSource(id,1,sources,nullptr);
  glCompileShader(id);
  GLint compileStatus;
  glGetShaderiv(id,GL_COMPILE_STATUS,&compileStatus);
  printShaderInfoLog(id);
  return id;
}

GLuint createProgram(GLuint s0,GLuint s1){
  GLuint id = glCreateProgram();
  glAttachShader(id,s0);
  glAttachShader(id,s1);
  glLinkProgram(id);
  printShaderProgramInfoLog(id);
  return id;
}

int main(int argc,char*argv[]){
  TriangleProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
