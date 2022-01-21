#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <geGL/Texture.h>
#include <glm/detail/func_exponential.hpp>
#include <glm/gtc/constants.hpp>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <imguiVars/addVarsLimits.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <drawGrid.h>
#include <FreeImagePlus.h>
#include <imguiDormon/imgui.h>
#include <Timer.h>
#include <holoCalibration.h>

#include<assimp/cimport.h>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

#include<geGL/geGL.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_access.hpp>
#include<glm/gtx/vector_angle.hpp>

#include <memory>
#include <time.h>
#include <fstream>
#include <sstream>
#include <iomanip>

#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

std::string const varsPrefix = "measurements";

SDL_Surface *flipSurface(SDL_Surface *sfc)
{
    SDL_Surface *result = SDL_CreateRGBSurface(sfc->flags, sfc->w, sfc->h,
                          sfc->format->BytesPerPixel * 8, sfc->format->Rmask, sfc->format->Gmask,
                          sfc->format->Bmask, sfc->format->Amask);
    const auto pitch = sfc->pitch;
    const auto pxlength = pitch*(sfc->h - 1);
    auto pixels = static_cast<unsigned char *>(sfc->pixels) + pxlength;
    auto rpixels = static_cast<unsigned char *>(result->pixels) ;

    for(auto line = 0; line < sfc->h; ++line)
    {
        memcpy(rpixels, pixels, pitch);
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

class TestCase
{
public:
    enum TestType {MAX_SLIDER=0, BEST_SLIDER, CHECKBOX};
    enum TestCategory {CAMERAS, CONE_MAX, CONE_MIN, CONE_BEST, DOF30, DOF60, CIRCULAR, CIRCULAR_INVERT};
    std::string name;
    TestType type;
    TestCategory category{CAMERAS};
    bool compensate{false};
};


class Holo: public simple3DApp::Application
{
public:
    Holo(int argc, char *argv[]) : Application(argc, argv) {}
    virtual ~Holo() {}
    virtual void draw() override;
    void stop()
    {
        mainLoop->stop();
    };

    vars::Vars vars;
    bool fullscreen = false;

    virtual void                init() override;
    virtual void                resize(uint32_t x, uint32_t y) override;
    virtual void                key(SDL_Event const &e, bool down) override;
    virtual void                mouseMove(SDL_Event const &event) override;
};

std::shared_ptr<ge::gl::Texture> loadColorTexture(std::string fileName)
{
    if(fileName.empty())
        return std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D, GL_RGB8, 1, 1, 1);

    fipImage colorImg;
    colorImg.load(fileName.c_str());
    auto const width   = colorImg.getWidth();
    auto const height  = colorImg.getHeight();
    auto const BPP     = colorImg.getBitsPerPixel();
    auto const imgType = colorImg.getImageType();
    auto const data    = colorImg.accessPixels();

    std::cerr << "color BPP : " << BPP << std::endl;
    std::cerr << "color type: " << imgType << std::endl;

    GLenum format;
    GLenum type;

    if(imgType == FIT_BITMAP)
    {
        std::cerr << "color imgType: FIT_BITMAP" << std::endl;

        if(BPP == 24)format = GL_BGR;

        if(BPP == 32)format = GL_BGRA;

        type = GL_UNSIGNED_BYTE;
    }

    if(imgType == FIT_RGBAF)
    {
        std::cerr << "color imgType: FIT_RGBAF" << std::endl;

        if(BPP == 32*4)format = GL_RGBA;

        if(BPP == 32*3)format = GL_RGB;

        type = GL_FLOAT;
    }

    if(imgType == FIT_RGBA16)
    {
        std::cerr << "color imgType: FIT_RGBA16" << std::endl;

        if(BPP == 48)format = GL_RGB ;

        if(BPP == 64)format = GL_RGBA;

        type = GL_UNSIGNED_SHORT;
    }

    auto colorTex = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D, GL_RGB8, 1, width, height);
    //ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
    //ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
    ge::gl::glTextureSubImage2D(colorTex->getId(), 0, 0, 0, width, height, format, type, data);
    return colorTex;
}


class Model
{
public:
    aiScene const *model = nullptr;
    Model(std::string const &name);
    virtual ~Model();
    std::vector<float> getVertices() const;
    std::vector<float> getNormals() const;
    std::vector<float> getUVs() const;

    std::string getName() const
    {
        return name;
    };

protected:
    void generateVertices();
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> uvs;

    std::string name;
};

class RenderModel: public ge::gl::Context
{
public:
    RenderModel(Model *mdl, std::string textureFileName, std::string bckgFileName);
    ~RenderModel();
    void draw(glm::mat4 const &view, glm::mat4 const &projection, vars::Vars &vars);
    std::shared_ptr<ge::gl::VertexArray>vao           = nullptr;
    std::shared_ptr<ge::gl::Buffer     >vertices      = nullptr;
    std::shared_ptr<ge::gl::Buffer     >normals       = nullptr;
    std::shared_ptr<ge::gl::Buffer     >uvs       = nullptr;
    std::shared_ptr<ge::gl::Buffer     >indices       = nullptr;
    std::shared_ptr<ge::gl::Buffer     >indexVertices = nullptr;
    std::shared_ptr<ge::gl::Program    >program       = nullptr;
    std::shared_ptr<ge::gl::Program    >bckgProgram       = nullptr;
    std::shared_ptr<ge::gl::Texture>   bckgTex = nullptr;
    std::shared_ptr<ge::gl::Texture>   modelTex = nullptr;
    glm::vec3 lightPos{0, 0, 1000};
    uint32_t nofVertices = 0;
};

Model::Model(std::string const &fileName)
{
    model = aiImportFile(fileName.c_str(), aiProcess_Triangulate|aiProcess_GenNormals|aiProcess_SortByPType);

    if (model == nullptr)
    {
        std::cerr << "Can't open file: " << fileName << std::endl;
    }
    else
    {
        generateVertices();
        name = model->GetShortFilename(fileName.c_str());
    }
}

Model::~Model()
{
    assert(this!=nullptr);

    if(this->model)aiReleaseImport(this->model);
}

std::vector<float> Model::getVertices() const
{
    return vertices;
}

std::vector<float> Model::getNormals() const
{
    return normals;
}

std::vector<float> Model::getUVs() const
{
    return uvs;
}

