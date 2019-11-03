#include <fstream>
#include <sstream>
#include <filesystem>
#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <FreeImagePlus.h>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

constexpr int INPUT_NAME_SIZE = 128;

class FragmentPlay: public simple3DApp::Application{
 public:
  FragmentPlay(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~FragmentPlay(){}
  virtual void draw() override;

  vars::Vars vars;
  bool fullscreen = false;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
  virtual void                key(SDL_Event const& e, bool down) override;
};

void loadColorTexture(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"texture.update"}))return;
    if(!vars.getBool("texture.update")) return;

  std::ifstream infile(vars.getString("texture.file"));
  if(infile.fail())
  {
      vars.reCreate<bool>("texture.update", false);
      return;
  }

  fipImage colorImg;
  colorImg.load(vars.getString("texture.file").c_str());
  auto const width   = colorImg.getWidth();
  auto const height  = colorImg.getHeight();
  auto const BPP     = colorImg.getBitsPerPixel();
  auto const imgType = colorImg.getImageType();
  auto const data    = colorImg.accessPixels();

  std::cerr << "color BPP : " << BPP << std::endl;
  std::cerr << "color type: " << imgType << std::endl;

  GLenum format;
  GLenum type;
  if(imgType == FIT_BITMAP){
    std::cerr << "color imgType: FIT_BITMAP" << std::endl;
    if(BPP == 24)format = GL_BGR;
    if(BPP == 32)format = GL_BGRA;
    type = GL_UNSIGNED_BYTE;
  }
  if(imgType == FIT_RGBAF){
    std::cerr << "color imgType: FIT_RGBAF" << std::endl;
    if(BPP == 32*4)format = GL_RGBA;
    if(BPP == 32*3)format = GL_RGB;
    type = GL_FLOAT;
  }
  if(imgType == FIT_RGBA16){
    std::cerr << "color imgType: FIT_RGBA16" << std::endl;
    if(BPP == 48)format = GL_RGB ;
    if(BPP == 64)format = GL_RGBA;
    type = GL_UNSIGNED_SHORT;
  }
  if(imgType == FIT_FLOAT){
    format = GL_RED;
    type = GL_FLOAT;
  }
  if(imgType == FIT_RGBF){
    format = GL_RGB;
    type = GL_FLOAT;
  }

 
  auto texture = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D,GL_RGB8,1,width,height);
  ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1);
  ge::gl::glTextureSubImage2D(texture->getId(),0,0,0,width,height,format,type,data);
  auto map = vars.get<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures")->insert({vars.getUint32("texture.bindingPoint"), texture});
  vars.getString("texture.log") += std::to_string(vars.getUint32("texture.bindingPoint"))+" "+vars.getString("texture.file")+"\n";
  vars.reCreate<bool>("texture.update", false);
}


void loadTextures(vars::Vars&vars){
  loadColorTexture(vars);
}

void createProgram(vars::Vars&vars){
  if(!vars.getBool("shader.realtime"))
  {
      if(notChanged(vars,"all",__FUNCTION__,{"shader.update"}))return;
        if(!vars.getBool("shader.update")) return;
  }

  std::string const vsSrc = R".(
  #version 450 core

  out vec2 texCoords;

  void main(){
    texCoords = vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = vec4(texCoords*2-1,0,1);
  }
  ).";

  std::ifstream file;
  file.open(vars.getString("shader.file"));
  std::string fsSrc;
  
   if(file.fail()) 
      fsSrc = R".(
      #version 450 core
      layout(location=0)out vec4 fragColor; 
      void main()
      {
          fragColor = vec4(1.0f);
      }
      ).";
  else
  {
      std::stringstream stream;
      stream << file.rdbuf();
      fsSrc = stream.str();
  }

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("program",vs,fs);
  vars.reCreate<bool>("shader.update", false);
}


