#include <ArgumentViewer/ArgumentViewer.h>
#include<Simple3DApp/Application.h>
#include<Vars/Vars.h>
#include<geGL/StaticCalls.h>
#include<geGL/geGL.h>
#include<BasicCamera/FreeLookCamera.h>
#include<BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include<Barrier.h>
#include<imguiSDL2OpenGL/imgui.h>
#include<imguiVars/imguiVars.h>
#include<drawGrid.h>
/*#include <assimp/Importer.hpp>
#include <assimp/scene.h>*/
#include <SDL2CPP/Exception.h>
#include <Timer.h>
#include <gpuDecoder.h>
#include<string>
#include<thread>
#include<mutex>
#include<condition_variable>

constexpr float FRAME_LIMIT = 1.0f/24;
constexpr bool SCREENSHOT_MODE = 0;
constexpr bool SCREENSHOT_VIDEO = 0;

class LightFields: public simple3DApp::Application
{
public:
    LightFields(int argc, char* argv[]) : Application(argc, argv) {}
    virtual ~LightFields() {}
    virtual void draw() override;

    vars::Vars vars;

    virtual void                init() override;
    //void                        parseArguments();
    virtual void                mouseMove(SDL_Event const& event) override;
    virtual void                key(SDL_Event const& e, bool down) override;
    virtual void                resize(uint32_t x,uint32_t y) override;
};

void createProgram(vars::Vars&vars)
{
    std::string const vsSrc = R".(
    uniform mat4 mvp;
    uniform float aspect = 1.f;
    out vec2 vCoord;
    out vec3 position;

    void main()
    {
        vCoord = vec2(gl_VertexID&1,gl_VertexID>>1);
        position = vec3((-1+2*vCoord)*vec2(aspect,1),0);
        gl_Position = mvp*vec4(position,1);
    }
    ).";

    //#################################################################

    std::string const fsSrc = R".(
    #extension GL_ARB_gpu_shader_int64 : enable
    #extension GL_ARB_bindless_texture : require
    uniform uint64_t lightfield;
    out vec4 fColor;
    in vec2 vCoord;
    in vec3 position;
    
    void main()
    {
        sampler2D s = sampler2D(lightfield);
        fColor = texture(s, vCoord*textureSize(s,0));
    }
    ).";
    
    //#################################################################

    std::string const csSrc = R".(  
    #extension GL_ARB_gpu_shader_int64 : enable
    #extension GL_ARB_bindless_texture : require
    #define LF_SIZE 64
    layout(local_size_x=8, local_size_y=8)in;
    uniform uint64_t lfTextures[LF_SIZE];
    layout(bindless_image)uniform layout(rgba8) writeonly image2D lightfield;
    void main()
    {
        sampler2D s = sampler2D(lfTextures[5]);
        vec4 color = texelFetch(s, ivec2(gl_GlobalInvocationID.xy),0);
        imageStore(lightfield, ivec2(gl_GlobalInvocationID.xy), color);
    }
    ).";

    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", vsSrc);
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", fsSrc);
    auto cs = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER, "#version 450\n", csSrc);

    auto program = vars.reCreate<ge::gl::Program>("lfProgram",vs,fs);
    if(program->getLinkStatus() == GL_FALSE)
        throw std::runtime_error("Cannot link shader program.");
    
    auto csProgram = vars.reCreate<ge::gl::Program>("csProgram",cs);
    if(csProgram->getLinkStatus() == GL_FALSE)
        throw std::runtime_error("Cannot link shader program.");
}

void createView(vars::Vars&vars)
{
    if(notChanged(vars,"all",__FUNCTION__, {}))return;
    vars.add<basicCamera::OrbitCamera>("view");
}

void createProjection(vars::Vars&vars)
{
    if(notChanged(vars,"all",__FUNCTION__, {"windowSize","camera.fovy","camera.near","camera.far"}))return;

    auto windowSize = vars.get<glm::uvec2>("windowSize");
    auto width = windowSize->x;
    auto height = windowSize->y;
    auto aspect = (float)width/(float)height;
    auto near = vars.getFloat("camera.near");
    auto far  = vars.getFloat("camera.far" );
    auto fovy = vars.getFloat("camera.fovy");

    vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,near,far);
}