void Model::generateVertices()
{
    size_t nofVertices = 0;

    for(size_t i=0; i<model->mNumMeshes; ++i)
        nofVertices+=model->mMeshes[i]->mNumFaces*3;

    vertices.reserve(nofVertices*3);

    for(size_t i=0; i<model->mNumMeshes; ++i)
    {
        auto mesh = model->mMeshes[i];

        for(size_t j=0; j<mesh->mNumFaces; ++j)
            for(size_t k=0; k<3; ++k)
                for(size_t l=0; l<3; ++l)
                {
                    auto element = mesh->mFaces[j].mIndices[k];
                    vertices.push_back(mesh->mVertices[element][l]);
                    normals.push_back(mesh->mNormals[element][l]);

                    if(l<2)
                        uvs.push_back(mesh->mTextureCoords[0][element][l]);
                }
    }
}

RenderModel::RenderModel(Model *mdl, std::string textureFileName, std::string bckgFileName)
{
    assert(this!=nullptr);

    if(mdl==nullptr)
        std::cerr << "mdl is nullptr!" << std::endl;

    this->nofVertices = 0;
    auto model = mdl->model;

    for(size_t i=0; i<model->mNumMeshes; ++i)
        this->nofVertices+=model->mMeshes[i]->mNumFaces*3;

    std::vector<float>vertData;
    vertData = mdl->getVertices();
    this->vertices = std::make_shared<ge::gl::Buffer>(this->nofVertices*sizeof(float)*3, vertData.data());
    auto normData = mdl->getNormals();
    this->normals = std::make_shared<ge::gl::Buffer>(this->nofVertices*sizeof(float)*3, normData.data());
    auto uvData = mdl->getUVs();
    this->uvs = std::make_shared<ge::gl::Buffer>(this->nofVertices*sizeof(float)*2, uvData.data());

    this->vao = std::make_shared<ge::gl::VertexArray>();
    this->vao->addAttrib(this->vertices, 0, 3, GL_FLOAT);
    this->vao->addAttrib(this->normals, 1, 3, GL_FLOAT);
    this->vao->addAttrib(this->uvs, 2, 2, GL_FLOAT);

    bckgTex = loadColorTexture(bckgFileName);
    bckgTex->texParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    bckgTex->texParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    modelTex = loadColorTexture(textureFileName);

    const std::string vertSrc =
        "#version 450 \n"
        R".(
      uniform mat4 projection = mat4(1);
      uniform mat4 view       = mat4(1);

      layout(location=0)in vec3 position;
      layout(location=1)in vec3 normal;
      layout(location=2)in vec2 uv;

      out vec3 vPosition;
      out vec3 vNormal;
      out vec2 vUV;

      flat out uint vID;

      void main(){
        vID = gl_VertexID/3;
        gl_Position = projection*view*vec4(position,1);
        vPosition = position;
        vNormal   = normal;
        vUV   = uv;
      }).";
    const std::string fragSrc =
        "#version 450\n"
        R".(
      layout(location=0)out vec4 fColor;
      layout(binding=0)uniform sampler2D modelTex;

      uniform mat4 view = mat4(1);
      uniform vec3 lightPos = vec3(0,1000,0);

      in vec3 vPosition;
      in vec3 vNormal;
      in vec2 vUV;

      flat in uint vID;
      vec3 hue(float t){
        t = fract(t);
        if(t<1/6.)return mix(vec3(1,0,0),vec3(1,1,0),(t-0/6.)*6);
        if(t<2/6.)return mix(vec3(1,1,0),vec3(0,1,0),(t-1/6.)*6);
        if(t<3/6.)return mix(vec3(0,1,0),vec3(0,1,1),(t-2/6.)*6);
        if(t<4/6.)return mix(vec3(0,1,1),vec3(0,0,1),(t-3/6.)*6);
        if(t<5/6.)return mix(vec3(0,0,1),vec3(1,0,1),(t-4/6.)*6);
                  return mix(vec3(1,0,1),vec3(1,0,0),(t-5/6.)*6);
      }

      void main(){

        vec3  diffuseColor   = texture(modelTex, vUV).rgb;//hue(vID*3.14159254f);
        vec3  specularColor  = vec3(1);
        float specularFactor = 1;

        vec3 cameraPos = vec3(inverse(view)*vec4(0,0,0,1));

        vec3 N = normalize(vNormal);
        vec3 L = normalize(lightPos-vPosition);
        vec3 V = normalize(cameraPos-vPosition   );
        vec3 R = reflect(V,N);

        float dF = max(dot(L,N),0);
        float sF = max(dot(R,L),0)*dF;

        vec3 dl = dF * diffuseColor;
        vec3 sl = sF * specularColor;

        //fColor = vec4(dl + sl,1);
        fColor = vec4(diffuseColor,1);

      }).";
    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, vertSrc);
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, fragSrc);
    this->program = std::make_shared<ge::gl::Program>(vs, fs);
    this->program->setNonexistingUniformWarning(false);

    const std::string bckgVertSrc =
        "#version 450 \n"
        R".(
      #extension GL_KHR_vulkan_glsl : enable
      out vec2 uv;
      uniform mat4 projection = mat4(1);
      uniform mat4 view       = mat4(1);
      uniform bool isRotation = false;

      void main(){
        uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
        gl_Position = vec4(2.0f*uv-1.0f, 0.9999999f, 1.0f);
        gl_Position *= 5; 
        gl_Position.z = -10;
        gl_Position.w = 1;
        mat4 newView = view;
        if(!isRotation)
            newView=mat4(1);
        gl_Position = projection*view*gl_Position;//c4(position,1);
      }
    ).";
    const std::string bckgFragSrc =
        "#version 450\n"
        R".(
      in vec2 uv;
      layout(location = 0) out vec4 outColor;
      layout(binding=0)uniform sampler2D bckgTex;
      uniform float blurAmount = 0.0;

    const float pi = atan(1.0) * 4.0;
    const int samples = 16;
    const float sigma = float(samples) * 0.25;

    float gaussian(vec2 i) {
        return 1.0 / (2.0 * pi * sigma*sigma * exp(-((i.x*i.x + i.y*i.y) / (2.0 * sigma*sigma))));
    }

    vec3 blur(sampler2D sp, vec2 uv, vec2 scale) {
        vec3 col = vec3(0.0);
        float accum = 0.0;
        float weight;
        vec2 offset;

        for (int x = -samples / 2; x < samples / 2; ++x) {
            for (int y = -samples / 2; y < samples / 2; ++y) {
                offset = vec2(x, y);
                weight = gaussian(offset);
                col += texture(sp, uv + scale * offset).rgb * weight;
                accum += weight;
            }
        }

        return col / accum;
        }


      void main(){
        vec2 pixelSize = 1.0/textureSize(bckgTex, 0);
        outColor = vec4(blur(bckgTex,uv, pixelSize*blurAmount),1);
        //outColor = texture(bckgTex,uv);

      }).";
    auto bckgVs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, bckgVertSrc);
    auto bckgFs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, bckgFragSrc);
    this->bckgProgram = std::make_shared<ge::gl::Program>(bckgVs, bckgFs); // exit(1);
}

