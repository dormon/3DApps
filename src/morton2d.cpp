#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars.h>
#include <addVarsLimits.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include<assimp/cimport.h>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

class AABB{
  public:
    AABB(std::vector<glm::vec2>const&p):AABB(){
      for(auto const&x:p)
        add(x);
    }
    AABB():minPoint(std::numeric_limits<float>::max()),maxPoint(-std::numeric_limits<float>::max()){}
    void add(glm::vec2 const&p){
      minPoint = glm::min(minPoint,p);
      maxPoint = glm::max(maxPoint,p);
    }
    void add(AABB const&p){
      add(p.minPoint);
      add(p.maxPoint);
    }
    float area()const{
      auto d = maxPoint-minPoint;
      return glm::abs(d.x*d.y);
    }
    glm::vec2 minPoint;
    glm::vec2 maxPoint;
};

std::vector<glm::vec2>getModelPoints(std::string const&n){
  auto model = aiImportFile(n.c_str(),aiProcess_Triangulate|aiProcess_GenNormals|aiProcess_SortByPType);
  std::vector<glm::vec2>vertices;
  size_t nofVertices = 0;
  for(size_t i=0;i<model->mNumMeshes;++i)
    nofVertices+=model->mMeshes[i]->mNumFaces*3;
  vertices.reserve(nofVertices*3);
  for(size_t i=0;i<model->mNumMeshes;++i){
    auto mesh = model->mMeshes[i];
    for(size_t j=0;j<mesh->mNumFaces;++j)
      for(size_t k=0;k<3;++k){
        auto x = mesh->mVertices[mesh->mFaces[(uint32_t)j].mIndices[(uint32_t)k]][(uint32_t)0];
        auto y = mesh->mVertices[mesh->mFaces[(uint32_t)j].mIndices[(uint32_t)k]][(uint32_t)1];
        auto z = mesh->mVertices[mesh->mFaces[(uint32_t)j].mIndices[(uint32_t)k]][(uint32_t)2];
        vertices.push_back(glm::vec2(x,y));
      }
  }
  return vertices;
}

float uniform(){
  return (float)rand() / (float)RAND_MAX;
}

float rangeRan(float mmin,float mmax){
  return uniform()*(mmax-mmin) + mmin;
}

std::vector<glm::vec2>getPoints(size_t n){
  std::vector<glm::vec2>res;
  for(size_t i=0;i<n;++i)
    res.push_back(glm::vec2(rangeRan(-1,+1),rangeRan(-1,+1)));
  return res;
}

std::vector<glm::vec2>normalize(std::vector<glm::vec2>const&p){
  auto aabb = AABB(p);
  std::vector<glm::vec2>r;
  for(auto const&x:p)
    r.push_back((x-aabb.minPoint)/(aabb.maxPoint-aabb.minPoint)*2.f-1.f);
  return r;
}



uint32_t requiredBits(uint32_t a){
  return (uint32_t)glm::ceil(glm::log2((float)a));
}

glm::uvec2 requiredBits(glm::uvec2 const&a){
  return glm::uvec2(requiredBits(a.x),requiredBits(a.y));
}

uint32_t morton(glm::vec2 const&p,AABB const&aabb,glm::uvec2 const& div){
  auto np = (p-aabb.minPoint) / (aabb.maxPoint - aabb.minPoint);
  auto inp = glm::clamp(glm::uvec2(np*glm::vec2(div)),glm::uvec2(0),div-glm::uvec2(1));
  uint32_t res = 0;
  auto const bits = requiredBits(div);
  for(size_t d=0;d<2;++d)
    for(size_t b=0;b<bits[d];++b){
      res |= ((inp[d]>>b)&1)<<(b*2+d);
    }
  return res;
}

