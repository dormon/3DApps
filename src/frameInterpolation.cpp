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
#include <imguiVars.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <FreeImagePlus.h>
#include <addVarsLimits.h>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

constexpr int INPUT_NAME_SIZE = 128;

class FrameInterpolation: public simple3DApp::Application{
    public:
        FrameInterpolation(int argc, char* argv[]) : Application(argc, argv) {}
        virtual ~FrameInterpolation(){}
        virtual void draw() override;

        vars::Vars vars;
        bool fullscreen = false;

        virtual void                init() override;
        virtual void                resize(uint32_t x,uint32_t y) override;
        virtual void                key(SDL_Event const& e, bool down) override;
};

void loadColorTexture(vars::Vars&vars){ 
    auto textureFiles = vars.getVector<std::string>("textureFiles");
    for(int i=0; i<textureFiles.size(); i++)
    {
        std::ifstream infile(textureFiles[i]);
        std::cerr << textureFiles[i];
        if(infile.fail())
            throw std::runtime_error("Cannot load texture!");
        fipImage colorImg;
        colorImg.load(textureFiles[i].c_str());
        auto const width   = colorImg.getWidth();
        auto const height  = colorImg.getHeight();
        auto const BPP     = colorImg.getBitsPerPixel();
        auto const imgType = colorImg.getImageType();
        auto const data    = colorImg.accessPixels();

        std::cerr << "color BPP : " << BPP << std::endl;
        std::cerr << "color type: " << imgType << std::endl;

        GLenum format;
        GLenum type;
        GLenum internatFormat = GL_RGB8;
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
            internatFormat = GL_RGBA32F;
        }
        if(imgType == FIT_RGBA16){
            std::cerr << "color imgType: FIT_RGBA16" << std::endl;
            if(BPP == 48)format = GL_RGB ;
            if(BPP == 64)format = GL_RGBA;
            type = GL_UNSIGNED_SHORT;
            internatFormat = GL_RGB16;
        }
        if(imgType == FIT_FLOAT){
            std::cerr << "color imgType: FIT_FLOAT" << std::endl;
            format = GL_RED;
            type = GL_FLOAT;
            internatFormat = GL_R32F;
        }
        if(imgType == FIT_RGBF){
            std::cerr << "color imgType: FIT_RGBF" << std::endl;
            format = GL_RGB;
            type = GL_FLOAT;
            internatFormat = GL_RGB32F;
        }     
        auto texture = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D,internatFormat,1,width,height);
        ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
        ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1);
        ge::gl::glTextureSubImage2D(texture->getId(),0,0,0,width,height,format,type,data);
        vars.getVector<std::shared_ptr<ge::gl::Texture>>("textures").push_back(texture);
    }
}


void loadTextures(vars::Vars&vars){
    loadColorTexture(vars);
}

void createProgram(vars::Vars&vars){
    if(notChanged(vars,"all",__FUNCTION__,{}))return;

    std::string const csSrc = R".(
  #version 450 core
  layout(local_size_x=64)in;
  layout(binding=0)uniform sampler2D image1;
  layout(binding=1)uniform sampler2D image2;
  layout(binding=2)uniform sampler2D image1Depth;
  layout(binding=3)uniform sampler2D image2Depth;

  uniform float near = 0.1f;
  uniform float far  = 25.f;
  uniform float fovy = 3.141592f/2.f;
  uniform float baseline = 0.5;
  uniform float step = 0.5;

  layout(std430, binding=2) buffer pixelLayout
  {
        vec4 pixels[];
  };

  void main(){ 
    ivec2 size = textureSize(image1,0);

    if(gl_GlobalInvocationID.x > size.x*size.y)
        return;

    float aspect = float(size.x)/float(size.y);
    float T = near*tan(fovy/2);
    float B = -T;
    float R = T*aspect;
    float L = -R;

    ivec2 coord;
    coord.x = int(gl_GlobalInvocationID.x)%size.x;
    coord.y = int(gl_GlobalInvocationID.x)/size.x;
    pixels[gl_GlobalInvocationID.x] = texelFetch(image1,coord,0);
    float depth = texelFetch(image1Depth,coord,0).x;

    vec3 point = normalize(vec3( L+(float(coord.x)/size.x)*(R-L), B+(float(coord.y)/size.y)*(T-B),-near))*depth;

    vec3 interCam = vec3(baseline, 0.0, 0.0);
    vec3 camToPoint = point-interCam;
    vec3 interPoint = (-near/camToPoint.z) * camToPoint;
    coord.x = int((interPoint.x-L)/(R-L)*size.x);
    coord.y = int((interPoint.y-B)/(T-B)*size.y);

    if(depth < 10000.0f)
    {
      pixels[gl_GlobalInvocationID.x] /= 2.0;
      pixels[gl_GlobalInvocationID.x] += texelFetch(image2, coord, 0);
    }
    }
  ).";

    std::string const vsSrc = R".(
  #version 450 core

  out int vertexId;
  layout(binding=0)uniform sampler2D image1;
  
  void main(){
    ivec2 size   = textureSize(image1,0);

    ivec2 coord;
    coord.x = gl_VertexID%size.x;
    coord.y = gl_VertexID/size.x;
    vec2 ndc = vec2(coord)/vec2(size)*2-1;

    gl_Position = vec4(ndc,1,1);
    vertexId = gl_VertexID;
  }
  ).";

    std::string const fsSrc = R".(
      #version 450 core
      layout(location=0)out vec4 fragColor;
      flat in int vertexId;

      layout(std430, binding=2) buffer pixelLayout
      {
            vec4 pixels[];
      };

      void main()
      {
          fragColor = pixels[vertexId];
      }
      ).";

    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
            vsSrc
            );
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
            fsSrc
            );
    auto cs = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,
            csSrc
            );
    vars.reCreate<ge::gl::Program>("program",vs,fs);
    vars.reCreate<ge::gl::Program>("computeProgram",cs);
}