void createCamera(vars::Vars&vars)
{
    createProjection(vars);
    createView(vars);
}

void asyncVideoLoading(vars::Vars &vars)
{
    SDL_GLContext c = SDL_GL_CreateContext(*vars.get<SDL_Window*>("mainWindow")); 
    SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),c);
    ge::gl::init(SDL_GL_GetProcAddress);
    ge::gl::setHighDebugMessage();

    auto decoder = std::make_unique<GpuDecoder>(vars.getString("videoFile").c_str());
    //TODO get info from mkv about cams
    vars.reCreate<unsigned int>("lf.width",decoder->getWidth());
    vars.reCreate<unsigned int>("lf.height", decoder->getHeight());
    vars.reCreate<float>("texture.aspect",decoder->getAspect());
    auto length = vars.addUint32("length",static_cast<int>(decoder->getLength()/64000000.0*25));
    
    //Must be here since all textures are created in this context and handles might overlap
    auto lightfieldTexture = ge::gl::Texture(GL_TEXTURE_2D,GL_RGBA8,1,vars.getUint32("lf.width"),vars.getUint32("lf.height"));
    auto lightfield = vars.add<GLuint64>("lightfield",ge::gl::glGetImageHandleARB(lightfieldTexture.getId(), 0, GL_FALSE, 0, GL_RGBA8));
    //vars.add<ge::gl::Texture>("disparity",GL_TEXTURE_2D,GL_R32,1,vars.getUint32("lf.width"),vars.getUint32("lf.height"));
  
    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    auto mainRuns = vars.get<bool>("mainRuns");
    int frameNum = 1;   
int a = 0; 
    while(mainRuns)
    {    
        int seekFrame = *vars.get<int>("seekFrame");
        if(seekFrame > -1)
        {
            decoder->seek(seekFrame*64);
            frameNum = seekFrame; 
            vars.reCreate<int>("seekFrame", -1);
        }

        if(frameNum > length)
            frameNum = 1;
        if(!vars.getBool("pause"))
        {
            int index = decoder->getActiveBufferIndex();
            std::string nextTexturesName = "lfTextures" + std::to_string(index);
            //TODO is this copy deep?
            vars.reCreateVector<GLuint64>(nextTexturesName, decoder->getFrames(64));
            GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);
        
            vars.getUint32("lfTexturesIndex") = index;

            //TODO without waiting? async draw to increase responsitivity
            frameNum++;
            vars.reCreate<int>("frameNum", frameNum);
        
            //vars.getBool("pause") = true;
        }
        //TODO sleep according to FPS
        vars.getBool("loaded") = true;
        rdyCv->notify_all();   
    }
}

void LightFields::init()
{
    auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
    auto const videoFile = args->gets("--video","","h265 compressed LF video (pix_fmt yuv420p)");
    auto const gridSize = args->getu32("--grid",8,"number of cameras in row/column (assuming a rectangle)");
    auto const showHelp = args->isPresent("-h","shows help");
    if (showHelp || !args->validate()) {
        std::cerr << args->toStr();
        exit(0);
    }
    vars.addString("videoFile", videoFile);
    vars.add<glm::uvec2>("gridSize",glm::uvec2(gridSize));

    window->createContext("loading", 450u, sdl2cpp::Window::CORE, sdl2cpp::Window::DEBUG);
    vars.add<SDL_GLContext>("loadingContext", window->getContext("loading"));
    vars.add<SDL_GLContext>("renderingContext", window->getContext("rendering"));
    vars.add<SDL_Window*>("mainWindow", window->getWindow());
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);  
    
    SDL_SetWindowSize(window->getWindow(),1920,1080);
    vars.addBool("mainRuns", true);
    vars.addBool("pause", false);
    vars.add<int>("seekFrame", -1);
    vars.addUint32("lfTexturesIndex", 0);
    auto rdyMutex = vars.add<std::mutex>("rdyMutex");
    auto rdyCv = vars.add<std::condition_variable>("rdyCv");
    vars.addBool("loaded", false);
    vars.add<std::thread>("loadingThread",asyncVideoLoading, std::ref(vars));
    {
        std::unique_lock<std::mutex> lck(*rdyMutex);
        rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
    }
    ge::gl::glMakeImageHandleResidentARB(*vars.get<GLuint64>("lightfield"), GL_READ_WRITE);

    SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),window->getContext("rendering"));

    vars.add<ge::gl::VertexArray>("emptyVao");
    vars.addFloat("input.sensitivity",0.01f);
    vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
    vars.addFloat("camera.fovy",glm::half_pi<float>());
    vars.addFloat("camera.near",.1f);
    vars.addFloat("camera.far",1000.f);
    vars.addFloat("focus",0.0f);
    vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
    vars.addUint32("frame",0);
    createProgram(vars);
    createCamera(vars);

    ge::gl::glEnable(GL_DEPTH_TEST);
}

