#include <sstream>
#include <geGL/geGL.h>

using namespace ge::gl;
using namespace std;

shared_ptr<Shader>makeShader(GLuint type,size_t version,string const&s){
  stringstream ss;
  ss << "#version "<<version << std::endl;
  ss << s;
  
  return make_shared<Shader>(type,ss.str());
}

std::shared_ptr<ge::gl::Shader>makeVertexShader(size_t version,std::string const&s){
  return makeShader(GL_VERTEX_SHADER,version,s);
}

std::shared_ptr<ge::gl::Shader>makeFragmentShader(size_t version,std::string const&s){
  return makeShader(GL_FRAGMENT_SHADER,version,s);
}

std::shared_ptr<ge::gl::Shader>makeGeometryShader(size_t version,std::string const&s){
  return makeShader(GL_GEOMETRY_SHADER,version,s);
}

std::shared_ptr<ge::gl::Shader>makeControlShader(size_t version,std::string const&s){
  return makeShader(GL_TESS_CONTROL_SHADER,version,s);
}

std::shared_ptr<ge::gl::Shader>makeEvaluationShader(size_t version,std::string const&s){
  return makeShader(GL_TESS_EVALUATION_SHADER,version,s);
}

std::shared_ptr<ge::gl::Shader>makeComputeShader(size_t version,std::string const&s){
  return makeShader(GL_COMPUTE_SHADER,version,s);
}