RenderModel::~RenderModel()
{
    assert(this!=nullptr);
}

void RenderModel::draw(glm::mat4 const &view, glm::mat4 const &projection, vars::Vars &vars)
{
    assert(this!=nullptr);
    ge::gl::glEnable(GL_DEPTH_TEST);
    this->bckgProgram->use();
    this->bckgTex->bind(0);
    this->bckgProgram->set1f("blurAmount", vars.getFloat("blurAmount"));
    this->bckgProgram->setMatrix4fv("projection", glm::value_ptr(projection));
    this->bckgProgram->setMatrix4fv("view", glm::value_ptr(view      ));
    auto currentTestID = vars.addOrGetUint32(varsPrefix+"current",0);
    this->bckgProgram->set1i("isRotation", currentTestID<6 || (currentTestID > 12 && currentTestID <18)); 
    this->vao->bind();
    this->glDrawArrays(GL_TRIANGLES, 0, 3);
    this->program->use();
    this->modelTex->bind(0);
    this->program->set3fv("lightPos", glm::value_ptr(this->lightPos));
    this->program->setMatrix4fv("projection", glm::value_ptr(projection));
    this->program->setMatrix4fv("view", glm::value_ptr(view      ));
    this->glDrawArrays(GL_TRIANGLES, 0, this->nofVertices);
    this->vao->unbind();
}

void preprareDrawModel(vars::Vars &vars)
{
    if(notChanged(vars, "all", __FUNCTION__, {"modelFileName"}))return;

    vars.add<Model          >("model", vars.getString("modelFileName"));
    vars.add<RenderModel    >("renderModel", vars.get<Model>("model"), vars.getString("modelTexFileName"), vars.getString("bckgTexFileName"));
}

void drawModel(vars::Vars &vars, glm::mat4 const &view, glm::mat4 const &proj)
{
    preprareDrawModel(vars);

    auto rm = vars.get<RenderModel>("renderModel");
    rm->draw(view, proj, vars);
}

void drawModel(vars::Vars &vars)
{
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
    drawModel(vars, view->getView(), projection->getProjection());
}

void createView(vars::Vars &vars)
{
    if(notChanged(vars, "all", __FUNCTION__, {"useOrbitCamera"}))return;

    if(vars.getBool("useOrbitCamera"))
        vars.reCreate<basicCamera::OrbitCamera>("view");
    else
        vars.reCreate<basicCamera::FreeLookCamera>("view");
}

void createProjection(vars::Vars &vars)
{
    if(notChanged(vars, "all", __FUNCTION__, {"windowSize", "camera.fovy", "camera.near", "camera.far"}))return;

    auto windowSize = vars.get<glm::uvec2>("windowSize");
    auto width = windowSize->x;
    auto height = windowSize->y;
    auto aspect = (float)width/(float)height;
    auto nearv = vars.getFloat("camera.near");
    auto farv  = vars.getFloat("camera.far" );
    auto fovy = vars.getFloat("camera.fovy");

    vars.reCreate<basicCamera::PerspectiveCamera>("projection", fovy, aspect, nearv, farv);
}

void createCamera(vars::Vars &vars)
{
    createProjection(vars);
    createView(vars);
}

void loadTextures(vars::Vars &vars)
{
    if(notChanged(vars, "all", __FUNCTION__, {"quiltTexFileName"}))return;
    vars.add<std::shared_ptr<ge::gl::Texture>>("quiltTex", loadColorTexture(vars.getString("quiltTexFileName")));
}