SDL_Surface* flipSurface(SDL_Surface* sfc) {
     SDL_Surface* result = SDL_CreateRGBSurface(sfc->flags, sfc->w, sfc->h,
         sfc->format->BytesPerPixel * 8, sfc->format->Rmask, sfc->format->Gmask,
         sfc->format->Bmask, sfc->format->Amask);
     const auto pitch = sfc->pitch;
     const auto pxlength = pitch*(sfc->h - 1);
     auto pixels = static_cast<unsigned char*>(sfc->pixels) + pxlength;
     auto rpixels = static_cast<unsigned char*>(result->pixels) ;
     for(auto line = 0; line < sfc->h; ++line) {
         memcpy(rpixels,pixels,pitch);
         pixels -= pitch;
         rpixels += pitch;
     }
     return result;
}

void screenShot(std::string filename, int w, int h)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rmask = 0xff000000;
    Uint32 gmask = 0x00ff0000;
    Uint32 bmask = 0x0000ff00;
    Uint32 amask = 0x000000ff;  
#else
    Uint32 rmask = 0x000000ff;
    Uint32 gmask = 0x0000ff00;
    Uint32 bmask = 0x00ff0000;
    Uint32 amask = 0xff000000;
#endif
    SDL_Surface *ss = SDL_CreateRGBSurface(0, w, h, 24, rmask, gmask, bmask, amask);
    ge::gl::glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, ss->pixels);
    SDL_Surface *s = flipSurface(ss);
    SDL_SaveBMP(s, filename.c_str());
    SDL_FreeSurface(s); 
    SDL_FreeSurface(ss); 
}

