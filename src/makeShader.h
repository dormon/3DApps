#pragma once

#include <geGL/geGL.h>

std::shared_ptr<ge::gl::Shader>makeShader(GLuint type,size_t version,std::string const&s);

std::shared_ptr<ge::gl::Shader>makeVertexShader(size_t version,std::string const&s);
std::shared_ptr<ge::gl::Shader>makeFragmentShader(size_t version,std::string const&s);
std::shared_ptr<ge::gl::Shader>makeGeometryShader(size_t version,std::string const&s);
std::shared_ptr<ge::gl::Shader>makeControlShader(size_t version,std::string const&s);
std::shared_ptr<ge::gl::Shader>makeEvaluationShader(size_t version,std::string const&s);
std::shared_ptr<ge::gl::Shader>makeComputeShader(size_t version,std::string const&s);