void createHoloProgram(vars::Vars &vars)
{
    if(notChanged(vars, "all", __FUNCTION__, {}))return;

    std::string const vsSrc = R".(
      #version 450 core

      out vec2 texCoords;

      void main(){
        texCoords = vec2(gl_VertexID&1,gl_VertexID>>1);
        gl_Position = vec4(texCoords*2-1,0,1);
      }
      ).";

    std::string const fsSrc = R".(
      #version 450 core

      in vec2 texCoords;

      layout(location=0)out vec4 fragColor;

      uniform int showQuilt = 0;
      uniform int showAsSequence = 0;
      uniform uint selectedView = 0;
      // HoloPlay values
      uniform float pitch = 354.42108f;
      uniform float tilt = -0.1153f;
      uniform float center = 0.04239f;
      uniform float invView = 1.f;
      uniform float flipX;
      uniform float flipY;
      uniform float subp = 0.00013f;
      uniform float zoomAmount = 1.0;
      uniform float imageZ = 0.9;
      uniform int ri = 0;
      uniform int bi = 2;
      uniform vec4 tile = vec4(5,9,45,45);
      uniform vec4 viewPortion = vec4(0.99976f, 0.99976f, 0.00f, 0.00f);
      uniform vec4 aspect;
      uniform uint drawOnlyOneImage = 0;
      uniform mat4 cameraTransformations[45];
      uniform mat4 projection;
      uniform bool compensation = false;

      layout(binding=0)uniform sampler2D screenTex;

      uniform float focus = 0.f;

      uniform uint stride = 1;

      vec2 texArr(vec3 uvz)
      {
          // decide which section to take from based on the z.
          float z = floor(uvz.z * tile.z);
          float focusMod = focus*(1-2*clamp(z/tile.z,0,1));
          float x = (mod(z, tile.x) + clamp(uvz.x+focusMod,-10,10)) / tile.x; //TODO clamping 0-1 might be necessary not 10
          float y = (floor(z / tile.x) + uvz.y) / tile.y;
          return vec2(x, y) * viewPortion.xy;
      }

      vec2 compensationTransform(vec2 coords, int index)
      {
        coords=((coords-0.5)*zoomAmount)+0.5;
        if(!compensation)
            return coords;
        vec4 worldCoords = inverse(projection)*vec4(2*coords-1,imageZ,1);
        worldCoords/=worldCoords.w;
        vec4 transCoords = projection*cameraTransformations[index]*vec4(worldCoords);
        return (((transCoords.xy/transCoords.w)+1)/2);
      } 

      vec2 quiltCompensation(vec2 coords)
      {
        ivec2 gridCoords = ivec2(trunc(coords.xy*tile.xy));
        int index = gridCoords.y*int(tile.x)+gridCoords.x;
        vec2 nCoords = texArr(vec3(mod(coords.xy*tile.xy,vec2(1)),index/float(tile.z)));

        vec2 tileSize = 1.0/tile.xy;
        vec4 tileBorders = vec4(tileSize*gridCoords, tileSize*(gridCoords+1));
        vec2 normalizedCoords = (nCoords-tileBorders.xy)/tileSize;
        return compensationTransform(normalizedCoords, index) * tileSize + tileBorders.xy;
      }

      vec2 compensate(vec3 coords)
      {
        coords.xy = compensationTransform(coords.xy, int(coords.z*tile.z));
        return texArr(coords);
      }

      vec4 getClampingBorders(vec2 coords)
      {
        vec2 gridCoords = trunc(coords.xy*tile.xy);
        vec2 tileSize = 1.0/tile.xy;
        return vec4(tileSize*gridCoords, tileSize*(gridCoords+1));
      }

      void main()
      {
        vec3 nuv = vec3(texCoords.xy, 0.0);

        vec4 rgb[3];
        for (int i=0; i < 3; i++) 
        {
            nuv.z = (texCoords.x + i * subp + texCoords.y * tilt) * pitch - center;
            nuv.z = fract(nuv.z);
            if(invView > 0.5)
                nuv.z = (1.0 - nuv.z);
            nuv.z = (floor(nuv.z*tile.z/stride)*stride + fract(nuv.z*tile.z))/tile.z;
          {
            if(drawOnlyOneImage == 1){
              if(uint(nuv.z *tile.z) == selectedView || uint(nuv.z *tile.z) == 19)
                  rgb[i] = texture(screenTex, texArr(nuv));
              else
                rgb[i] = vec4(0);
            }else{
                rgb[i] = texture(screenTex, compensate(nuv));//texArr(nuv));
            }
          }
        }

          if(showQuilt == 0)
            fragColor = vec4(rgb[ri].r, rgb[1].g, rgb[bi].b, 1.0);
          else{
    if(showAsSequence == 0)
    { 
        vec4 borders =  getClampingBorders(texCoords.xy);
        vec2 finalCoords = clamp( quiltCompensation(texCoords.xy), borders.xy, borders.zw);
        //vec2 finalCoords = quiltCompensation(texCoords.xy);
        fragColor = texture(screenTex, finalCoords);
    }
    else{
        uint sel = min(selectedView,uint(tile.x*tile.y-1));
        fragColor = texture(screenTex, texCoords.xy/vec2(tile.xy) + vec2(vec2(1.f)/tile.xy)*vec2(sel%uint(tile.x),sel/uint(tile.x)));
    }
}
}
).";

auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
        vsSrc
        );
auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
        fsSrc
        );
auto prg = vars.reCreate<ge::gl::Program>("holoProgram",vs,fs);
//exit(1);
//prg->setNonexistingUniformWarning(false);
}

class Quilt{
    public:
        glm::uvec2 counts = glm::uvec2(5,9);
        glm::uvec2 baseRes = glm::uvec2(380,238);
        glm::uvec2 res = glm::uvec2(1024,512);
        std::shared_ptr<ge::gl::Framebuffer>fbo;
        std::shared_ptr<ge::gl::Texture>color;
        std::shared_ptr<ge::gl::Texture>depth;
        vars::Vars&vars;