void drawFragmentPlay(vars::Vars&vars){
  loadTextures(vars);
   
    for(auto const& [point, texture] : *vars.get<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures"))
        texture->bind(point);
  
    auto program = vars.get<ge::gl::Program>("program");
    program->setNonexistingUniformWarning(false);
    for(auto const& variable : *vars.get<std::vector<std::pair<char[INPUT_NAME_SIZE], int>>>("inputInts"))
        program->set1i(variable.first, variable.second);
    for(auto const& variable : *vars.get<std::vector<std::pair<char[INPUT_NAME_SIZE], float>>>("inputFloats"))
        program->set1f(variable.first, variable.second);
    program->use();

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void FragmentPlay::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  //auto const shaderFile = args->gets("--quilt","","quilt image 5x9");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }
  vars.add<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures");
  vars.addVector<std::pair<char[INPUT_NAME_SIZE], int>>("inputInts");
  vars.addVector<std::pair<char[INPUT_NAME_SIZE], float>>("inputFloats");
  vars.addString("texture.log");
  vars.addString("project.file");
  vars.addUint32("texture.bindingPoint", 0);
  vars.addBool("texture.update", false);
  vars.addBool("shader.update", true);
  vars.addBool("shader.realtime", false);
  vars.add<std::filesystem::file_time_type>("lastModification");
  vars.addString("shader.file", std::string("none"));
  vars.addString("texture.file", std::string("none"));
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

  std::cerr << SDL_GetNumVideoDisplays() << std::endl;
  
  createProgram(vars);
}

void FragmentPlay::key(SDL_Event const& event, bool DOWN) {
  if(event.key.keysym.sym == SDLK_f && DOWN){
    fullscreen = !fullscreen;
    if(fullscreen)
      window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
    else
      window->setFullscreen(sdl2cpp::Window::WINDOW);
  }
}

template<class T>
void drawInputs(vars::Vars&vars)
{
  std::string vectorName = "inputInts";
  if constexpr (std::is_same_v<T,float>)
    vectorName = "inputFloats";
  
  auto inputs = vars.get<std::vector<std::pair<char[128], T>>>(vectorName);

  for(int i=0; i<inputs->size(); i++) 
  {    
      ImGui::Columns(2);
      ImGui::PushItemWidth(-1);
      ImGui::InputText(("###"+vectorName+"name"+std::to_string(i)).c_str(), inputs->at(i).first, IM_ARRAYSIZE(inputs->at(i).first)); 
      ImGui::PopItemWidth();
      ImGui::NextColumn();
      ImGui::PushItemWidth(-1);
      std::string label = "###"+vectorName+"value"+std::to_string(i);
      if constexpr (std::is_same_v<T,float>)
          ImGui::InputFloat(label.c_str(), &inputs->at(i).second);
      else if constexpr (std::is_same_v<T,int>)
          ImGui::InputInt(label.c_str(), &inputs->at(i).second);
      ImGui::PopItemWidth();
      ImGui::NextColumn();
  }
  ImGui::Columns(1);
  if(ImGui::Button("Add"))
  {  
    std::string name = std::string("uniformName") + std::to_string(inputs->size());
    inputs->push_back({});
    name.copy(inputs->back().first, name.size()+1);
    inputs->back().first[name.size()] = '\0'; 
  }
}

template<class T>
void loadInputs(std::ifstream &file, vars::Vars&vars)
{
  std::string vectorName = "inputInts";
  if constexpr (std::is_same_v<T,float>)
    vectorName = "inputFloats";

  auto inputs = vars.get<std::vector<std::pair<char[128], T>>>(vectorName);
  std::string token;
  inputs->clear();
  while(file >> token)
    if(token == "#") break;
    else
    {std::cerr << token;
       T value = static_cast<T>(std::stof(token));
       file >> token;
        inputs->push_back({});
        token.copy(inputs->back().first, token.size()+1);
        inputs->back().first[token.size()] = '\0'; 
        inputs->back().second = value; 
    }
}

void loadProject(vars::Vars&vars)
{  
  std::ifstream file(vars.getString("project.file"));
  if(file.fail())
    return; 

  std::string token;
  file >> token;
  vars.reCreate<std::string>("shader.file", token);
  vars.reCreate<bool>("shader.update", true);
  loadInputs<int>(file, vars);
  loadInputs<float>(file, vars);

  auto textures = vars.get<std::map<int, std::shared_ptr<ge::gl::Texture>>>("textures");
  textures->clear();
  vars.reCreate<std::string>("texture.log", "");
  while(file >> token)
  {
       int value = (std::stoi(token));
       file >> token;
    
        vars.reCreate<std::string>("texture.file", token);
        vars.reCreate<int>("texture.bindingPoint", value);
        vars.reCreate<bool>("texture.update", true);
        loadColorTexture(vars);
  } 
}

void saveProject(vars::Vars&vars)
{
    std::ofstream file(vars.getString("project.file"));
    file << vars.getString("shader.file") << std::endl;
    auto intInputs = vars.get<std::vector<std::pair<char[128], int>>>("inputInts");
    for(auto const &input : *intInputs)
     file << input.second << " " << input.first;
    file<< std::endl << "#" << std::endl;
    auto floatInputs = vars.get<std::vector<std::pair<char[128], float>>>("inputFloats");
    for(auto const &input : *floatInputs)
     file << input.second << " " << input.first;
    file<< std::endl << "#" << std::endl;
    file << vars.getString("texture.log");
    
}

void FragmentPlay::draw(){
  createProgram(vars);

  ge::gl::glClear(GL_DEPTH_BUFFER_BIT);

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawFragmentPlay(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);
  ImGui::Begin("vars");           
  
  if(ImGui::Button("Reload shader"))
    vars.reCreate<bool>("shader.update", true);
   
  if(ImGui::Button("Add texture"))
    vars.reCreate<bool>("texture.update", true);

  if(ImGui::Button("Load project"))
    loadProject(vars);
  
  if(ImGui::Button("Save project"))
    saveProject(vars);

  if(ImGui::CollapsingHeader("Input floats"))
    drawInputs<float>(vars);
   
  if(ImGui::CollapsingHeader("Input ints"))
    drawInputs<int>(vars);
 
  if(ImGui::CollapsingHeader("Bound textures"))
    ImGui::Text("%s", vars.getString("texture.log").c_str());    


  ImGui::End();

  swap();
}

void FragmentPlay::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
  FragmentPlay app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