class Space{
  public:
    Space(std::vector<glm::vec2>const&p,glm::uvec2 const&div):data(p),aabb(p){
      for(size_t i=0;i<data.size();++i){
        auto mor = morton(data.at(i),aabb,div);
        mortons.push_back(mor);
        morton2Id[mor] = i;
      }
      std::sort(std::begin(mortons),std::end(mortons));
      for(auto const&m:mortons)
        sortedIds.push_back(morton2Id.at(m));

      size_t n=mortons.size();
      for(size_t i=0;i<n;i+=4){
        AABB aabb;
        for(size_t j=0;j<4&&i+j<n;++j)
          aabb.add(data[sortedIds.at(i+j)]);
        hierarchy.push_back(aabb);
      }
      n = hierarchy.size();
      size_t offset = 0;
      while(n != 1){
        //std::cerr << "offset: " << offset << " - size: " << n << std::endl;
        offsets.push_back(offset);
        sizes.push_back(n);
        for(size_t i=0;i<n;i+=4){
          AABB aabb;
          for(size_t j=0;j<4&&i+j<n;++j)
            aabb.add(hierarchy[offset + i+j]);
          hierarchy.push_back(aabb);
        }
        offset += n;
        n = hierarchy.size() - offset;
      }
      area = 0;
      for(auto const&a:hierarchy)
        area += a.area();




      
    }
    float area;
    std::vector<uint32_t>offsets;
    std::vector<uint32_t>sizes;
    std::vector<glm::vec2>data;
    std::vector<uint32_t>mortons;
    std::map<uint32_t,uint32_t>morton2Id;
    std::vector<uint32_t>sortedIds;
    std::vector<AABB>hierarchy;

    AABB aabb;
};

void createProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  layout(binding=0)buffer Points{vec2 points[];};
  layout(binding=1)buffer SortedIDs{uint sortedIds[];};
  void main(){
    gl_Position = vec4(points[sortedIds[gl_VertexID]],1,1);
  }
  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  void main(){
    fColor = vec4(1);
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
  vars.reCreate<ge::gl::Program>("program",vs,fs);

}

void createAABBProgram(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  out flat uint vi;
  void main(){
    vi = gl_VertexID;
  }
  ).";

  std::string const gsSrc = R".(
  uniform uint offset = 0;
  layout(binding=0)buffer AABB{vec4 aabb[];};

  layout(points)in;
  layout(line_strip,max_vertices=5)out;

  in flat uint vi[];

  void main(){
    vec4 a = aabb[offset + vi[0]];

    gl_Position = vec4(a.x,a.y,1,1);EmitVertex();
    gl_Position = vec4(a.z,a.y,1,1);EmitVertex();
    gl_Position = vec4(a.z,a.w,1,1);EmitVertex();
    gl_Position = vec4(a.x,a.w,1,1);EmitVertex();
    gl_Position = vec4(a.x,a.y,1,1);EmitVertex();
    EndPrimitive();
  }
  
  ).";

  std::string const fsSrc = R".(
  out vec4 fColor;
  uniform vec3 color = vec3(1,0,0);
  void main(){
    fColor = vec4(color,1);
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
  vars.reCreate<ge::gl::Program>("aabbProgram",vs,gs,fs);

}


std::vector<glm::vec2>getUniform(size_t nofPoints){
  size_t NX = (size_t)glm::sqrt((float)nofPoints);
  size_t NY = (size_t)glm::ceil((float)nofPoints / (float)NX);
  size_t counter = 0;
  std::vector<glm::vec2>res;
  for(size_t y=0;y<NY;++y)
    for(size_t x=0;x<NX;++x){
      if(counter >= nofPoints){y=NY;break;}
      counter++;
      glm::vec2 p;
      p.x = 2*((float)x / (float)NX)-1;
      p.y = 2*((float)y / (float)NY)-1;
      res.push_back(p);
    }
  return res;
}


void createPoints(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"nofPoints","spaceSignal"}))return;
  auto&v = vars.reCreateVector<glm::vec2>("points");
  //v = getPoints(vars.getUint32("nofPoints"));
  v = getUniform(vars.getUint32("nofPoints"));
}


void createSpace(vars::Vars&vars){
  //auto p = normalize(getModelPoints("/media/devel/models/conference/conference.obj"));
  if(notChanged(vars,"all",__FUNCTION__,{"points","div"}))return;
  auto&p = vars.getVector<glm::vec2>("points");
  vars.getUint32("nofPoints") = p.size();
  auto space = vars.reCreate<Space>("space",p,*vars.get<glm::uvec2>("div"));
  auto buf = vars.reCreate<ge::gl::Buffer>("pointsBuffer",space->data);
  auto ids = vars.reCreate<ge::gl::Buffer>("sortedIdsBuffer",space->sortedIds);
  auto aabbs = vars.reCreate<ge::gl::Buffer>("hierarchyBuffer",space->hierarchy);
  vars.reCreate<float>("area"   ,space->area);
  vars.reCreate<float>("percent",space->area/4.f * 100);
}

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~EmptyProject(){}
  virtual void draw() override;

  vars::Vars vars;

  virtual void                init() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};




