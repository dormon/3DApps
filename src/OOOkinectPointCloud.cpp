#include <kinectPointCloud.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/Camera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <addVarsLimits.h>
#include <azureKinectUtils.h>

int colorModeIndex{0};
int depthModeIndex{1};
const k4a_color_resolution_t colorModes[]{K4A_COLOR_RESOLUTION_720P, K4A_COLOR_RESOLUTION_2160P, K4A_COLOR_RESOLUTION_1440P, K4A_COLOR_RESOLUTION_1080P, K4A_COLOR_RESOLUTION_3072P, K4A_COLOR_RESOLUTION_1536P};
const k4a_depth_mode_t depthModes[]{K4A_DEPTH_MODE_NFOV_UNBINNED, K4A_DEPTH_MODE_WFOV_UNBINNED, K4A_DEPTH_MODE_NFOV_2X2BINNED, K4A_DEPTH_MODE_WFOV_2X2BINNED};
constexpr int DEVICE_COUNT{1};

void createKPCProgram(vars::Vars&vars){
    if(notChanged(vars,"all",__FUNCTION__,{}))return;

    std::string vsSrc = R".(
  uniform mat4 projView;
  out vec3 vColor;

  layout(binding=DEVICE_COUNT*2)uniform sampler2D xyTexture;
  layout(binding=0)uniform sampler2D colorTextures[DEVICE_COUNT];
  layout(binding=DEVICE_COUNT)uniform isampler2D depthTextures[DEVICE_COUNT];

  uniform vec2 clipRange;
  uniform int lod;

  void main(){
    ivec2 coord;
    ivec2 size = textureSize(depthTextures[0],0);
    int pixels = size.x*size.y;
    int index = gl_VertexID/pixels;
    int pixelIndex = gl_VertexID-index*pixels;
    coord.x = (lod*pixelIndex)%size.x;
    coord.y = (lod*pixelIndex)/size.x;
    float depth = float(texelFetch(depthTextures[index],coord,0).x);
    vec2 xy = texelFetch(xyTexture, coord, 0).xy;
    vec3 pos = vec3(depth*xy.x, depth*xy.y, depth); 
    if(depth == 0 || depth < clipRange.s || depth > clipRange.t || (xy.x == 0.0 && xy.y == 0.0))
    {
        gl_Position = vec4(2,2,2,1);
        return;
    }

    pos *= 0.001f;
    pos *= -1;
    vColor = texelFetch(colorTextures[index],coord,0).xyz;
    gl_Position = projView * vec4(pos,1);
  }
  ).";

    std::string const fsSrc = R".(
  out vec4 fColor;
  in  vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";
    
    const std::string s = "DEVICE_COUNT";
    const std::string t = std::to_string(DEVICE_COUNT);
    std::string::size_type n = 0;
    while ( ( n = vsSrc.find( s, n ) ) != std::string::npos )
    {
        vsSrc.replace( n, s.size(), t );
        n += t.size();
    }

    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
            "#version 450\n",
            vsSrc
            );
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
            "#version 450\n",
            fsSrc
            );
    vars.reCreate<ge::gl::Program>("pointCloudProgram",vs,fs)->setNonexistingUniformWarning(false);
}

void createXyTable(const k4a_calibration_t *calibration, k4a::image *xyTable)
{
    k4a_float2_t *table_data = (k4a_float2_t *)(void *)xyTable->get_buffer();

    int width = calibration->depth_camera_calibration.resolution_width;
    int height = calibration->depth_camera_calibration.resolution_height;

    k4a_float2_t p;
    k4a_float3_t ray;
    int valid;

    for (int y = 0, idx = 0; y < height; y++)
    {
        p.xy.y = (float)y;
        for (int x = 0; x < width; x++, idx++)
        {
            p.xy.x = (float)x;

            k4a_calibration_2d_to_3d(
                    calibration, &p, 1.f, K4A_CALIBRATION_TYPE_DEPTH, K4A_CALIBRATION_TYPE_DEPTH, &ray, &valid);

            if (valid)
            {
                table_data[idx].xy.x = ray.xyz.x;
                table_data[idx].xy.y = ray.xyz.y;
            }
            else
            {
                table_data[idx].xy.x = nanf("");
                table_data[idx].xy.y = nanf("");
            }
        }
    }
}

