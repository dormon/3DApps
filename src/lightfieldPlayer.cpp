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

constexpr float FRAME_LIMIT{1.0f/24};
constexpr bool SCREENSHOT_MODE{0};
constexpr bool SCREENSHOT_VIDEO{0};
constexpr bool MEASURE_TIME{1};
//TODO not multiple of local size resolution cases
constexpr int LOCAL_SIZE_X{32};
constexpr int LOCAL_SIZE_Y{8};

class LightFields: public simple3DApp::Application
{
public:
    LightFields(int argc, char* argv[]) : Application(argc, argv) {}
    virtual ~LightFields(){}
    virtual void draw() override;

    vars::Vars vars;

    virtual void                init() override;
    virtual void                deinit() override {vars.reCreate<bool>("mainRuns", false);}
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
    #extension GL_ARB_gpu_shader_int64 : require
    #extension GL_ARB_bindless_texture : require
    const uint lfSize = 64;
    uniform uint64_t lfTextures[lfSize];
    uniform uint64_t focusMap;
    //layout(bindless_image)uniform layout(r8ui) readonly uimage2D focusMap;
    out vec4 fColor;
    in vec2 vCoord;
    in vec3 position;
    
    void main()
    {
        usampler2D s = usampler2D(focusMap);
        fColor = vec4(vec3(float(texture(s, vCoord*textureSize(s,0)).x)/32.0),1.0);
    }
    ).";
    
    //#################################################################

    std::string csSrc = R".(  
    #extension GL_ARB_gpu_shader_int64 : require
    #extension GL_NV_shader_thread_shuffle : require
    #extension GL_ARB_bindless_texture : require
    #extension GL_ARB_shader_ballot : require 
    #extension GL_ARB_shader_image_load_store : require

    const uint lfSize = 64;
    const ivec2 gridSize = ivec2(8);
    const uint focusLevels = 32; //warp size
 
    layout(local_size_x=LSX, local_size_y=LSY)in;
    layout(bindless_image)uniform layout(r8ui) writeonly uimage2D focusMap;
    
    uniform uint64_t lfTextures[lfSize];
    uniform vec2 viewCoord;
    uniform float focusStep;
    uniform float focus;
    uniform float aspect;

    const float PI = 3.141592653589793238462643;
    vec3 bgr2xyz(vec3 BGR)
    {
        float r = BGR[0];
        float g = BGR[1];
        float b = BGR[2];
        if( r > 0.04045 )
            r = pow( ( r + 0.055 ) / 1.055, 2.4 );
        else
            r = r / 12.92;
        if( g > 0.04045 )
            g = pow( ( g + 0.055 ) / 1.055, 2.4 );
        else
            g = g / 12.92;
        if( b > 0.04045 )
            b = pow( ( b + 0.055 ) / 1.055, 2.4 );
        else
            b = b / 12.92;
        r *= 100.0;
        g *= 100.0;
        b *= 100.0;
        return vec3(r * 0.4124 + g * 0.3576 + b * 0.1805,
        r * 0.2126 + g * 0.7152 + b * 0.0722,
        r * 0.0193 + g * 0.1192 + b * 0.9505);
    }

    vec3 xyz2lab(vec3 XYZ)
    {
        float x = XYZ[0] / 95.047;
        float y = XYZ[1] / 95.047;
        float z = XYZ[2] / 95.047;
        if( x > 0.008856 )
            x = pow( x , .3333333333 );
        else
            x = ( 7.787 * x ) + ( 16.0 / 116.0 );
        if( y > 0.008856 )
            y = pow( y , .3333333333 );
        else
            y = ( 7.787 * y ) + ( 16.0 / 116.0 );
        if( z > 0.008856 )
            z = pow( z , .3333333333 );
        else
            z = ( 7.787 * z ) + ( 16.0 / 116.0 );
        return vec3(
        ( 116.0 * y ) - 16.0,
        500.0 * ( x - y ),
        200.0 * ( y - z ));
    }

    vec3 lab2lch(vec3 Lab)
    {
        return vec3(
        Lab[0],
        sqrt( ( Lab[1] * Lab[1] ) + ( Lab[2] * Lab[2] ) ),
        atan( Lab[1], Lab[2] ));
    }

    float deltaE2000l( vec3 lch1, vec3 lch2 )
    {
        float avg_L = ( lch1[0] + lch2[0] ) * 0.5;
        float delta_L = lch2[0] - lch1[0];
        float avg_C = ( lch1[1] + lch2[1] ) * 0.5;
        float delta_C = lch1[1] - lch2[1];
        float avg_H = ( lch1[2] + lch2[2] ) * 0.5;
        if( abs( lch1[2] - lch2[2] ) > PI )
            avg_H += PI;
        float delta_H = lch2[2] - lch1[2];
        if( abs( delta_H ) > PI )
        {
            if( lch2[2] <= lch1[2] )
                delta_H += PI * 2.0;
            else
                delta_H -= PI * 2.0;
        }

        delta_H = sqrt( lch1[1] * lch2[1] ) * sin( delta_H ) * 2.0;
        float T = 1.0 -
                0.17 * cos( avg_H - PI / 6.0 ) +
                0.24 * cos( avg_H * 2.0 ) +
                0.32 * cos( avg_H * 3.0 + PI / 30.0 ) -
                0.20 * cos( avg_H * 4.0 - PI * 7.0 / 20.0 );
        float SL = avg_L - 50.0;
        SL *= SL;
        SL = SL * 0.015 / sqrt( SL + 20.0 ) + 1.0;
        float SC = avg_C * 0.045 + 1.0;
        float SH = avg_C * T * 0.015 + 1.0;
        float delta_Theta = avg_H / 25.0 - PI * 11.0 / 180.0;
        delta_Theta = exp( delta_Theta * -delta_Theta ) * ( PI / 6.0 );
        float RT = pow( avg_C, 7.0 );
        RT = sqrt( RT / ( RT + 6103515625.0 ) ) * sin( delta_Theta ) * -2.0; // 6103515625 = 25^7
        delta_L /= SL;
        delta_C /= SC;
        delta_H /= SH;
        return sqrt( delta_L * delta_L + delta_C * delta_C + delta_H * delta_H + RT * delta_C * delta_H );
    }

    float deltaE2000( vec3 bgr1, vec3 bgr2 )
    {
        vec3 xyz1, xyz2, lab1, lab2, lch1, lch2;
        xyz1 = bgr2xyz( bgr1);
        xyz2 = bgr2xyz( bgr2);
        lab1 = xyz2lab( xyz1);
        lab2 = xyz2lab( xyz2);
        lch1 = lab2lch( lab1);
        lch2 = lab2lch( lab2);
        return deltaE2000l( lch1, lch2 );
    }

    void main()
    {
        const uint pixelId = gl_WorkGroupID.x*gl_WorkGroupSize.y + gl_LocalInvocationID.y;
        const ivec2 size = imageSize(focusMap);
        const ivec2 outCoords = ivec2(pixelId%size.x, pixelId/size.x);
        const vec2 inCoords = outCoords/vec2(size);
        const float focusValue = focus + gl_LocalInvocationID.x*focusStep;

        int n=0;
        vec3 mean = vec3(0);
        float m2=0.0;
        
        for(int x=0; x<gridSize.x; x++)
            for(int y=0; y<gridSize.y; y++)
            {   
                float dist = distance(viewCoord, vec2(x,y));
                if(dist > 2.0) continue;
                
                int slice = y*int(gridSize.x)+x;
                vec2 offset = viewCoord-vec2(x,y);
                offset.y *=-1;
                vec2 focusedCoords = clamp(inCoords+offset*focusValue*vec2(1.0,aspect), 0.0, 1.0); 
                sampler2D s = sampler2D(lfTextures[slice]);
                vec3 color = texture(s,focusedCoords).xyz;
            
                n++;
                vec3 delta = color-mean;
                float d = distance(color, mean);
                mean += delta/n;
                m2 += d*distance(color,mean);
            } 
        m2 /= n-1;

        for (uint offset=focusLevels/2; offset>0; offset/=2)
        {
            m2 = min(m2,shuffleDownNV(m2,offset,focusLevels));
        }
        float minM2=readFirstInvocationARB(m2);
        if(minM2 == m2)
        {
            imageStore(focusMap, outCoords, uvec4(mean.x*255));//gl_LocalInvocationID.x));
        }
    }
    ).";

    csSrc.replace(csSrc.find("LSX"), 3, std::to_string(LOCAL_SIZE_X));
    csSrc.replace(csSrc.find("LSY"), 3, std::to_string(LOCAL_SIZE_Y));

    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", vsSrc);
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", fsSrc);
    auto cs = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER, "#version 450\n", csSrc);

    auto program = vars.reCreate<ge::gl::Program>("lfProgram",vs,fs);
    if(program->getLinkStatus() == GL_FALSE)
        throw std::runtime_error("Cannot link shader program.");
    
    auto csProgram = vars.reCreate<ge::gl::Program>("csProgram",cs);
    if(csProgram->getLinkStatus() == GL_FALSE)
        throw std::runtime_error("Cannot link shader program.");
    csProgram->setNonexistingUniformWarning(false);
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
    auto lfSize = glm::uvec2(*vars.reCreate<unsigned int>("lf.width",decoder->getWidth()), *vars.reCreate<unsigned int>("lf.height", decoder->getHeight()));
    vars.reCreate<float>("texture.aspect",decoder->getAspect());
    //TODO correct framerate etc
    auto length = vars.addUint32("length",static_cast<int>(decoder->getLength()/64000000.0*25));
    
    //Must be here since all textures are created in this context and handles might overlap
    const uint32_t focusMapDiv = 4;
    auto focusMapSize = glm::uvec2(vars.addUint32("focusMap.width", lfSize.x/focusMapDiv), vars.addUint32("focusMap.height", lfSize.y/focusMapDiv));
    auto focusMapTexture = ge::gl::Texture(GL_TEXTURE_2D,GL_R8UI,1,focusMapSize.x,focusMapSize.y);
    auto focusMap = vars.add<GLuint64>("focusMap.image",ge::gl::glGetImageHandleARB(focusMapTexture.getId(), 0, GL_FALSE, 0, GL_R8UI));
  
    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    int frameNum = 1;   
    while(vars.getBool("mainRuns"))
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
            vars.reCreateVector<GLuint64>(nextTexturesName, decoder->getFrames(64));
            GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);
        
            vars.getUint32("lfTexturesIndex") = index;

            //TODO without waiting? async draw to increase responsitivity
            frameNum++;
            vars.reCreate<int>("frameNum", frameNum);
        
            //vars.getBool("pause") = true;
        }
        vars.getBool("loaded") = true;
        rdyCv->notify_all();
    }
}