void EmptyProject::mouseMove(SDL_Event const& e) {
  auto const mState = e.motion.state;
  if (mState & SDL_BUTTON_LMASK) {
  }
  if (mState & SDL_BUTTON_RMASK) {
  }

}

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("scale",1.f);
  vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
  vars.addUint32("nofPoints",100);
  vars.add<glm::uvec2>("div",128,128);
  vars.addUint32("selectedAABB",0);
  vars.addUint32("spaceSignal");
  createPoints(vars);
  createSpace(vars);
}

void EmptyProject::draw(){
  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT);

  createProgram(vars);
  createPoints(vars);
  createSpace(vars);
  createAABBProgram(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  auto windowSize = vars.get<glm::uvec2>("windowSize");
  vars.get<ge::gl::Buffer>("pointsBuffer")->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  vars.get<ge::gl::Buffer>("sortedIdsBuffer")->bindBase(GL_SHADER_STORAGE_BUFFER,1);
  vars.get<ge::gl::Program>("program")
    ->use();
  if(vars.addOrGetBool("drawLines",false))
    ge::gl::glDrawArrays(GL_LINE_STRIP,0,vars.getUint32("nofPoints"));
  else
    ge::gl::glDrawArrays(GL_POINTS,0,vars.getUint32("nofPoints"));

  auto space = vars.get<Space>("space");
  uint32_t maxLevel = vars.addOrGetUint32("maxLevel",0);
  addVarsLimitsU(vars,"maxLevel",0,space->offsets.size()-1);
  uint32_t minLevel = vars.addOrGetUint32("minLevel",0);
  addVarsLimitsU(vars,"minLevel",0,maxLevel);
  vars.get<ge::gl::Buffer>("hierarchyBuffer")->bindBase(GL_SHADER_STORAGE_BUFFER,0);
  auto aabbProgram = vars.get<ge::gl::Program>("aabbProgram");
  aabbProgram
    ->set1ui("offset",space->offsets[maxLevel])
    ->use();
  if(vars.addOrGetBool("drawAABB",false)){
    if(vars.addOrGetBool("drawSelectedAABB",false)){
      for(size_t i=minLevel;i<=maxLevel;++i){
        aabbProgram->set1ui("offset",0);
        glm::vec3 colors[] = {
          glm::vec3(1,0,0),
          glm::vec3(0,1,0),
          glm::vec3(0,0,1),
          glm::vec3(1,1,0),
          glm::vec3(1,0,1),
          glm::vec3(0,1,1),
          glm::vec3(1,1,1),
        };
        aabbProgram->set3fv("color",glm::value_ptr(colors[i]));
        auto nofAABB = (uint32_t)glm::pow(4.f,(float)(maxLevel-i));
        ge::gl::glDrawArrays(GL_POINTS,space->offsets[i]+vars.addOrGetUint32("selectedAABB")*nofAABB,nofAABB);
      }
      addVarsLimitsU(vars,"selectedAABB",0,space->sizes[maxLevel]);
    }else{
     ge::gl::glDrawArrays(GL_POINTS,0,space->sizes[maxLevel]);
    }
  }

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  drawImguiVars(vars);

  swap();
}

void EmptyProject::key(SDL_Event const& event, bool DOWN) {
  auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
  (*keys)[event.key.keysym.sym] = DOWN;
  if(event.key.keysym.sym == SDLK_g && DOWN)
    vars.reCreate<uint32_t>("spaceSignal");
}

void EmptyProject::resize(uint32_t x,uint32_t y){
  auto windowSize = vars.get<glm::uvec2>("windowSize");
  windowSize->x = x;
  windowSize->y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}

int main(int argc,char*argv[]){
  EmptyProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
