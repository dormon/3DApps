#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include <SDL2CPP/MainLoop.h>
#include <SDL2CPP/Window.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <sstream>

using namespace ge::gl;
using namespace std;

template<typename RETURN,typename...ARGS>
class Barrier{
  public:
    using PTR = RETURN(*)(ARGS...);
    Barrier(PTR const&,RETURN && defRet):returnValue(defRet){}
    bool notChanged(ARGS ... args){
      auto newInputs = std::tuple<ARGS...>(args...);
      auto same = arguments == newInputs;
      if((!first) && same)return same;
      first = false;
      arguments = newInputs;
      return false;
    }
    bool first = true;
    RETURN             returnValue;
    std::tuple<ARGS...>arguments  ;
};

template<typename RETURN,typename...ARGS,typename VRET>
Barrier<RETURN,ARGS...>make_Barrier(RETURN(*ptr)(ARGS...),VRET && returnDef){
  return Barrier<RETURN,ARGS...>{ptr,static_cast<RETURN>(returnDef)};
}

shared_ptr<Program> getProgram(size_t vertices,int spacing,int drawMode){
  static auto barrier = make_Barrier(getProgram,nullptr);
  if(barrier.notChanged(vertices,spacing,drawMode))
    return barrier.returnValue;

  stringstream vsSrc;
  vsSrc << "#version                     " << 450                << endl;
  vsSrc << "#line                        " << __LINE__           << endl;
  vsSrc << "#define PATCH_VERTICES       " << vertices           << endl;
  vsSrc << R".(

  void main(){
    if(gl_VertexID == 0)gl_Position = vec4(-.5,-.5,0,1);
    if(gl_VertexID == 1)gl_Position = vec4(+.5,-.5,0,1);

    #if PATCH_VERTICES == 3
      if(gl_VertexID == 2)gl_Position = vec4(0,+.5,0,1);
    #else
      if(gl_VertexID == 2)gl_Position = vec4(+.5,+.5,0,1);
    #endif

    if(gl_VertexID == 3)gl_Position = vec4(-.5,+.5,0,1);
  }

  ).";
  auto vert = make_shared<Shader>(GL_VERTEX_SHADER,vsSrc.str());

  stringstream csSrc;
  csSrc << "#version                     " << 450                << endl;
  csSrc << "#line                        " << __LINE__           << endl;
  csSrc << "#define PATCH_VERTICES       " << vertices           << endl;
  csSrc << R".(

  // Number of vertices in output primitive
  // Number of invocation of control shader
  layout(vertices=PATCH_VERTICES)out;
  
  uniform vec2 inner = vec2(1,1);//tessellation inner level
  uniform vec4 outer = vec4(1,1,1,1);//tessellation outer level
  
  void main(){
  	gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;
  	if(gl_InvocationID==0){
  		gl_TessLevelOuter[0]=outer[0];
  		gl_TessLevelOuter[1]=outer[1];
  		gl_TessLevelOuter[2]=outer[2];
  		gl_TessLevelOuter[3]=outer[3];
  		gl_TessLevelInner[0]=inner[0];
  		gl_TessLevelInner[1]=inner[1];
  	}
  }

  ).";
  auto cont = make_shared<Shader>(GL_TESS_CONTROL_SHADER,csSrc.str());

  std::string const spacingNames[] = {
      "equal_spacing", "fractional_even_spacing", "fractional_odd_spacing"
  };
  std::string const primitiveNames[] = {
    "isolines","triangles","quads"
  };
  stringstream evSrc;
  evSrc << "#version                     " << 450                        << endl;
  evSrc << "#line                        " << __LINE__                   << endl;
  evSrc << "#define PATCH_VERTICES       " << vertices                   << endl;
  evSrc << "#define SPACING              " << spacingNames[spacing]      << endl;
  evSrc << "#define PRIMITIVE            " << primitiveNames[vertices-2] << endl;
  evSrc << R".(

  layout(PRIMITIVE,SPACING)in;
  
  void main(){

  #if PATCH_VERTICES == 2
	  gl_Position=mix(gl_in[0].gl_Position,gl_in[1].gl_Position,gl_TessCoord.x)+vec4(0,gl_TessCoord.y,0,0);
  #elif PATCH_VERTICES == 3
	  gl_Position=
		  gl_in[0].gl_Position*gl_TessCoord.x+
		  gl_in[1].gl_Position*gl_TessCoord.y+
		  gl_in[2].gl_Position*gl_TessCoord.z;
  #else
  	vec4 A=mix(gl_in[0].gl_Position,gl_in[1].gl_Position,gl_TessCoord.x);
  	vec4 B=mix(gl_in[3].gl_Position,gl_in[2].gl_Position,gl_TessCoord.x);
  	gl_Position=mix(A,B,gl_TessCoord.y);
  #endif
  }

  ).";
  auto eval = make_shared<Shader>(GL_TESS_EVALUATION_SHADER,evSrc.str());

  stringstream gsSrc;
  gsSrc << "#version                     " << 450                << endl;
  gsSrc << "#line                        " << __LINE__           << endl;
  gsSrc << "#define PATCH_VERTICES       " << vertices           << endl;
  gsSrc << "#define DRAW_MODE            " << drawMode           << endl;
  gsSrc << R".(

  #if PATCH_VERTICES < 3
  layout(lines)in;
  #else
  layout(triangles)in;
  #endif

  #if DRAW_MODE == 0
  layout(triangle_strip,max_vertices=3)out;
  #endif
  #if DRAW_MODE == 1
  layout(line_strip,max_vertices=4)out;
  #endif
  #if DRAW_MODE == 2
  layout(points,max_vertices=3)out;
  #endif

  void main(){
    #if PATCH_VERTICES > 2
    #if DRAW_MODE == 1
    gl_Position = gl_in[0].gl_Position;EmitVertex();
    gl_Position = gl_in[1].gl_Position;EmitVertex();
    gl_Position = gl_in[2].gl_Position;EmitVertex();
    gl_Position = gl_in[0].gl_Position;EmitVertex();
    #else
    gl_Position = gl_in[0].gl_Position;EmitVertex();
    gl_Position = gl_in[1].gl_Position;EmitVertex();
    gl_Position = gl_in[2].gl_Position;EmitVertex();
    #endif
    #else
    gl_Position = gl_in[0].gl_Position;EmitVertex();
    gl_Position = gl_in[1].gl_Position;EmitVertex();
    #endif
    EndPrimitive();
  }

  ).";
  auto geom = make_shared<Shader>(GL_GEOMETRY_SHADER,gsSrc.str());

  stringstream fsSrc;
  fsSrc << "#version                     " << 450                << endl;
  fsSrc << "#line                        " << __LINE__           << endl;
  fsSrc << R".(
  
  out vec4 fColor;

  void main(){
    fColor = vec4(1);
  }

  ).";
  auto frag = make_shared<Shader>(GL_FRAGMENT_SHADER,fsSrc.str());
  barrier.returnValue = make_shared<Program>(vert,cont,eval,geom,frag);
  return barrier.returnValue;
}

