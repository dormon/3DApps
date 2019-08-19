#include <Simple3DApp/Application.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>

using namespace ge::gl;

class KillGPUProject: public simple3DApp::Application{
 public:
  KillGPUProject(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~KillGPUProject(){}
  vars::Vars vars;

  virtual void init() override;
  virtual void deinit() override;
  virtual void draw() override;
  std::shared_ptr<ge::gl::Program>prg;
};

void KillGPUProject::draw(){
  prg->use();
  prg->dispatch(1,1,1);
  swap();
}

void KillGPUProject::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  auto const kill = args->isPresent("--kill","you really want to kill your GPU");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  if(!kill)
    exit(0);

  std::string const csSrc = R".(
  #version 450 core

  layout(local_size_x=64)in;
  layout(binding=0)buffer Data{uint data[];};

  void main(){
    while(data[gl_GlobalInvocationID.x]!=1337)
      data[gl_GlobalInvocationID.x]+=10000;
  }
  ).";
  prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,csSrc));


}

void KillGPUProject::deinit(){
}

int main(int argc,char*argv[]){
  KillGPUProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