void drawFrameInterpolation(vars::Vars&vars){ 
    int i=0; 
    for(auto const &texture : vars.getVector<std::shared_ptr<ge::gl::Texture>>("textures")) 
    {
        texture->bind(i);
        i++;
    }

    auto computeProgram = vars.get<ge::gl::Program>("computeProgram");
    computeProgram->setNonexistingUniformWarning(false);
    computeProgram->set1f("baseline", vars.getFloat("baseline"));
    computeProgram->set1f("step", vars.getFloat("step"));
    computeProgram->use();
    ge::gl::glDispatchCompute(ceil(vars.getInt32("textureSize")/(64.0f*3)),1,1);
    ge::gl::glFinish();

    auto program = vars.get<ge::gl::Program>("program");
    program->setNonexistingUniformWarning(false);
    program->use();

    ge::gl::glPointSize(2);
    ge::gl::glDrawArrays(GL_POINTS,0,vars.getInt32("textureSize"));
}

void FrameInterpolation::init(){
    auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
    //auto const shaderFile = args->gets("--quilt","","quilt image 5x9");
    auto const showHelp = args->isPresent("-h","shows help");
    if (showHelp || !args->validate()) {
        std::cerr << args->toStr();
        exit(0);
    }
    vars.add<ge::gl::VertexArray>("emptyVao");
    vars.addVector<std::shared_ptr<ge::gl::Texture>>("textures");
    auto &textureFiles = vars.addVector<std::string>("textureFiles");
    textureFiles.push_back("../data/0001.png");
    textureFiles.push_back("../data/0002.png");
    textureFiles.push_back("../data/0001.exr");
    textureFiles.push_back("../data/0002.exr");
    /*textureFiles.push_back("/home/ichlubna/Workspace/lf/render/3DApps/data/0001.png");
      textureFiles.push_back("/home/ichlubna/Workspace/lf/render/3DApps/data/0002.png");
      textureFiles.push_back("/home/ichlubna/Workspace/lf/render/3DApps/data/0001.exr");
      textureFiles.push_back("/home/ichlubna/Workspace/lf/render/3DApps/data/0002.exr");*/
    vars.addFloat("baseline");
    vars.addFloat("step");
    addVarsLimitsF(vars,"baseline",-10,10,0.01f);
    addVarsLimitsF(vars,"step",0.0f,1.0f,0.01f);
    vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

    std::cerr << SDL_GetNumVideoDisplays() << std::endl;
    createProgram(vars);
    loadTextures(vars); 
    vars.addInt32("textureSize", vars.getVector<std::shared_ptr<ge::gl::Texture>>("textures")[0]->getSize());

    auto result = vars.add<ge::gl::Buffer>("result",ceil(vars.getInt32("textureSize")/(3.0f)*16));
    result->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
}

void FrameInterpolation::key(SDL_Event const& event, bool DOWN) {
    if(event.key.keysym.sym == SDLK_f && DOWN){
        fullscreen = !fullscreen;
        if(fullscreen)
            window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
        else
            window->setFullscreen(sdl2cpp::Window::WINDOW);
    }
}

void FrameInterpolation::draw(){
    createProgram(vars);

    ge::gl::glClear(GL_DEPTH_BUFFER_BIT);

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT);

    vars.get<ge::gl::VertexArray>("emptyVao")->bind();

    drawFrameInterpolation(vars);

    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

    drawImguiVars(vars);
    ImGui::Begin("vars");           

    ImGui::End();

    swap();
}

void FrameInterpolation::resize(uint32_t x,uint32_t y){
    auto windowSize = vars.get<glm::uvec2>("windowSize");
    windowSize->x = x;
    windowSize->y = y;
    vars.updateTicks("windowSize");
    ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
    FrameInterpolation app{argc, argv};
    app.start();
    return EXIT_SUCCESS;
}