int main(int argc,char*argv[]){
  auto mainLoop = make_shared<sdl2cpp::MainLoop>();
  auto window   = make_shared<sdl2cpp::Window  >();
  window->createContext("rendering");
  ge::gl::init();
  mainLoop->addWindow("mainWindow",window);
  auto imgui = std::make_unique<imguiSDL2OpenGL::Imgui>(window->getWindow());
  mainLoop->setEventHandler([&](SDL_Event const&event){
    return imgui->processEvent(&event);
  });

  float innerLevel[2] = {1.f,1.f};
  float outerLevel[4] = {1.f,1.f,1.f,1.f};
  int spacing = 0;
  int drawMode = 0;
  int primitive = 2;
  float minLevel = 0.f;
  float maxLevel = 64.f;

  auto vao = make_shared<VertexArray>();
  mainLoop->setIdleCallback([&]{
    glClearColor(0.f,0.1f,0.f,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    imgui->newFrame(window->getWindow());

    vao->bind();
    auto const vertices = primitive+2;
    auto program = getProgram(vertices,spacing,drawMode);
    program
      ->set2fv("inner",innerLevel)
      ->set4fv("outer",outerLevel)
      ->use();
	  glPatchParameteri(GL_PATCH_VERTICES,vertices);
    glPointSize(4);
    glDrawArrays(GL_PATCHES,0,vertices);
    vao->unbind();
    ImGui::Begin("vars");
    ImGui::PushItemWidth(-90);
    ImGui::LabelText("label", "Value");
    ImGui::DragScalar("Inner0"  ,ImGuiDataType_Float,&innerLevel[0],0.1f,&minLevel   ,&maxLevel);
    ImGui::DragScalar("Inner1"  ,ImGuiDataType_Float,&innerLevel[1],0.1f,&minLevel   ,&maxLevel);
    ImGui::DragScalar("Outer0"  ,ImGuiDataType_Float,&outerLevel[0],0.1f,&minLevel   ,&maxLevel);
    ImGui::DragScalar("Outer1"  ,ImGuiDataType_Float,&outerLevel[1],0.1f,&minLevel   ,&maxLevel);
    ImGui::DragScalar("Outer2"  ,ImGuiDataType_Float,&outerLevel[2],0.1f,&minLevel   ,&maxLevel);
    ImGui::DragScalar("Outer3"  ,ImGuiDataType_Float,&outerLevel[3],0.1f,&minLevel   ,&maxLevel);
    char const* primitiveNames[] = {"isolines","triangles","quads"};
    ImGui::ListBox("primitive", &primitive, primitiveNames, IM_ARRAYSIZE(primitiveNames), 4);
    const char* spacingNames[] = { "equal_spacing", "fractional_even_spacing", "fractional_odd_spacing"};
    ImGui::ListBox("spacing type", &spacing, spacingNames, IM_ARRAYSIZE(spacingNames), 4);
    const char* drawModes[] = { "fill", "line", "point"};
    ImGui::ListBox("draw mode", &drawMode, drawModes, IM_ARRAYSIZE(drawModes), 4);
    ImGui::End();
    imgui->render(window->getWindow(), window->getContext("rendering"));
    window->swap();
  });
  (*mainLoop)();
  imgui = nullptr;
  return 0;
}
