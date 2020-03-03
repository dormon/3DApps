#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <imguiVars/addVarsLimits.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <drawGrid.h>
#include <FreeImagePlus.h>
#include <imguiDormon/imgui.h>
#include <drawBunny.h>


#include<assimp/cimport.h>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

#include<geGL/geGL.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_access.hpp>


#define ___ std::cerr << __FILE__ << " " << __LINE__ << std::endl

class Holo: public simple3DApp::Application{
 public:
  Holo(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~Holo(){}
  virtual void draw() override;

  vars::Vars vars;
  bool fullscreen = false;

  virtual void                init() override;
  virtual void                resize(uint32_t x,uint32_t y) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                mouseMove(SDL_Event const& event) override;
};


class Model{
  public:
    aiScene const*model = nullptr;
    Model(std::string const&name);
    virtual ~Model();
	std::vector<float> getVertices() const;

	std::string getName() const { return name; };

protected:
	void generateVertices();
	std::vector<float> vertices;

	std::string name;
};

class RenderModel: public ge::gl::Context{
  public:
    RenderModel(Model*mdl);
    ~RenderModel();
    void draw(glm::mat4 const&view,glm::mat4 const&projection);
    std::shared_ptr<ge::gl::VertexArray>vao           = nullptr;
    std::shared_ptr<ge::gl::Buffer     >vertices      = nullptr;
    std::shared_ptr<ge::gl::Buffer     >normals       = nullptr;
    std::shared_ptr<ge::gl::Buffer     >indices       = nullptr;
    std::shared_ptr<ge::gl::Buffer     >indexVertices = nullptr;
    std::shared_ptr<ge::gl::Program    >program       = nullptr;
    uint32_t nofVertices = 0;
};

Model::Model(std::string const&fileName)
{
	model = aiImportFile(fileName.c_str(),aiProcess_Triangulate|aiProcess_GenNormals|aiProcess_SortByPType);

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

Model::~Model(){
  assert(this!=nullptr);
  if(this->model)aiReleaseImport(this->model);
}

std::vector<float> Model::getVertices() const
{
	return vertices;
}

void Model::generateVertices(){
  size_t nofVertices = 0;
  for(size_t i=0;i<model->mNumMeshes;++i)
    nofVertices+=model->mMeshes[i]->mNumFaces*3;
  vertices.reserve(nofVertices*3);
  for(size_t i=0;i<model->mNumMeshes;++i){
    auto mesh = model->mMeshes[i];
    for(size_t j=0;j<mesh->mNumFaces;++j)
      for(size_t k=0;k<3;++k)
        for(size_t l=0;l<3;++l)
          vertices.push_back(mesh->mVertices[mesh->mFaces[(uint32_t)j].mIndices[(uint32_t)k]][(uint32_t)l]);
  }
}

RenderModel::RenderModel(Model*mdl){
  assert(this!=nullptr);
  if(mdl==nullptr)
    std::cerr << "mdl is nullptr!" << std::endl;

  this->nofVertices = 0;
  auto model = mdl->model;
  for(size_t i=0;i<model->mNumMeshes;++i)
    this->nofVertices+=model->mMeshes[i]->mNumFaces*3;

  std::vector<float>vertData;
  vertData = mdl->getVertices();
  this->vertices = std::make_shared<ge::gl::Buffer>(this->nofVertices*sizeof(float)*3,vertData.data());

  std::vector<float>normData;
  normData.reserve(this->nofVertices*3);
  for(size_t i=0;i<model->mNumMeshes;++i){
    auto mesh = model->mMeshes[i];
    for(uint32_t j=0;j<mesh->mNumFaces;++j)
      for(uint32_t k=0;k<3;++k)
        for(uint32_t l=0;l<3;++l)
          normData.push_back(mesh->mNormals[mesh->mFaces[j].mIndices[k]][l]);
  }
  this->normals = std::make_shared<ge::gl::Buffer>(this->nofVertices*sizeof(float)*3,normData.data());


/*
  {
    std::vector<float>ver;
    for(size_t i=0;i<model->mNumMeshes;++i){
      auto mesh = model->mMeshes[i];
      for(size_t j=0;j<mesh->mNumVertices;++j){
        for(size_t k=0;k<3;++k)
          ver.push_back(mesh->mVertices[j][k]);
        for(size_t k=0;k<3;++k)
          ver.push_back(mesh->mNormals[j][k]);
      }
    }
    std::vector<uint32_t>ind;
    uint32_t offset=0;
    for(size_t i=0;i<model->mNumMeshes;++i){
      auto mesh = model->mMeshes[i];
      for(size_t j=0;j<mesh->mNumFaces;++j)
        for(size_t k=0;k<3;++k)
          ind.push_back(offset+mesh->mFaces[j].mIndices[k]);
      offset+=mesh->mNumFaces*3;
    }
    this->indexVertices = std::make_shared<ge::gl::Buffer>(ver.size()*sizeof(float),ver.data());
    this->indices = std::make_shared<ge::gl::Buffer>(ind.size()*sizeof(uint32_t),ind.data());
    this->vao = std::make_shared<ge::gl::VertexArray>();
    this->vao->addAttrib(this->indexVertices,0,3,GL_FLOAT,sizeof(float)*6,0);
    this->vao->addAttrib(this->indexVertices,1,3,GL_FLOAT,sizeof(float)*6,sizeof(float)*3);
    this->vao->addElementBuffer(this->indices);
    this->nofVertices = ind.size();
  }
*/
  //*
  this->vao = std::make_shared<ge::gl::VertexArray>();
  this->vao->addAttrib(this->vertices,0,3,GL_FLOAT);
  this->vao->addAttrib(this->normals,1,3,GL_FLOAT);
  // */

  const std::string vertSrc =
"#version 450 \n"
R".(
  uniform mat4 projection = mat4(1);
  uniform mat4 view       = mat4(1);

  layout(location=0)in vec3 position;
  layout(location=1)in vec3 normal;

  out vec3 vPosition;
  out vec3 vNormal;

  flat out uint vID;

  void main(){
    vID = gl_VertexID/3;
    gl_Position = projection*view*vec4(position,1);
    vPosition = position;
    vNormal   = normal;
  }).";
  const std::string fragSrc = 
"#version 450\n" 
R".(
  layout(location=0)out vec4 fColor;

  uniform vec4 lightPos = vec4(0,1000,0,1);

  uniform mat4 view = mat4(1);

  in vec3 vPosition;
  in vec3 vNormal;

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

    vec3  diffuseColor   = hue(vID*3.14159254f);//vec3(0.5,0.5,0.5);
    vec3  specularColor  = vec3(1);
    float specularFactor = 1;

    vec3 cameraPos = vec3(inverse(view)*vec4(0,0,0,1));

    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPos.xyz-vPosition);
    vec3 V = normalize(cameraPos-vPosition   );
    vec3 R = reflect(V,N);