void initKPCDevice(int colorModeIndex, int depthModeIndex, vars::Vars&vars){
    const uint32_t deviceCount = k4a::device::get_installed_count();
    if (deviceCount < DEVICE_COUNT)
        throw std::runtime_error("Not enough Azure Kinect devices connected");

    k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.depth_mode = depthModes[depthModeIndex];
    config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    config.color_resolution = colorModes[colorModeIndex];
    config.camera_fps = (config.color_resolution == K4A_COLOR_RESOLUTION_3072P || config.depth_mode == K4A_DEPTH_MODE_WFOV_UNBINNED) ? K4A_FRAMES_PER_SECOND_15 : K4A_FRAMES_PER_SECOND_30;
    config.synchronized_images_only = true;
    auto size = vars.reCreate<glm::ivec2>("depthSize"); 
    *size = getDepthResolution(config.depth_mode);

    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto dev = vars.reCreate<k4a::device>("kinectDevice"+index);
        *dev = k4a::device::open(i);
        dev->start_cameras(&config);
        vars.reCreate<ge::gl::Texture>("colorTexture"+index,GL_TEXTURE_2D,GL_RGBA32F,1,size->x,size->y); 
        vars.reCreate<ge::gl::Texture>("depthTexture"+index,GL_TEXTURE_2D,GL_R16UI,1,size->x,size->y);
    }

    auto cal = vars.get<k4a::device>("kinectDevice0")->get_calibration(config.depth_mode, config.color_resolution);
    auto trans = vars.reCreate<k4a::transformation>("transformation",cal); 

    auto xyTable = vars.reCreate<k4a::image>("xyTable");
    *xyTable = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM, size->x, size->y, size->x * (int)sizeof(k4a_float2_t));
    createXyTable(&cal,xyTable);
    auto xyTexture = vars.reCreate<ge::gl::Texture>("xyTexture",GL_TEXTURE_2D,GL_RG32F,1,size->x,size->y);
    ge::gl::glTextureSubImage2D(xyTexture->getId(),0,0,0,size->x, size->y,GL_RG,GL_FLOAT,xyTable->get_buffer());
}

void initKPC(vars::Vars&vars){
    vars.addFloat("image.pointSize",1);
    addVarsLimitsF(vars,"image.pointSize",0.0f,10.0f);
    vars.addUint32("lod", 1);
    addVarsLimitsU(vars,"lod",1,10);
    vars.add<glm::vec2>("clipRange", 0, 2000);

    initKPCDevice(colorModeIndex, depthModeIndex ,vars);
}

void updateKPC(vars::Vars&vars)
{
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        k4a::capture capture;
        auto dev = vars.get<k4a::device>("kinectDevice"+index);
        if (dev->get_capture(&capture, std::chrono::milliseconds(0)))
        {
            const k4a::image depthImage = capture.get_depth_image();
            const k4a::image colorImage = capture.get_color_image();

            auto size = vars.get<glm::ivec2>("depthSize");
            auto depthTexture = vars.get<ge::gl::Texture>("depthTexture"+index);
            auto colorTexture = vars.get<ge::gl::Texture>("colorTexture"+index);

            auto trans = vars.get<k4a::transformation>("transformation");
            k4a::image tImage = trans->color_image_to_depth_camera(depthImage, colorImage);
            ge::gl::glTextureSubImage2D(depthTexture->getId(),0,0,0,size->x, size->y,GL_RED_INTEGER,GL_UNSIGNED_SHORT,depthImage.get_buffer());
            ge::gl::glTextureSubImage2D(colorTexture->getId(),0,0,0,size->x, size->y,GL_BGRA,GL_UNSIGNED_BYTE,tImage.get_buffer());
        }
    }

    ImGui::Begin("vars");
    std::vector<const char*> colorModeList = { "720p", "2160p", "1440p", "1080p", "3072p", "1536p" };
    static int currentColor = 0;
    bool changed = false;
    if(ImGui::ListBox("Color mode", &currentColor, colorModeList.data(), colorModeList.size(), 4))
    {
        colorModeIndex = currentColor;
        changed = true;
    }
    std::vector<const char*> depthModeList = { "NFOV unbinned", "WFOV unbinned", "NFOV binned", "WFOV unbinned" };
    static int currentDepth = 1;
    if(ImGui::ListBox("Depth mode", &currentDepth, depthModeList.data(), depthModeList.size(), 4))
    {
        depthModeIndex = currentDepth;
        changed = true;
    }
    ImGui::End();  
    if(changed)
        initKPCDevice(colorModeIndex, depthModeIndex, vars);
}

void drawKPC(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){

    createKPCProgram(vars);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();


    auto xyTexture     = vars.get<ge::gl::Texture>("xyTexture");
    auto lod = vars.getUint32("lod");
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto colorTex = vars.get<ge::gl::Texture>("colorTexture"+index);
        auto depthTex = vars.get<ge::gl::Texture>("depthTexture"+index);
        colorTex->bind(i);
        depthTex->bind(i+DEVICE_COUNT);
    }
    xyTexture->bind(DEVICE_COUNT*2);
    vars.get<ge::gl::Program>("pointCloudProgram")
        ->setMatrix4fv("projView"      ,glm::value_ptr(proj*view))
        ->set2fv("clipRange",reinterpret_cast<float*>(vars.get<glm::vec2>("clipRange")))
        ->set1i("lod", lod)
        ->use();

    auto size = vars.get<glm::ivec2>("depthSize");
    ge::gl::glEnable(GL_DEPTH_TEST);
    ge::gl::glPointSize(vars.getFloat("image.pointSize"));
    ge::gl::glDrawArrays(GL_POINTS,0,DEVICE_COUNT*size->x*size->y/lod);
    ge::gl::glDisable(GL_DEPTH_TEST);

    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto colorTex = vars.get<ge::gl::Texture>("colorTexture"+index);
        auto depthTex = vars.get<ge::gl::Texture>("depthTexture"+index);
        colorTex->unbind(i);
        depthTex->unbind(i+DEVICE_COUNT);
    }
    xyTexture->unbind(DEVICE_COUNT*2);
}

void drawKPC(vars::Vars&vars)
{
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
    drawKPC(vars,view->getView(),projection->getProjection());
}