        void createTextures(){
            if(notChanged(vars,"all",__FUNCTION__,{"quiltRender.texScale","quiltRender.texScaleAspect"}))return;
            float texScale = vars.getFloat("quiltRender.texScale"); 
            float texScaleAspect =  vars.getFloat("quiltRender.texScaleAspect");
            auto newRes = glm::uvec2(glm::vec2(baseRes) * texScale * glm::vec2(texScaleAspect,1.f));
            if(newRes == res)return;
            res = newRes;
            std::cerr << "reallocate quilt textures - " << res.x << " x " << res.y << std::endl;
            fbo = std::make_shared<ge::gl::Framebuffer>();
            color = std::make_shared<ge::gl::Texture>(GL_TEXTURE_2D,GL_RGB8,1,res.x*counts.x,res.y*counts.y);
            color->texParameteri(GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            color->texParameteri(GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            depth = std::make_shared<ge::gl::Texture>(GL_TEXTURE_RECTANGLE,GL_DEPTH24_STENCIL8,1,res.x*counts.x,res.y*counts.y);
            depth->texParameteri(GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            depth->texParameteri(GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            fbo->attachTexture(GL_COLOR_ATTACHMENT0,color);
            fbo->attachTexture(GL_DEPTH_ATTACHMENT,depth);
            GLenum buffers[] = {GL_COLOR_ATTACHMENT0};
            fbo->drawBuffers(1,buffers);
        }

        Quilt(vars::Vars&vars):vars(vars)
        {
            createTextures();
        }

        void draw(std::function<void(glm::mat4 const&view,glm::mat4 const&proj)>const&fce,glm::mat4 const&centerView,glm::mat4 const&centerProj){
            createTextures();
            GLint origViewport[4];
            ge::gl::glGetIntegerv(GL_VIEWPORT,origViewport);

            fbo->bind();
            ge::gl::glClearColor(0,0,0,1);
            ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

            float strength = vars.addOrGetFloat("DStrength",0.f);
            float viewCone = glm::radians<float>(vars.getFloat("quiltRender.viewCone"));
            
            auto currentTestID = vars.addOrGetUint32(varsPrefix+"current",0);
            auto currentTestItem = vars.getVector<TestCase>("testItems")[currentTestID];
            float defaultCone = glm::pi<float>()/6.0f;
            constexpr float MAX_CONE = glm::radians(85.0f);
            constexpr float MAX_FOCUS = 0.887;
            float tanCone = glm::tan(viewCone);
            float maxTanCone = glm::tan(MAX_CONE);
            const std::set<TestCase::TestCategory> cone{TestCase::CONE_BEST, TestCase::CONE_MAX, TestCase::CONE_MIN};         

            vars.reCreate<float>("quiltView.focus",0.056f);
            if(cone.find(currentTestItem.category) != cone.end())
            {
                viewCone = MAX_CONE*strength;
                if(currentTestItem.category == TestCase::CONE_MIN) 
                    viewCone = MAX_CONE*(1.0f-strength);
                
                tanCone = glm::tan(viewCone);
                float focus = glm::mix(0.0f, MAX_FOCUS, tanCone/maxTanCone);
                vars.reCreate<float>("quiltView.focus", focus); 
            }
            else if(currentTestItem.category == TestCase::DOF30)
            {
                vars.reCreate<float>("blurAmount", 10*strength);
                tanCone = glm::tan(glm::radians(30.0f));
            }
            else if(currentTestItem.category == TestCase::DOF60)
            {
                vars.reCreate<float>("blurAmount", 10*strength);
                tanCone = glm::tan(glm::radians(60.0f));
                float focus = glm::mix(0.0f, MAX_FOCUS, tanCone/maxTanCone);
                vars.reCreate<float>("quiltView.focus", focus); 
            }


            auto const numViews = counts.x * counts.y;
            float d = vars.addOrGetFloat("quiltRender.d",0.70f);
            float S = 0.5f*d*tanCone;
            //float tilt = d*aspect*glm::tan(vars.getFloat("camera.fovy")/2);
            auto& matrices = vars.getVector<glm::mat4>("cameraTransformations");
            matrices.clear();
            matrices.reserve(45);
            size_t counter = 0;
            for(size_t j=0;j<counts.y;++j)
                for(size_t i=0;i<counts.x;++i){
                    ge::gl::glViewport(i*(res.x),j*(res.y),res.x,res.y);

                    float currentViewLerp = 0.f;
                    if (numViews > 1)
                        currentViewLerp = (float)counter / (numViews - 1) - 0.5f;

                    glm::mat4 view = centerView;
                    glm::mat4 proj = centerProj;

                    float t = (float)counter / (float)(numViews - 1);

                    float s = S-2*t*S;
                    view[3][0] += s;
                    //proj[2][0] += s/tilt;//(tilt+vars.addOrGetFloat("DTilt",0.f)*counter*0.01f);

                    auto editedView = getTestMatrix(vars,counter,view);
                    auto deltaMatrix = view*glm::inverse(editedView);
                    matrices.push_back(glm::inverse(deltaMatrix));

                    fce(editedView,proj);
                    counter++;
                }
            fbo->unbind();
            ge::gl::glViewport(origViewport[0],origViewport[1],origViewport[2],origViewport[3]);
        }

        glm::mat4 rotateViewMatrix(glm::mat4 const&mat, float angle, glm::vec3 axis)
        {
            auto camPos = glm::inverse(mat)*glm::vec4(0.f,0.f,0.f,1.f);
            return  glm::translate(glm::mat4(1.f),+glm::vec3(camPos))*
                glm::rotate   (glm::mat4(1.f),angle,axis)*
                glm::translate(glm::mat4(1.f),-glm::vec3(camPos))*
                mat;
        }

        glm::mat4 getTestMatrix(vars::Vars&vars,uint32_t counter,glm::mat4 const&inView){
            auto currentTestID = vars.addOrGetUint32(varsPrefix+"current",0);
            auto currentTestItem = vars.getVector<TestCase>("testItems")[currentTestID];
            float       strength      =                     vars.addOrGetFloat ("DStrength"           ,0.f  );
            float       d             =                     vars.addOrGetFloat ("quiltRender.d"       ,0.7f );
            auto        view          = inView;

            addVarsLimitsF(vars,"quiltRender.d",0,400,0.01);

            uint32_t axis = (currentTestID%3)+1; 
            if(axis == 3) axis = 4;
            uint32_t freq = vars.addOrGetFloat ("DFreq",25.f);
            bool jagged    = (currentTestID/3)%2;
            bool translate = (currentTestID/6)%2; 
            float deform = 0.f;

            float COMPENSATION_COEF{3.0};
            if(vars.getString("bckgTexFileName") == "")
                COMPENSATION_COEF = 6.0;

            constexpr float JAGGED_COEF{0.2};
            constexpr float ALL_COEF{0.065};
            constexpr float Z_COEF{4.0};
            constexpr float TRANSLATE_COEF{2.5};
            constexpr float CIRCULAR_COEF{0.25};

            vars.reCreate<float>("imageZ",0.9);
            if(currentTestItem.category == TestCase::CAMERAS)
            {
                float halfStrength = strength*ALL_COEF;
                if(vars.getBool("compensation"))
                    halfStrength *= COMPENSATION_COEF;

                if(jagged)deform = JAGGED_COEF*halfStrength*(glm::sin((float)(counter)*freq/100.f*glm::pi<float>()*2.f))*0.5f; 
                else      deform = halfStrength*counter*0.05f;

                if(axis==4)
                    deform *= Z_COEF;

                if(translate){
                    vars.reCreate<float>("imageZ",0.93);
                    deform *= TRANSLATE_COEF;
                    for(int k=0;k<3;++k)
                        view[3][k] += ((axis>>k)&1)*deform;
                }else{
                    auto camPos = glm::inverse(view)*glm::vec4(0.f,0.f,0.f,1.f);
                    view = glm::translate(glm::mat4(1.f),+glm::vec3(camPos))*
                        glm::rotate   (glm::mat4(1.f),deform,glm::vec3((axis>>0)&1,(axis>>1)&1,(axis>>2)&1))*
                        glm::translate(glm::mat4(1.f),-glm::vec3(camPos))*
                        view;

                    view = rotateViewMatrix(view, deform, glm::vec3((axis>>0)&1,(axis>>1)&1,(axis>>2)&1));
                }
            }
            else if(currentTestItem.category == TestCase::CIRCULAR || currentTestItem.category == TestCase::CIRCULAR_INVERT)
            {
                vars.reCreate<float>("imageZ",0.93);
                const float r = 5;// 0.5f*d*glm::tan(viewCone)*2;
                glm::vec2 circleCenter{0,-3};
                float invert = (currentTestItem.category == TestCase::CIRCULAR_INVERT) ? 1 : -1.0;
                float t = (float)counter / (float)(counts.x * counts.y - 1);
                auto a = t*glm::half_pi<float>()-glm::pi<float>()*0.25;
                auto circleCoord = glm::vec2(circleCenter.x+r*glm::sin(a), (circleCenter.y+r*glm::cos(a))*invert);
                auto pos = glm::vec3(circleCoord.x,0.0, circleCoord.y);
                auto cPos = glm::vec3(circleCenter.x,0.0, circleCenter.y);
                if(invert > 0)
                {
                    cPos.z *= -1;
                    cPos = cPos+(pos-cPos)*2.0f;
                }
                auto circleView = glm::lookAt(pos, cPos, glm::vec3(0,1,0));                
                view = glm::mix(inView, circleView, strength*CIRCULAR_COEF); 

            }
            else
                return inView;
            return view;
        }
};

void drawHolo(vars::Vars&vars){
    loadTextures(vars);
    createHoloProgram(vars);

    if(vars.getBool("renderQuilt")){
        vars.get<Quilt>("quilt")->color->bind(0);
    }else{
        vars.get<std::shared_ptr<ge::gl::Texture>>("quiltTex")->get()->bind(0);
    }

    auto matrices = vars.getVector<glm::mat4>("cameraTransformations");
    vars.get<ge::gl::Program>("holoProgram")
        ->set1i ("showQuilt"       ,                vars.getBool       ("showQuilt"            ))
        ->set1i ("showAsSequence"  ,                vars.getBool       ("showAsSequence"       ))
        ->set1ui("selectedView"    ,                vars.getUint32     ("selectedView"         ))
        ->set1i ("showQuilt"       ,                vars.getBool       ("showQuilt"            ))
        ->set1f ("pitch"           ,                vars.getFloat      ("quiltView.pitch"      ))
        ->set1f ("tilt"            ,                vars.getFloat      ("quiltView.tilt"       ))
        ->set1f ("center"          ,                vars.getFloat      ("quiltView.center"     ))
        ->set1f ("invView"         ,                vars.getFloat      ("quiltView.invView"    ))
        ->set1f ("subp"            ,                vars.getFloat      ("quiltView.subp"       ))
        ->set1i ("ri"              ,                vars.getInt32      ("quiltView.ri"         ))
        ->set1i ("bi"              ,                vars.getInt32      ("quiltView.bi"         ))
        ->set4fv("tile"            ,glm::value_ptr(*vars.get<glm::vec4>("quiltView.tile"       )))
        ->set4fv("viewPortion"     ,glm::value_ptr(*vars.get<glm::vec4>("quiltView.viewPortion")))
        ->set1ui("drawOnlyOneImage",                vars.getBool       ("drawOnlyOneImage"     ))
        ->set1f ("focus"           ,                vars.getFloat      ("quiltView.focus"      ))
        ->set1f ("zoomAmount"           ,           vars.getFloat      ("quiltView.zoom"      ))
        ->set1f ("imageZ"           ,               vars.getFloat      ("imageZ"      ))
        ->set1ui("stride"          ,                vars.getUint32     ("quiltView.stride"     ))
        ->set1i("compensation"    ,                vars.getBool       ("compensation"         ))
        ->setMatrix4fv("cameraTransformations",     glm::value_ptr(*matrices.data()), matrices.size())
        ->setMatrix4fv("projection",  glm::value_ptr(vars.getReinterpret<basicCamera::CameraProjection>("projection")->getProjection()))
        ->use();

    ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

void saveResults(vars::Vars&vars, const std::vector<TestCase> &items)
{
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    std::string fileName{asctime(timeinfo)};
    fileName.pop_back();
    if(vars.getBool("moveHead"))
        fileName += "_moving";
    else
        fileName += "_static";

    if(vars.getString("bckgTexFileName") == "")
        fileName += "_noBackground";

    std::ofstream f(vars.getString("resultDir")+fileName+".csv");

    for(const auto &item : items)
        f << item.name << ",";
    f << std::endl;
    for(const auto &item : items)
        f << vars.getFloat(varsPrefix+item.name) << ",";
    f << std::endl;
    for(const auto &item : items)
        f << *vars.get<double>(varsPrefix+item.name+"Time") << ",";
    f.close();
}

void changeColor(int i)
{
    glm::vec3 color(0.0f);
    color[i] = 1.0f;     
    std::vector<ImGuiCol_> imCols{ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
    float step = 1.0f/imCols.size();
    float current = 0.0f;
    for(auto const& imCol : imCols)
    {
       current +=step;
       auto newColor = color*current; 
       ImGui::GetStyle().Colors[imCol] = ImVec4(newColor.x, newColor.y, newColor.z, 1.0f);
    }
}

void drawTestingGui(vars::Vars&vars)
{
    auto timer = vars.addOrGet<Timer<double>>("timer");
    auto resetTimer = vars.get<bool>("resetTimer");
    if(*resetTimer)
    {
        timer->reset();
        *resetTimer = false;
    }
    const std::vector<std::string> labels{"Move the slider to the highest (righmost) value which is still producing visually acceptable and pleasant result.",
        "Move the slider wherever you need to achieve the nicest result for you.",
        "Turn on or off the effect (checkbox) and leave it at the state which looks better."};
    auto &items = vars.getVector<TestCase>("testItems");
    auto slider{vars.addOrGet<float>("DStrength",0.f)};
    auto &currentID{vars.getUint32(varsPrefix+"current")};
    auto currentItem{items[currentID]};
    auto prefixedName = varsPrefix+currentItem.name;
    auto &currentVar{vars.addOrGetFloat(prefixedName,0)};

    ImGui::Begin("Testing");

    changeColor(currentItem.type);
    ImGui::TextWrapped("%s", (labels[currentItem.type].c_str()));
        
    if(currentItem.type == TestCase::MAX_SLIDER || currentItem.type == TestCase::BEST_SLIDER)
      ImGui::SliderFloat("Slider", slider, 0, 1.0, "%.6f");
    else
        ImGui::Checkbox("Enable effect", reinterpret_cast<bool*>(slider));

    if (ImGui::Button("Next"))
    {
        currentVar = *slider;
        *slider = 0;
        auto t = vars.reCreate<double>(prefixedName+"Time", vars.get<Timer<double>>("timer")->elapsedFromStart());
        *resetTimer = true;    
        currentID++;
        vars.reCreate<bool>("compensation", items[currentID].compensate);

        std::cout << currentItem.name << " " << currentVar << " " << *t << std::endl;

        if(currentID > items.size()-1)
        {
            saveResults(vars, items);
            (*vars.get<Holo*>("thisApp"))->stop();
        }
    }
    std::string headInfo{"Please do not move your head while performing this task!"};
    if(vars.getBool("moveHead"))
        headInfo = "Please move your head around to see the scene from all angles!";
    ImGui::TextWrapped("%s", (headInfo.c_str()));
    ImGui::End();       
}

void Holo::draw(){
    ge::gl::glClear(GL_DEPTH_BUFFER_BIT);
    createCamera(vars);
    basicCamera::CameraTransform*view;

    if(vars.getBool("useOrbitCamera"))
        view = vars.getReinterpret<basicCamera::CameraTransform>("view");
    else{
        auto freeView = vars.get<basicCamera::FreeLookCamera>("view");

        float freeCameraSpeed = vars.addOrGetFloat("camera.speed",0.01f);
        auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
        /*
        for (int a = 0; a < 3; ++a)
            freeView->move(a, float((*keys)["d s"[a]] - (*keys)["acw"[a]]) *
                    freeCameraSpeed);
        */
        view = freeView;
    }

    ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
    ge::gl::glClear(GL_COLOR_BUFFER_BIT);

    vars.get<ge::gl::VertexArray>("emptyVao")->bind();

    auto drawScene = [&](glm::mat4 const&view,glm::mat4 const&proj){
        if(vars.addOrGetBool("drawGrid",false)){
            vars.get<ge::gl::VertexArray>("emptyVao")->bind();
            drawGrid(vars,view,proj);
            vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
        }
        drawModel(vars,view,proj);
    };
    auto drawSceneSimple = [&](){
        auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
        auto proj = vars.getReinterpret<basicCamera::CameraProjection>("projection")->getProjection();
        drawScene(view,proj);
    };
    if(vars.getBool("renderScene")){
        drawSceneSimple();
    }
    else{
        auto quilt = vars.get<Quilt>("quilt");
        vars.get<ge::gl::VertexArray>("emptyVao")->bind();
        quilt->draw(
                drawScene,
                vars.getReinterpret<basicCamera::CameraTransform>("view")->getView(),
                vars.getReinterpret<basicCamera::CameraProjection>("projection")->getProjection()
                );
        vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
        vars.get<ge::gl::VertexArray>("emptyVao")->bind();
        drawHolo(vars);
        vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
    }

    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

    auto debugScreen = vars.getVector<float>("debugScreen");
    if(debugScreen[0] != -1)
    {
        int test = static_cast<int>(debugScreen[0]);
        std::stringstream name;
        name << std::setw(2) << std::setfill('0') << test;
        auto &items = vars.getVector<TestCase>("testItems");
        name << "-" << debugScreen [1] << "_" << items[test].name << ".bmp";
        screenShot(name.str(),window->getWidth(), window->getHeight());
        (*vars.get<Holo*>("thisApp"))->stop();        
    }

    drawTestingGui(vars);
    drawImguiVars(vars);

    swap();
}

void Holo::init(){
    auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
    auto const windowSize = args->getu32v("--window-size",{1024,1024});
    auto const notResizable = args->isPresent("--notResizable");

    auto const debugScreen = args->getf32v("--debug",{-1,-1});

    auto const quiltFile = args->gets("--quilt","","quilt image 5x9");
    auto const modelFile = args->gets("--model","","model file");
    auto const textureFile = args->gets("--texture","","texture file");
    auto const backgroundFile = args->gets("--bckg","","background file");
    auto const resultDir = args->gets("--outDir","","where to save results");
    auto const moveHead = args->isPresent("--moveHead","user is allowed to move head");
    auto const showHelp = args->isPresent("-h","shows help");
    if (showHelp || !args->validate()) {
        std::cerr << args->toStr();
        exit(0);
    }

    vars.addVector<float>("debugScreen", debugScreen);
    uint32_t start=0;
    bool showQuilt{false};
    size_t w=windowSize[0];
    size_t h=windowSize[1];
    if(debugScreen[0] != -1)
    {
        start=static_cast<uint32_t>(debugScreen[0]);
        vars.add<float>("DStrength",debugScreen[1]);
        w = 1920;
        h = 1080;
        showQuilt = true;
    }
    vars.addUint32(varsPrefix+"current", start);

    if(notResizable)
        SDL_SetWindowResizable(window->getWindow(),SDL_FALSE);
    window->setSize(w,h);

    vars.add<Holo*>("thisApp", this);

    vars.addString("quiltTexFileName",quiltFile);
    vars.addString("bckgTexFileName",backgroundFile);
    vars.addString("modelFileName",modelFile);
    vars.addString("modelTexFileName",textureFile);
    vars.addString("resultDir",resultDir);
    vars.addBool("moveHead", moveHead);

    vars.add<ge::gl::VertexArray>("emptyVao");
    vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
    vars.addFloat("input.sensitivity",0.01f);
    vars.addFloat("camera.fovy",0.9f);//glm::half_pi<float>());
    vars.addFloat("camera.near",.1f);
    vars.addFloat("camera.far",1000.f);
    vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
    vars.addBool("useOrbitCamera",false);
    vars.addFloat("blurAmount", 0.0);

    vars.addBool("resetTimer", true);

    HoloCalibration::Calibration cal = HoloCalibration::getCalibration();
    vars.addFloat      ("quiltView.pitch"      ,cal.recalculatedPitch());
    vars.addFloat      ("quiltView.tilt"       ,cal.tilt());
    vars.addFloat      ("quiltView.center"     ,cal.center);
    vars.addFloat      ("quiltView.invView"    ,cal.invView);
    vars.addFloat      ("quiltView.subp"       ,cal.subp());
    vars.addInt32      ("quiltView.ri"         ,0);
    vars.addInt32      ("quiltView.bi"         ,2);
    vars.add<glm::vec4>("quiltView.tile"       ,5.00f, 9.00f, 45.00f, 45.00f);
    vars.add<glm::vec4>("quiltView.viewPortion",0.99976f, 0.99976f, 0.00f, 0.00f);
    vars.addFloat      ("quiltView.focus"      ,0.056f);
    vars.addFloat      ("quiltView.zoom"      ,0.5f);
    vars.addUint32     ("quiltView.stride"     ,1);
    vars.addFloat      ("imageZ"       ,0.9);
    addVarsLimitsF(vars,"quiltView.focus",-1,+1,0.001f);
    vars.addBool ("showQuilt", showQuilt);
    vars.addBool ("compensation");
    vars.addBool ("renderQuilt", true);
    vars.addBool ("renderScene",false);
    vars.addBool ("showAsSequence",false);
    vars.addBool ("drawOnlyOneImage",false);
    vars.addUint32("selectedView",0);
    addVarsLimitsU(vars,"selectedView",0,44);
    addVarsLimitsF(vars,"quiltView.tilt",-10,10,0.01);
    vars.addVector<glm::mat4>("cameraTransformations");

    vars.addFloat("quiltRender.size",5.f);
    vars.addFloat("quiltRender.fov",90.f);
    vars.addFloat("quiltRender.viewCone",35.f);
    vars.addFloat("quiltRender.texScale",3.2f);
    addVarsLimitsF(vars,"quiltRender.texScale",0.1f,5,0.01f);
    vars.addFloat("quiltRender.texScaleAspect",0.745f);
    addVarsLimitsF(vars,"quiltRender.texScaleAspect",0.1f,10,0.01f);
    
    vars.add<Quilt>("quilt",vars);

    createCamera(vars);

    auto &items = vars.addVector<TestCase>("testItems");
    items.insert(items.end(), { 
            {"YawNoiseLinear", TestCase::MAX_SLIDER},
            {"PitchNoiseLinear", TestCase::MAX_SLIDER},
            {"RollNoiseLinear", TestCase::MAX_SLIDER},
            {"YawNoiseJagged", TestCase::MAX_SLIDER},
            {"PitchNoiseJagged", TestCase::MAX_SLIDER},
            {"RollJagged", TestCase::MAX_SLIDER},
            {"HorizontalNoiseLinear", TestCase::MAX_SLIDER},
            {"VerticalNoiseLinear", TestCase::MAX_SLIDER},
            {"ZoomNoiseLinear", TestCase::MAX_SLIDER},
            {"HorizontalNoiseJagged", TestCase::MAX_SLIDER},
            {"VerticalNoiseJagged", TestCase::MAX_SLIDER},
            {"ZoomJagged", TestCase::MAX_SLIDER},
            });
    auto compItems = items;
    for(auto& item : compItems)
    {
        item.name += "Compensated"; 
        item.compensate = true;
    }
    items.reserve(compItems.size());
    items.insert(items.end(), compItems.begin(), compItems.end()); 
    items.insert(items.end(), 
            {  
            {"Circular", TestCase::MAX_SLIDER, TestCase::CIRCULAR},
            {"CircularInvert", TestCase::MAX_SLIDER, TestCase::CIRCULAR_INVERT},
            {"CircularCompensated", TestCase::MAX_SLIDER, TestCase::CIRCULAR, true},
            {"CircularInvertCompensated", TestCase::MAX_SLIDER, TestCase::CIRCULAR_INVERT, true},
            {"3DEffectMax", TestCase::MAX_SLIDER, TestCase::CONE_MAX},
            {"3DEffectMin", TestCase::MAX_SLIDER, TestCase::CONE_MIN},
            {"3DEffectBest", TestCase::BEST_SLIDER, TestCase::CONE_BEST},
            {"DoF30", TestCase::BEST_SLIDER, TestCase::DOF30},
            {"DoF60", TestCase::BEST_SLIDER, TestCase::DOF60},
            });

    if(vars.getString("bckgTexFileName") == "")
    {
        items.pop_back();
        items.pop_back();
    }

    GLint dims[4];
    ge::gl::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
    std::cerr << "maxFramebuffer: " << dims[0] << " x " << dims[1] << std::endl;

    ImGui::GetStyle().ScaleAllSizes(4.5f);
    ImGui::GetIO().FontGlobalScale = 4.5f;
}

void Holo::key(SDL_Event const& event, bool DOWN) {
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    (*keys)[event.key.keysym.sym] = DOWN;
    if(event.key.keysym.sym == SDLK_f && DOWN){
        fullscreen = !fullscreen;
        if(fullscreen)
            window->setFullscreen(sdl2cpp::Window::FULLSCREEN_DESKTOP);
        else
            window->setFullscreen(sdl2cpp::Window::WINDOW);
    }
    if(event.key.keysym.sym == SDLK_p && DOWN){
        auto const windowSize     = vars.get<glm::uvec2>("windowSize");
        screenShot("screenshot",windowSize->x,windowSize->y);
    }
    if(event.key.keysym.sym == SDLK_ESCAPE && DOWN)
        (*vars.get<Holo*>("thisApp"))->stop();
}

void Holo::mouseMove(SDL_Event const& e) {
    return;
    if(vars.getBool("useOrbitCamera")){
        auto sensitivity = vars.getFloat("input.sensitivity");
        auto orbitCamera =
            vars.getReinterpret<basicCamera::OrbitCamera>("view");
        auto const windowSize     = vars.get<glm::uvec2>("windowSize");
        auto const orbitZoomSpeed = 0.1f;//vars.getFloat("args.camera.orbitZoomSpeed");
        auto const xrel           = static_cast<float>(e.motion.xrel);
        auto const yrel           = static_cast<float>(e.motion.yrel);
        auto const mState         = e.motion.state;
        if (mState & SDL_BUTTON_LMASK) {
            if (orbitCamera) {
                orbitCamera->addXAngle(yrel * sensitivity);
                orbitCamera->addYAngle(xrel * sensitivity);
            }
        }
        if (mState & SDL_BUTTON_RMASK) {
            if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
        }
        if (mState & SDL_BUTTON_MMASK) {
            orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
                    float(windowSize->x) * 2.f);
            orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                    float(windowSize->y) * 2.f);
        }
    }else{
        auto const xrel           = static_cast<float>(e.motion.xrel);
        auto const yrel           = static_cast<float>(e.motion.yrel);
        auto view = vars.get<basicCamera::FreeLookCamera>("view");
        auto sensitivity = vars.getFloat("input.sensitivity");
        if (e.motion.state & SDL_BUTTON_LMASK) {
            view->setAngle(
                    1, view->getAngle(1) + xrel * sensitivity);
            view->setAngle(
                    0, view->getAngle(0) + yrel * sensitivity);
        }
    }
}


void Holo::resize(uint32_t x,uint32_t y){
    auto windowSize = vars.get<glm::uvec2>("windowSize");
    windowSize->x = x;
    windowSize->y = y;
    vars.updateTicks("windowSize");
    ge::gl::glViewport(0,0,x,y);
    std::cerr << "resize(" << x << "," << y << ")" << std::endl;
}


int main(int argc,char*argv[]){
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
    Holo app{argc, argv};
    app.start();
    return EXIT_SUCCESS;
}