    float dF = max(dot(L,N),0);
    float sF = max(dot(R,L),0)*dF;

    vec3 dl = dF * diffuseColor;
    vec3 sl = sF * specularColor;

    fColor = vec4(dl + sl,1);

  }).";
  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, vertSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, fragSrc);
  this->program = std::make_shared<ge::gl::Program>(vs,fs);
}

RenderModel::~RenderModel(){
  assert(this!=nullptr);
}


void RenderModel::draw(glm::mat4 const&view,glm::mat4 const&projection){
  assert(this!=nullptr);
  ge::gl::glEnable(GL_DEPTH_TEST);
  this->vao->bind();
  this->program->use();
  this->program->setMatrix4fv("projection",glm::value_ptr(projection));
  this->program->setMatrix4fv("view"      ,glm::value_ptr(view      ));
  //this->glDrawElements(GL_TRIANGLES,this->nofVertices,GL_UNSIGNED_INT,nullptr);
  this->glDrawArrays(GL_TRIANGLES,0,this->nofVertices);
  this->vao->unbind();
}

void preprareDrawModel(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"modelFileName"}))return;

  vars.add<Model          >("model"      ,vars.getString("modelFileName"));
  vars.add<RenderModel    >("renderModel",vars.get<Model>("model"));
}

void drawModel(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  preprareDrawModel(vars);

  auto rm = vars.get<RenderModel>("renderModel");
  rm->draw(view,proj);
}