void LightFields::init()
{
/*GLint result;
ge::gl::glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &result);
std::cerr << result << std::endl;
*/
   /* 
    constexpr int size{64};
    std::vector<glm::vec3> data(size);
    glm::vec3 sum(0);
    srand(time(NULL));
    for (auto& x : data)
    {
        x = glm::vec3(rand()%256, rand()%256, rand()%256);
        sum += x;
        //std::cerr << x.x << std::endl;
    }
    sum /= size;
    float dev = 0;
    for (auto& x : data)
        dev += glm::pow(glm::distance(x,sum),2);
    dev /= size;
    std::cerr << dev << std::endl;

    //online
    int n=0;
    glm::vec3 mean(0);
    float m2;
    for (auto& x : data)
    {
        n++;
        glm::vec3 delta = x-mean;
        float d = distance(x, mean);
        mean += delta/glm::vec3(n);
        m2 += d*glm::distance(x, mean);
    }
    m2 /= n-1;
    std::cerr << m2 << std::endl;
    exit(1);
    */

    auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
    auto const videoFile = args->gets("--video","","h265 compressed LF video (pix_fmt yuv420p)");
    auto const gridSize = args->getu32("--grid",8,"number of cameras in row/column (assuming a rectangle)");
    auto const showHelp = args->isPresent("-h","shows help");
    if (showHelp || !args->validate()) {
        std::cerr << args->toStr();
        exit(0);
    }
    vars.addString("videoFile", videoFile);
    vars.add<glm::ivec2>("gridSize",glm::ivec2(glm::uvec2(gridSize)));

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
    ge::gl::glMakeImageHandleResidentARB(*vars.get<GLuint64>("focusMap.image"), GL_READ_WRITE);

    SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),window->getContext("rendering"));

    vars.add<ge::gl::VertexArray>("emptyVao");
    vars.addFloat("input.sensitivity",0.01f);
    vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
    vars.addFloat("camera.fovy",glm::half_pi<float>());
    vars.addFloat("camera.near",.1f);
    vars.addFloat("camera.far",1000.f);
    vars.addFloat("focus",0.0f);
    vars.addFloat("focusStep",0.0f);
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

    std::vector<GLuint64> t = vars.getVector<GLuint64>(currentTexturesName);
    for(auto a : t)
    	ge::gl::glMakeTextureHandleResidentARB(a);

    createCamera(vars);
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

    drawGrid(vars);
    
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    auto gridIndex = *vars.get<glm::ivec2>("gridSize")-1;
    float aspect = vars.getFloat("texture.aspect");
    glm::vec3 lfPos(0,0,0);
    glm::vec3 camPos(view->getView()[3]);
    glm::vec3 centerRay = normalize(lfPos - camPos); 
    float range = .2;
    float coox = (glm::clamp(centerRay.x*-1,-range*aspect,range*aspect)/(range*aspect)+1)/2.;
    float cooy = (glm::clamp(centerRay.y,-range,range)/range+1)/2.;
    glm::vec2 viewCoord = glm::vec2(glm::clamp(coox*(gridIndex.x),0.0f,static_cast<float>(gridIndex.x)),glm::clamp(cooy*(gridIndex.y),0.0f,static_cast<float>(gridIndex.y)));

    auto csProgram = vars.get<ge::gl::Program>("csProgram");
    csProgram
    //->set2uiv("gridSize",glm::value_ptr(*vars.get<glm::uvec2>("gridSize")))
    ->set1i("focus",vars.getFloat("focus"))
    ->set1i("focusStep",vars.getFloat("focusStep"))
    ->set2fv("viewCoord",glm::value_ptr(viewCoord))
    ->set1f("aspect",aspect)
    ->use();
    ge::gl::glProgramUniformHandleui64vARB(csProgram->getId(), csProgram->getUniformLocation("lfTextures"), 64, vars.getVector<GLuint64>(currentTexturesName).data());
    ge::gl::glProgramUniformHandleui64vARB(csProgram->getId(), csProgram->getUniformLocation("focusMap"), 1, vars.get<GLuint64>("focusMap.image"));

    auto query = ge::gl::AsynchronousQuery(GL_TIME_ELAPSED, GL_QUERY_RESULT, ge::gl::AsynchronousQuery::UINT64);
    if constexpr (MEASURE_TIME) query.begin();

    ge::gl::glDispatchCompute(*vars.get<unsigned int>("focusMap.width") * *vars.get<unsigned int>("focusMap.height")/LOCAL_SIZE_Y,1,1);    
    ge::gl::glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    ge::gl::glFinish();
    
    if constexpr (MEASURE_TIME)
    {
        query.end();
        vars.reCreate<float>("csElapsed", query.getui64()/1000000.0);
    } 

    auto program = vars.get<ge::gl::Program>("lfProgram");
    program
    ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()))
    ->set1f("aspect",aspect)
    ->use();
    ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lfTextures"), 64, vars.getVector<GLuint64>(currentTexturesName).data());
    ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("focusMap"), 1, vars.get<GLuint64>("focusMap.image"));
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
            mainLoop->stop();
        }
    }
}

void LightFields::key(SDL_Event const& event, bool DOWN)
{
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    (*keys)[event.key.keysym.sym] = DOWN;

    if(event.key.keysym.sym == SDLK_ESCAPE)
        mainLoop->stop();
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