void LightFields::draw()
{
    auto timer = vars.addOrGet<Timer<double>>("timer");
    timer->reset();
    
    std::string currentTexturesName = "lfTextures" + std::to_string(vars.getUint32("lfTexturesIndex"));

    //TODO just once for both buffers    
    std::vector<GLuint64> t = vars.getVector<GLuint64>(currentTexturesName);
    for(auto a : t)
    	ge::gl::glMakeTextureHandleResidentARB(a);

    createCamera(vars);
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

    drawGrid(vars);
 
    auto csProgram = vars.get<ge::gl::Program>("csProgram");
    csProgram
    //->set2uiv("gridSize",glm::value_ptr(*vars.get<glm::uvec2>("gridSize")))
    //->set1i("focus",vars.getFloat("focus"))
    ->use();
    ge::gl::glProgramUniformHandleui64vARB(csProgram->getId(), csProgram->getUniformLocation("lfTextures"), 64, vars.getVector<GLuint64>(currentTexturesName).data());
    ge::gl::glProgramUniformHandleui64vARB(csProgram->getId(), csProgram->getUniformLocation("lightfield"), 1, vars.get<GLuint64>("lightfield"));
    //TODO not multiple of 8 resolution cases
    constexpr int LOCAL_SIZE_X = 8; 
    constexpr int LOCAL_SIZE_Y = 8; 
    ge::gl::glDispatchCompute(*vars.get<unsigned int>("lf.width")/LOCAL_SIZE_X,*vars.get<unsigned int>("lf.height")/LOCAL_SIZE_Y,1);    
    ge::gl::glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    ge::gl::glFinish();

    auto program = vars.get<ge::gl::Program>("lfProgram");
    program
    ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
    ->set1f("aspect",vars.getFloat("texture.aspect"))
    ->use();
    ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lfTextures"), 64, vars.getVector<GLuint64>(currentTexturesName).data());
    ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lightfield"), 1, vars.get<GLuint64>("lightfield"));
    ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
 
    drawImguiVars(vars);
    ImGui::Begin("Playback");
    if(ImGui::SliderInt("Timeline", vars.get<int>("frameNum"), 1, vars.getUint32("length")))
        vars.reCreate<int>("seekFrame", *vars.get<int>("frameNum"));
    ImGui::Selectable("Pause", &vars.getBool("pause"));
    ImGui::DragFloat("Focus", &vars.getFloat("focus"),0.001f);
    ImGui::End();
    swap();

    GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);
    
    for(auto a : t)
       	ge::gl::glMakeTextureHandleNonResidentARB(a);

    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    {
        std::unique_lock<std::mutex> lck(*rdyMutex);
        rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
    }
    vars.getBool("loaded") = false;

    ge::gl::glFinish();
    auto time = vars.get<Timer<double>>("timer")->elapsedFromStart();
    vars.addOrGetFloat("elapsed") = time;
    //std::cerr << time << std::endl;
    if(time > FRAME_LIMIT)
        std::cerr << "Lag" << std::endl;
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long>((FRAME_LIMIT-time)*1000))); 
    
    if constexpr (SCREENSHOT_MODE)
    {
        static int a=0;
        a++;
        vars.reCreate<float>("xSelect",0.1*a);
        screenShot(std::to_string(a)+"shot.bmp", window->getWidth(), window->getHeight());
        if(!SCREENSHOT_VIDEO || 0.1*a > 7.0)
        {
            vars.reCreate<float>("xSelect",3.5);
            vars.getBool("mainRuns") = false;
            exit(0);
        }
    }
}

void LightFields::key(SDL_Event const& event, bool DOWN)
{
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    (*keys)[event.key.keysym.sym] = DOWN;

    if(event.key.keysym.sym == SDLK_ESCAPE)
    {
        vars.getBool("mainRuns") = false;
        exit(0); //I know, dirty...
    }
}

void LightFields::mouseMove(SDL_Event const& e)
{
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.get<glm::uvec2>("windowSize");
    auto const orbitZoomSpeed = 0.1f;//vars.getFloat("args.camera.orbitZoomSpeed");
    auto const xrel           = static_cast<float>(e.motion.xrel);
    auto const yrel           = static_cast<float>(e.motion.yrel);
    auto const mState         = e.motion.state;
    if (mState & SDL_BUTTON_LMASK)
    {
        if (orbitCamera)
        {
            orbitCamera->addXAngle(yrel * sensitivity);
            orbitCamera->addYAngle(xrel * sensitivity);
        }
    }
    if (mState & SDL_BUTTON_RMASK)
    {
        if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
    }
    if (mState & SDL_BUTTON_MMASK)
    {
        orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
                                  float(windowSize->x) * 2.f);
        orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                  float(windowSize->y) * 2.f);
    }
}

void LightFields::resize(uint32_t x,uint32_t y)
{
    auto windowSize = vars.get<glm::uvec2>("windowSize");
    windowSize->x = x;
    windowSize->y = y;
    vars.updateTicks("windowSize");
    ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[])
{
    LightFields app{argc, argv};
    try
    {
    app.start();
    }
    catch(sdl2cpp::ex::Exception &e)
    {
    std::cerr << e.what();
   }
    app.vars.get<std::thread>("loadingThread")->join();
    return EXIT_SUCCESS;
}