void drawModel(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawModel(vars,view->getView(),projection->getProjection());
}


void createTrianglesProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  void main(){
  }
  ).";

  std::string const gsSrc = R".(

  layout(points)in;
  layout(triangle_strip,max_vertices=30)out;

  uniform mat4 projection;
  uniform mat4 view;
  out vec3 gPosition;
  out vec3 gNormal;
  out vec4 gColor;

  void genTriangle(vec3 a,vec3 b,vec3 c,vec3 color){
    vec3 n = normalize(cross(b-a,c-a));

    gPosition = a;
    gNormal = n;
    gColor = vec4(color,1);
    gl_Position = projection*view*vec4(a,1);
    EmitVertex();

    gPosition = b;
    gNormal = n;
    gColor = vec4(color,1);
    gl_Position = projection*view*vec4(b,1);
    EmitVertex();

    gPosition = c;
    gNormal = n;
    gColor = vec4(color,1);
    gl_Position = projection*view*vec4(c,1);
    EmitVertex();

    EndPrimitive();
  }

  void main(){
    genTriangle(vec3(-1,0,-1),vec3(+1,0,-1),vec3(+1,+1,-1),vec3(1,0,0));
    genTriangle(vec3(-3,0,-2),vec3(+3,0,-2),vec3(+3,+3,-2),vec3(0,1,0));
    genTriangle(vec3(-10,0,-4),vec3(+10,0,-4),vec3(+10,+10,-4),vec3(0,0,1));
    //gColor = vec4(0,0,0,1);
    //gl_Position = vec4(-1,-1,0,1);EmitVertex();
    //gl_Position = vec4(4,-1,0,1);EmitVertex();
    //gl_Position = vec4(-1,4,0,1);EmitVertex();
    //EndPrimitive();
  }

  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  in vec4 gColor;
  in vec3 gPosition;
  in vec3 gNormal;
  uniform mat4 view     = mat4(1);
  uniform vec3 lightPos = vec3(0,0,10);
  uniform uvec2 size = uvec2(1024,512);

  void main(){
    vec3 cameraPos = vec3(inverse(view)*vec4(0,0,0,1));
    vec3 N = normalize(gNormal);
    vec3 L = normalize(lightPos - gPosition);
    vec3 V = normalize(cameraPos - gPosition);
    vec3 R = -reflect(L,N);
    float df = abs(dot(N,L));
    float sf = pow(abs(dot(R,V)),100);
    fColor = gColor * df + vec4(sf);
    uvec2 pixSize = uvec2(1,1);
    //if(any(greaterThanEqual(gl_FragCoord.xy,size)))discard;
    uvec2 coord = uvec2(gl_FragCoord.xy/size);
    uint offset = 0;//coord.x + coord.y*5;

    //if((uint(gl_FragCoord.x)%size.x) >= 100+offset && (uint(gl_FragCoord.y)%size.y) >= 100 && (uint(gl_FragCoord.x)%size.x) < 100+pixSize.x+offset &&(uint(gl_FragCoord.y)%size.y) < 100+pixSize.y)
    //  fColor = vec4(0,1,0,1);
//    fColor = gColor;
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      vsSrc
      );
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,
      "#version 450\n",
      gsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("trianglesProgram",vs,gs,fs);

}

void drawTriangles(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  createTrianglesProgram(vars);
  ge::gl::glEnable(GL_DEPTH_TEST);
  vars.get<ge::gl::Program>("trianglesProgram")
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->use();
  ge::gl::glDrawArrays(GL_POINTS,0,1);
}

void drawTriangles(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  drawTriangles(vars,view->getView(),projection->getProjection());
}


void createView(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"useOrbitCamera"}))return;

  if(vars.getBool("useOrbitCamera"))
    vars.reCreate<basicCamera::OrbitCamera>("view");
  else
    vars.reCreate<basicCamera::FreeLookCamera>("view");
}

void createProjection(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize","camera.fovy","camera.near","camera.far"}))return;

  auto windowSize = vars.get<glm::uvec2>("windowSize");
  auto width = windowSize->x;
  auto height = windowSize->y;
  auto aspect = (float)width/(float)height;
  auto nearv = vars.getFloat("camera.near");
  auto farv  = vars.getFloat("camera.far" );
  auto fovy = vars.getFloat("camera.fovy");

  vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,nearv,farv);
}

void createCamera(vars::Vars&vars){
  createProjection(vars);
  createView(vars);
}

void create3DCursorProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  uniform mat4 projection = mat4(1);
  uniform mat4 view       = mat4(1);
  uniform mat4 origView   = mat4(1);
  uniform float distance = 10;
  out vec3 vColor;
  void main(){
    float size = 1;
    vColor = vec3(1,0,0);

    if(gl_VertexID == 0)gl_Position = projection * view * inverse(origView) * vec4(vec2(-10  ,+0   )*size,-distance,1);
    if(gl_VertexID == 1)gl_Position = projection * view * inverse(origView) * vec4(vec2(+10  ,+0   )*size,-distance,1);
    if(gl_VertexID == 2)gl_Position = projection * view * inverse(origView) * vec4(vec2(-10  ,+0.1 )*size,-distance,1);
    if(gl_VertexID == 3)gl_Position = projection * view * inverse(origView) * vec4(vec2(+0   ,-10  )*size,-distance,1);
    if(gl_VertexID == 4)gl_Position = projection * view * inverse(origView) * vec4(vec2(+0   ,+10  )*size,-distance,1);
    if(gl_VertexID == 5)gl_Position = projection * view * inverse(origView) * vec4(vec2(+0.1 ,-10  )*size,-distance,1);
  }
  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  in vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      vsSrc
      );
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
      "#version 450\n",
      fsSrc
      );
  vars.reCreate<ge::gl::Program>("cursorProgram",vs,fs);

}

void draw3DCursor(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){
  create3DCursorProgram(vars);
  auto origView = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  vars.get<ge::gl::Program>("cursorProgram")
    ->setMatrix4fv("projection",glm::value_ptr(proj))
    ->setMatrix4fv("view"      ,glm::value_ptr(view))
    ->setMatrix4fv("origView"  ,glm::value_ptr(origView))
    ->set1f       ("distance"  ,vars.getFloat("quiltRender.d"))
    ->use();
  ge::gl::glDrawArrays(GL_TRIANGLES,0,6);
}

void draw3DCursor(vars::Vars&vars){
  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
  draw3DCursor(vars,view->getView(),projection->getProjection());
}

void loadColorTexture(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"quiltFileName"}))return;
  fipImage colorImg;
  colorImg.load(vars.getString("quiltFileName").c_str());
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

  auto colorTex = vars.reCreate<ge::gl::Texture>(
      "quiltTex",GL_TEXTURE_2D,GL_RGB8,1,width,height);
  //ge::gl::glPixelStorei(GL_UNPACK_ROW_LENGTH,width);
  //ge::gl::glPixelStorei(GL_UNPACK_ALIGNMENT ,1    );
  ge::gl::glTextureSubImage2D(colorTex->getId(),0,0,0,width,height,format,type,data);
}


void loadTextures(vars::Vars&vars){
  loadColorTexture(vars);
}

void createHoloProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

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
  uniform int ri = 0;
  uniform int bi = 2;
  uniform vec4 tile = vec4(5,9,45,45);
  uniform vec4 viewPortion = vec4(0.99976f, 0.99976f, 0.00f, 0.00f);
  uniform vec4 aspect;
  uniform uint drawOnlyOneImage = 0;
  
  layout(binding=0)uniform sampler2D screenTex;
  
  uniform float focus = 0.f;

  uniform uint stride = 1;

  vec2 texArr(vec3 uvz)
  {
      // decide which section to take from based on the z.
 

      float z = floor(uvz.z * tile.z);
      float focusMod = focus*(1-2*clamp(z/tile.z,0,1));
      float x = (mod(z, tile.x) + clamp(uvz.x+focusMod,0,1)) / tile.x;
      float y = (floor(z / tile.x) + uvz.y) / tile.y;
      return vec2(x, y) * viewPortion.xy;
  }
  
  void main()
  {
  	vec3 nuv = vec3(texCoords.xy, 0.0);
  
  	vec4 rgb[3];
  	for (int i=0; i < 3; i++) 
  	{
  		nuv.z = (texCoords.x + i * subp + texCoords.y * tilt) * pitch - center;
  		//nuv.z = mod(nuv.z + ceil(abs(nuv.z)), 1.0);
  		//nuv.z = (1.0 - invView) * nuv.z + invView * (1.0 - nuv.z);
  		nuv.z = fract(nuv.z);
  		nuv.z = (1.0 - nuv.z);
      //if(stride >= 1 && stride <= 45 && (uint(nuv.z*45)%stride) != (uint(texCoords.y*1600)%stride)){
      //  rgb[i] = vec4(0);
      //}else
      nuv.z = (floor(nuv.z*tile.z/stride)*stride + fract(nuv.z*tile.z))/tile.z;
      {
        if(drawOnlyOneImage == 1){
          if(uint(nuv.z *tile.z) == selectedView || uint(nuv.z *tile.z) == 19)
  		      rgb[i] = texture(screenTex, texArr(nuv));
          else
            rgb[i] = vec4(0);
        }else{
  		    rgb[i] = texture(screenTex, texArr(nuv));
        }
  		  //rgb[i] = vec4(nuv.z, nuv.z, nuv.z, 1.0);
      }
  	}
  
      if(showQuilt == 0)
        fragColor = vec4(rgb[ri].r, rgb[1].g, rgb[bi].b, 1.0);
      else{
        if(showAsSequence == 0)
          fragColor = texture(screenTex, texCoords.xy);
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
  prg->setNonexistingUniformWarning(false);
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
    Quilt(vars::Vars&vars):vars(vars){
      createTextures();
    }
    void draw(std::function<void(glm::mat4 const&view,glm::mat4 const&proj)>const&fce,glm::mat4 const&centerView,glm::mat4 const&centerProj){
      createTextures();
      GLint origViewport[4];
      ge::gl::glGetIntegerv(GL_VIEWPORT,origViewport);

      fbo->bind();
      ge::gl::glClearColor(1,0,0,1);
      ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      size_t counter = 0;

      for(size_t j=0;j<counts.y;++j)
        for(size_t i=0;i<counts.x;++i){
          ge::gl::glViewport(i*(res.x),j*(res.y),res.x,res.y);


          float const fov = glm::radians<float>(vars.getFloat("quiltRender.fov"));
          float const size = vars.getFloat("quiltRender.size");
          float const camDist  =  size / glm::tan(fov * 0.5f); /// ?
          float const viewCone = glm::radians<float>(vars.getFloat("quiltRender.viewCone")); /// ?
          float const aspect = static_cast<float>(res.x) / static_cast<float>(res.y);
          float const viewConeSweep = -camDist * glm::tan(viewCone);
			    float const projModifier = 1.f / (size * aspect);
          auto const numViews = counts.x * counts.y;
          float currentViewLerp = 0.f; // if numviews is 1, take center view
          if (numViews > 1)
            currentViewLerp = (float)counter / (numViews - 1) - 0.5f;

          // .5*size*tan(cone)/tan(fov/2)
            // .5*tan(cone)/tan(fov/2)/aspect
          

          glm::mat4 view = centerView;
          glm::mat4 proj = centerProj;

          float t = (float)counter / (float)(numViews - 1);

          float d = vars.addOrGetFloat("quiltRender.d",0.70f);
          addVarsLimitsF(vars,"quiltRender.d",0,400,0.01);
          //float S = vars.addOrGetFloat("quiltRender.S",0.422f);
          //addVarsLimitsF(vars,"quiltRender.S",0,4,0.01);
          //view[3][0] += d - 2*d*t;
          //proj[2][0] += (d - 2*d*t)*S;
          
          float S = 0.5f*d*glm::tan(viewCone);
          float s = S-2*t*S;
          view[3][0] += s;
          proj[2][0] += s/(d*aspect*glm::tan(vars.getFloat("camera.fovy")/2));

          //std::cerr <<currentViewLerp * viewConeSweep << std::endl;
          //std::cerr << "view[3][0] " << view[3][0] << std::endl;
          //std::cerr << "proj[2][0] " << proj[2][0] << std::endl;
          //view[3][0] += currentViewLerp * viewConeSweep;
          //proj[2][0] += currentViewLerp * viewConeSweep * projModifier;

          fce(view,proj);
          counter++;
        }
      fbo->unbind();

      ge::gl::glViewport(origViewport[0],origViewport[1],origViewport[2],origViewport[3]);
    }
};

void drawHolo(vars::Vars&vars){
  loadTextures(vars);
  createHoloProgram(vars);

  if(vars.getBool("renderQuilt")){
    vars.get<Quilt>("quilt")->color->bind(0);
  }else{
    vars.get<ge::gl::Texture>("quiltTex")->bind(0);
  }
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
    ->set1ui("stride"          ,                vars.getUint32     ("quiltView.stride"     ))
    ->use();

  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
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
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float((*keys)["d s"[a]] - (*keys)["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }


  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  auto drawScene = [&](glm::mat4 const&view,glm::mat4 const&proj){
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    drawGrid(vars,view,proj);
    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
    //vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    //drawTriangles(vars,view,proj);
    //vars.get<ge::gl::VertexArray>("emptyVao")->bind();
    //drawBunny(vars,view,proj);
    drawModel(vars,view,proj);
    if(vars.addOrGetBool("quiltRender.drawCursor",true)){
      vars.get<ge::gl::VertexArray>("emptyVao")->bind();
      draw3DCursor(vars,view,proj);
      vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
    }
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


  drawImguiVars(vars);

  swap();
}

void Holo::init(){
  auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
  auto const quiltFile = args->gets("--quilt","","quilt image 5x9");
  auto const modelFile = args->gets("--model","","model file");
  auto const showHelp = args->isPresent("-h","shows help");
  if (showHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  vars.addString("quiltFileName",quiltFile);
  vars.addString("modelFileName",modelFile);
  
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("input.sensitivity",0.01f);
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addBool("useOrbitCamera",false);

  vars.addFloat      ("quiltView.pitch"      ,354.42108f);
  vars.addFloat      ("quiltView.tilt"       ,-0.1153f);
  vars.addFloat      ("quiltView.center"     ,0.04239f);
  vars.addFloat      ("quiltView.invView"    ,1.00f);
  vars.addFloat      ("quiltView.subp"       ,0.00013f);
  vars.addInt32      ("quiltView.ri"         ,0);
  vars.addInt32      ("quiltView.bi"         ,2);
  vars.add<glm::vec4>("quiltView.tile"       ,5.00f, 9.00f, 45.00f, 45.00f);
  vars.add<glm::vec4>("quiltView.viewPortion",0.99976f, 0.99976f, 0.00f, 0.00f);
  vars.addFloat      ("quiltView.focus"      ,0.00f);
  vars.addUint32     ("quiltView.stride"     ,1);
  addVarsLimitsF(vars,"quiltView.focus",-1,+1,0.001f);
  vars.addBool ("showQuilt");
  vars.addBool ("renderQuilt");
  vars.addBool ("renderScene",false);
  vars.addBool ("showAsSequence",false);
  vars.addBool ("drawOnlyOneImage",false);
  vars.addUint32("selectedView",0);
  addVarsLimitsU(vars,"selectedView",0,44);
  addVarsLimitsF(vars,"quiltView.tilt",-10,10,0.01);

  vars.addFloat("quiltRender.size",5.f);
  vars.addFloat("quiltRender.fov",90.f);
  vars.addFloat("quiltRender.viewCone",10.f);
  vars.addFloat("quiltRender.texScale",1.64f);
  addVarsLimitsF(vars,"quiltRender.texScale",0.1f,5,0.01f);
  vars.addFloat("quiltRender.texScaleAspect",0.745f);
  addVarsLimitsF(vars,"quiltRender.texScaleAspect",0.1f,10,0.01f);
  

  vars.add<Quilt>("quilt",vars);

  createCamera(vars);
  
  GLint dims[4];
  ge::gl::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
  std::cerr << "maxFramebuffer: " << dims[0] << " x " << dims[1] << std::endl;

  ImGui::GetStyle().ScaleAllSizes(4.f);
  ImGui::GetIO().FontGlobalScale = 4.f;
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
}

void Holo::mouseMove(SDL_Event const& e) {
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
