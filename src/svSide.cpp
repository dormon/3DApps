#include <Simple3DApp/Application.h>
#include <Vars/Vars.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/FreeLookCamera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <imguiVars/imguiVars.h>
#include <drawGrid.h>
#include <imguiVars/addVarsLimits.h>
#include <VarsGLMDecorator/VarsGLMDecorator.h>


std::string const doesLineInterectSubFrustum = R".(
bool doesLineInterectSubFrustum(in vec4 A,in vec4 B,in vec3 minCorner,in vec3 maxCorner){
  float tt[2] = {0.f,1.f};
  float M;
  float N;
  uint doMin;

  #define MINIMIZE()\
  M/=N;\
  doMin = uint(N<0.f);\
  N=(tt[doMin]-M)*(-1.f+2.f*doMin);\
  tt[doMin] = float(N<0)*tt[doMin] + float(N>=0)*M
  
  M = +A.w*minCorner[0]-A[0];
  N = +B[0]-A[0]-(B.w-A.w)*minCorner[0];
  MINIMIZE();
  M = +A.w*minCorner[1]-A[1];
  N = +B[1]-A[1]-(B.w-A.w)*minCorner[1];
  MINIMIZE();
  M = +A.w*minCorner[2]-A[2];
  N = +B[2]-A[2]-(B.w-A.w)*minCorner[2];
  MINIMIZE();

  M = -A.w*maxCorner[0]+A[0];
  N = -B[0]+A[0]+(B.w-A.w)*maxCorner[0];
  MINIMIZE();
  M = -A.w*maxCorner[1]+A[1];
  N = -B[1]+A[1]+(B.w-A.w)*maxCorner[1];
  MINIMIZE();
  M = -A.w*maxCorner[2]+A[2];
  N = -B[2]+A[2]+(B.w-A.w)*maxCorner[2];
  MINIMIZE();
  
#undef MINIMIZE
  return tt[0] <= tt[1];
}

).";

using DVars = VarsGLMDecorator<vars::Vars>;

void prepareDrawPointCloud(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  uniform float ax = 0.f;
  uniform float bx = 1.f;
  uniform float ay = 0.f;
  uniform float by = 1.f;
  uniform float az = 1.f;
  uniform float bz = 10.f;

  uniform vec3 A = vec3(0,0,0);
  uniform vec3 B = vec3(1,0,0);
  uniform vec4 light = vec4(10,10,10,1);

  uniform vec3 SM;
  uniform vec3 SN;

  out vec3 vColor;

  vec4 movePointToSubfrustum(
      in vec4 point    ,
      in vec2 leftRight,
      in vec2 bottomTop,
      in vec2 nearFar  ){
    vec4 result;
    result.x = (point.x + -point.w * ( -1 + leftRight.y + leftRight.x)) / (leftRight.y - leftRight.x);
    result.y = (point.y + -point.w * ( -1 + bottomTop.y + bottomTop.x)) / (bottomTop.y - bottomTop.x);
    result.z = (point.w * (nearFar.y + nearFar.x) - 2 * nearFar.x * nearFar.y ) / (nearFar.y - nearFar.x);
    result.w = point.w;
    return result;
  }


  float depthToZ(float d){
    return -2*NN*FF / (d*(NN-FF) + FF + NN);
  }

  vec3 getSample3D(vec3 c){
    //float s = 1.f+(c.z*.5f+.5f)*(FF/NN-1);
    float z  = depthToZ(c.z);
    float s = z/(-NN);

    return vec3(
      LL*s+(.5f+.5f*c.x)*(RR*s-LL*s),
      BB*s+(.5f+.5f*c.y)*(TT*s-BB*s),
      depthToZ(c.z)
      );
  }

  mat4 getProj(){
    mat4 prj = mat4(0);
    prj[0][0] = 2*NN/(RR-LL);
    prj[0][1] = 0;
    prj[0][2] = 0;
    prj[0][3] = 0;
  
    prj[1][0] = 0;
    prj[1][1] = 2*NN/(TT-BB);
    prj[1][2] = 0;
    prj[1][3] = 0;
  
    prj[2][0] = (RR+LL)/(RR-LL);
    prj[2][1] = (TT+BB)/(TT-BB);
    prj[2][2] = -(FF+NN)/(FF-NN);
    prj[2][3] = -1;
  
    prj[3][0] = 0;
    prj[3][1] = 0;
    prj[3][2] = -2*FF*NN/(FF-NN);
    prj[3][3] = 0;
    return prj;
  }

  mat4 getInvProj(){
    return transpose(inverse(getProj()));
  }

  bool pointInside(vec4 b){
    mat4 prj = getProj();

    vec4 a = prj * b;

    //a = movePointToSubfrustum(a,vec2(ax,bx),vec2(ay,by),vec2(az,bz));

    return all(greaterThanEqual(a.xyz,-a.www))&&all(lessThanEqual(a.xyz,+a.www));
  }

  vec4 getClipPlane(in vec4 a,in vec4 b,in vec4 c){
    if(a.w==0){
      if(b.w==0){
        if(c.w==0){
          return vec4(0,0,0,cross(b.xyz-a.xyz,c.xyz-a.xyz).z);
        }else{
          vec3 n = cross(a.xyz*c.w-c.xyz*a.w,b.xyz*c.w-c.xyz*b.w);
          return vec4(n*c.w,-dot(n,c.xyz));
        }
      }else{
        vec3 n = cross(c.xyz*b.w-b.xyz*c.w,a.xyz*b.w-b.xyz*a.w);
        return vec4(n*b.w,-dot(n,b.xyz));
      }
    }else{
      vec3 n = cross(b.xyz*a.w-a.xyz*b.w,c.xyz*a.w-a.xyz*c.w);
      return vec4(n*a.w,-dot(n,a.xyz));
    }
  }

  vec4 getEdgePlane(){
    vec4 a = getProj()*vec4(A,1);
    vec4 b = getProj()*vec4(B,1);
    vec4 c = getProj()*vec4(light);

    return getClipPlane(a,b,c);
  }

  vec4 getSamplePlane(){
    vec4 ll = getProj()*light;
    vec3 normal = normalize(cross(SN-SM,ll.xyz-SM*ll.w));
    return vec4(normal,-dot(normal,SM));
  }

  bool pointInFront(vec4 pp){
    return dot(getEdgePlane(),pp) > 0;
    //return dot(getSamplePlane(),pp) > 0;
  }
  
  #define REMEMBER3(a)if(a==vec3(1337))return
  #define REMEMBER4(a)if(a==vec4(1337))return

  void main(){
    REMEMBER3(A);
    REMEMBER3(B);
    REMEMBER4(light);
    REMEMBER3(SM);
    REMEMBER3(SN);


    vec4 pp;
    pp.x = (-50.f + float(((gl_VertexID%100)    )    ))*.2f;
    pp.y = (-50.f + float(((gl_VertexID/100)%100)    ))*.2f;
    pp.z = (-50.f + float(((gl_VertexID/100)/100)%100))*.2f;
    pp.w = 1.f;

    vColor = vec3(1,0,0);
    if(pointInFront(getProj()*pp))
      vColor = vec3(0,0,1);
      
    gl_Position = proj*view*pp;
    return;


    if(pointInside(pp)){
      vColor = vec3(1,0,0);
      if(pointInFront(getProj()*pp))
        vColor = vec3(0,0,1);
        
      gl_Position = proj*view*pp;
    }else{
      vColor = vec3(0,0,0);
      gl_Position = vec4(2,2,2,1);
    }
  }
  ).";

  std::string const fsSrc = R".(
  #version 450
  layout(location=0)out vec4 fColor;
  in vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawPointCloudProgram",vs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawPointCloudVAO");
}

void drawPointCloud(vars::Vars&vars){
  prepareDrawPointCloud(vars);

  auto prg = vars.get<ge::gl::Program>("drawPointCloudProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawPointCloudVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));

  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set1f("ax",vars.getFloat("ax"));
  prg->set1f("bx",vars.getFloat("bx"));
  prg->set1f("ay",vars.getFloat("ay"));
  prg->set1f("by",vars.getFloat("by"));
  prg->set1f("az",vars.getFloat("az"));
  prg->set1f("bz",vars.getFloat("bz"));


  auto SM = *vars.get<glm::vec3>("SM");
  auto SN = *vars.get<glm::vec3>("SN");
  prg->set3fv("SM",glm::value_ptr(SM));
  prg->set3fv("SN",glm::value_ptr(SN));


  auto light = *vars.get<glm::vec4>("light");
  auto A = *vars.get<glm::vec3>("A");
  auto B = *vars.get<glm::vec3>("B");

  prg->set4fv("light",glm::value_ptr(light));
  prg->set3fv("A",glm::value_ptr(A));
  prg->set3fv("B",glm::value_ptr(B));


  ge::gl::glDrawArrays(GL_POINTS,0,100*100*100);

  vao->unbind();
}

void prepareDrawSide(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  uniform vec3 A = vec3(0,0,0);
  uniform vec3 B = vec3(1,0,0);
  uniform vec4 light = vec4(10,10,10,1);
  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);



  out vec2 vCoord;
  void main(){
    if(gl_VertexID == 0){
      //ppp = movePointToSubfrustum(getProj()*vec4(A,1),vec2(ax,bx),vec2(ay,by),vec2(az,bz));
      vCoord = vec2(0,0);
      gl_Position = proj*view*vec4(A,1);
    }
    if(gl_VertexID == 1){
      //ppp = movePointToSubfrustum(getProj()*vec4(B,1),vec2(ax,bx),vec2(ay,by),vec2(az,bz));
      vCoord = vec2(1,0);
      gl_Position = proj*view*vec4(B,1);
    }
    if(gl_VertexID == 2){
      //ppp = movePointToSubfrustum(getProj()*vec4(A.xyz*light.w-light.xyz,0),vec2(ax,bx),vec2(ay,by),vec2(az,bz));
      vCoord = vec2(0,1);
      gl_Position = proj*view*vec4(A.xyz*light.w-light.xyz,0);
    }
    if(gl_VertexID == 3){
      //ppp = movePointToSubfrustum(getProj()*vec4(B.xyz*light.w-light.xyz,0),vec2(ax,bx),vec2(ay,by),vec2(az,bz));
      vCoord = vec2(1,1);
      gl_Position = proj*view*vec4(B.xyz*light.w-light.xyz,0);
    }
  }
  ).";

  std::string const fsSrc = R".(

  #version 450
  layout(location=0)out vec4 fColor;
  //in vec4 ppp;
  in vec2 vCoord;

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  uniform float ax = 0.f;
  uniform float bx = 1.f;
  uniform float ay = 0.f;
  uniform float by = 1.f;
  uniform float az = 1.f;
  uniform float bz = 10.f;

  uniform vec3 A = vec3(0,0,0);
  uniform vec3 B = vec3(1,0,0);
  uniform vec4 light = vec4(10,10,10,1);

  vec4 movePointToSubfrustum(
      in vec4 point    ,
      in vec2 leftRight,
      in vec2 bottomTop,
      in vec2 nearFar  ){
    vec4 result;
    result.x = (point.x + -point.w * ( -1 + leftRight.y + leftRight.x)) / (leftRight.y - leftRight.x);
    result.y = (point.y + -point.w * ( -1 + bottomTop.y + bottomTop.x)) / (bottomTop.y - bottomTop.x);
    result.z = (point.w * (nearFar.y + nearFar.x) - 2 * nearFar.x * nearFar.y ) / (nearFar.y - nearFar.x);
    result.w = point.w;
    return result;
  }

  mat4 getProj(){
    mat4 prj = mat4(0);
    prj[0][0] = 2*NN/(RR-LL);
    prj[0][1] = 0;
    prj[0][2] = 0;
    prj[0][3] = 0;
  
    prj[1][0] = 0;
    prj[1][1] = 2*NN/(TT-BB);
    prj[1][2] = 0;
    prj[1][3] = 0;
  
    prj[2][0] = (RR+LL)/(RR-LL);
    prj[2][1] = (TT+BB)/(TT-BB);
    prj[2][2] = -(FF+NN)/(FF-NN);
    prj[2][3] = -1;
  
    prj[3][0] = 0;
    prj[3][1] = 0;
    prj[3][2] = -2*FF*NN/(FF-NN);
    prj[3][3] = 0;

    return prj;
  }

  void main(){
    vec4 AA = getProj()*vec4(A,1);
    vec4 BB = getProj()*vec4(B,1);
    vec4 LL = getProj()*light;
    vec4 PP = AA+vCoord.x*(BB-AA) - vCoord.y*LL;

    vec4 ppp = movePointToSubfrustum(PP,vec2(ax,bx),vec2(ay,by),vec2(az,bz));


    float dd = 
    sqrt(pow(abs(abs(ppp.x)-abs(ppp.y)),2)+
    pow(abs(abs(ppp.x)-abs(ppp.z)),2));
    float cc = length(ppp.xy/ppp.w-vec2(-1,-1));
    if(dd < 0.03)
      fColor = vec4(1,1,1,0);
    else
      fColor = vec4(0,0,1,0);
    if(cc < 0.03)
      fColor = vec4(1,0,0,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawSideProgram",vs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawSideVAO");
}

void drawSide(vars::Vars&vars){
  prepareDrawSide(vars);

  ge::gl::glEnable(GL_DEPTH_TEST);
  ge::gl::glDepthMask(GL_TRUE);
  auto prg = vars.get<ge::gl::Program>("drawSideProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawSideVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();
  auto light = *vars.get<glm::vec4>("light");
  auto A = *vars.get<glm::vec3>("A");
  auto B = *vars.get<glm::vec3>("B");

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));
  prg->set4fv("light",glm::value_ptr(light));
  prg->set3fv("A",glm::value_ptr(A));
  prg->set3fv("B",glm::value_ptr(B));
  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set1f("ax",vars.getFloat("ax"));
  prg->set1f("bx",vars.getFloat("bx"));
  prg->set1f("ay",vars.getFloat("ay"));
  prg->set1f("by",vars.getFloat("by"));
  prg->set1f("az",vars.getFloat("az"));
  prg->set1f("bz",vars.getFloat("bz"));

  ge::gl::glPointSize(2);
  ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  vao->unbind();
}

void prepareDrawFrustum(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  void main(){
  }
  ).";

  std::string const gsSrc = R".(
  #version 450

  layout(points)in;
  layout(line_strip,max_vertices=100)out;

  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  uniform float ax = 0.f;
  uniform float bx = 1.f;
  uniform float ay = 0.f;
  uniform float by = 1.f;
  uniform float az = 1.f;
  uniform float bz = 10.f;

  uniform vec3 A;
  uniform vec3 B;
  uniform vec4 light;

  layout(location=13)out vec3 gColor;
 
vec4 movePointToSubfrustum(
    in vec4 point    ,
    in vec2 leftRight,
    in vec2 bottomTop,
    in vec2 nearFar  ){
  vec4 result;
  result.x = (point.x + -point.w * ( -1 + leftRight.y + leftRight.x)) / (leftRight.y - leftRight.x);
  result.y = (point.y + -point.w * ( -1 + bottomTop.y + bottomTop.x)) / (bottomTop.y - bottomTop.x);
  result.z = (point.w * (nearFar.y + nearFar.x) - 2 * nearFar.x * nearFar.y ) / (nearFar.y - nearFar.x);
  result.w = point.w;
  return result;
}

//ax+t*(bx-ax) < aw+t*(bw-aw)
//ax-aw+t*(bx-ax) < t*(bw-aw)
//ax-aw < -t*(bx-ax)+t*(bw-aw)
//(ax-aw) < t*(-bx+ax+bw-aw)
//(ax-aw)/(-bx+ax+bw-aw) < t
//
//ax+t*(bx-ax) > -aw-t*(bw-aw)
//(ax+aw) > t*(-bx+ax-bw+aw)
//(ax+aw)/(-bx+ax-bw+aw) > t
//(-ax-aw)/(-bx+ax-bw+aw) < t
bool doesEdgeIntersectFrustum(in vec4 A,in vec4 B){
  float tMin = 0.f;                         //register R0
  float tMax = 1.f;                         //register R1
  float divident;
  float divisor;
  #define MINIMIZE()\
  if(divisor < 0.f)tMin = max(tMin,divident/divisor);\
  if(divisor > 0.f)tMax = min(tMax,divident/divisor);\
  if(divisor == 0.f && divident < 0.f)tMin = 2.f

  divident = A[0]+A[3];
  divisor  = divident-B[0]-B[3];
  MINIMIZE();
  divident = A[1]+A[3];
  divisor  = divident-B[1]-B[3];
  MINIMIZE();
  divident = A[2]+A[3];
  divisor  = divident-B[2]-B[3];
  MINIMIZE();

  divident = -A[0]+A[3];
  divisor  = divident+B[0]-B[3];
  MINIMIZE();
  divident = -A[1]+A[3];
  divisor  = divident+B[1]-B[3];
  MINIMIZE();
  divident = -A[2]+A[3];
  divisor  = divident+B[2]-B[3];
  MINIMIZE();

#undef MINIMIZE
  return tMin <= tMax;


  //float tMin = 0.f;                         //register R0
  //float tMax = 1.f;                         //register R1
  //for(uint i = 0u; i < 3u; ++i){            //register R2
  //  float dividend = A[i]+A[3];             //register R3
  //  float divisor  = dividend - B[i] - B[3];//register R4
  //  if(divisor > 0.f)
  //    tMax = min(tMax,dividend/divisor);
  //  if(divisor < 0.f)
  //    tMin = max(tMin,dividend/divisor);
  //  if(divisor == 0.f && dividend < 0.f)
  //    return false;
  //  dividend = -A[i]+A[3];
  //  divisor = dividend + B[i] - B[3];
  //  if(divisor > 0.f)
  //    tMax = min(tMax,dividend/divisor);
  //  if(divisor < 0.f)
  //    tMin = max(tMin,dividend/divisor);
  //  if(divisor == 0.f && dividend < 0.f)
  //    return false;
  //}
  //return tMin <= tMax;
}

//*
bool doesDiagonalIntersectShadowVolumeSide(in vec4 A,in vec4 B,in vec4 L,in uint d){
  float a = -1.f + 2.f*float(d/2u);
  float b = -1.f + 2.f*float(d&1u);
  float u = B.x - A.x - a*B.y + a*A.y;
  float v = a*L.y - L.x;
  float w = a*A.y - A.x;
  float x = B.x - A.x - b*B.z + b*A.z;
  float y = b*L.z - L.x;
  float z = b*A.z - A.x;
  float divisor  = u*y - x*v;
  float dividend = w*y - z*v;
  if(divisor == 0.f)return false;
  float t = (w*y - z*v) / (u*y - x*v);
  if(t < 0.f || t > 1.f)return false;
  if(v == 0.f)return false;
  float l = (w-t*u)/v;
  if(l < 0.f || l > 1.f)return false;
  vec4 pp = mix(A,B,t)-L*l;
  return all(greaterThanEqual(pp.xyz,-pp.www))&&all(lessThanEqual(pp.xyz,+pp.www));
  //return true;
}
// */

/*
bool doesDiagonalIntersectShadowVolumeSide(in vec4 A,in vec4 B,in vec4 C,uint Diag){
  vec3 a=A.xyz;
  vec3 b=B.xyz;
  vec3 c=C.xyz;
  a[Diag]=-a[Diag];
  b[Diag]=-b[Diag];
  c[Diag]=-c[Diag];
  float m=(a.x-a.y);
  float n=(b.x-b.y);
  float o=(c.x-c.y);
  float p=(a.x-a.z);
  float q=(b.x-b.z);
  float r=(c.x-c.z);
  float d=(q*o-n*r);
  float t=(m*r-p*o)/d;
  float l=-(m*q-p*n)/d;
  vec4 X=A+t*B+l*C;
  return (t>0)&&(t<1)&&(l>0)&&(l<1)&&
    all(greaterThan(X.xyz,-X.www))&&all(lessThan(X.xyz,X.www));
}
*/


bool doesShadowVolumeSideIntersectsFrustum(in vec4 A,in vec4 B,in vec4 L){
  //return doesEdgeIntersectFrustum(A,B) || doesEdgeIntersectFrustum(A,A-L) || doesEdgeIntersectFrustum(B,B-L);
  for(uint d=0;d<4;++d)
    if(doesDiagonalIntersectShadowVolumeSide(A,B,L,d))return true;
  if(doesEdgeIntersectFrustum(A,B))return true;
  if(doesEdgeIntersectFrustum(A,A-L))return true;
  return doesEdgeIntersectFrustum(B,B-L);
}
#line 182

// line inresects
// i in {x,y,z}
// X(t) = A+t*(B-A)
// -X(t)w <= X(t)i <= + X(t)w
// 
// -X(t)w + 2aiX(t)w <= X(t)i <= -X(t) + 2biX(t)w
// X(t)w*(-1+2ai) <= X(t)i <= X(t)w*(-1+2bi)
// X(t)w*aai <= X(t)i <= X(t)w*bbi
//
// X(t)w*aai <= X(t)i
// X(t)i     <= X(t)w*bbi
//
// [Aw+t(Bw-Aw)]*aai <= Ai+t(Bi-Ai)
// Ai+t(Bi-Ai)       <= [Aw+t(Bw-Aw)]*bbi
//
// Aw*aai+t(Bw-Aw)aai <= Ai+t(Bi-Ai)
// Ai+t(Bi-Ai)        <= Aw*bbi+t(Bw-Aw)bbi
//
// Aw*aai-Ai          <= t(Bi-Ai)-t(Bw-Aw)aai
// Ai-Aw*bbi          <= t(Bw-Aw)bbi-t(Bi-Ai)
//
// Aw*aai-Ai          <= t[Bi-Ai-(Bw-Aw)aai]
// Ai-Aw*bbi          <= t[(Bw-Aw)bbi-Bi+Ai]
//
// +Aw*aai-Ai          <= t[+Bi-Ai-(Bw-Aw)aai]
// -Aw*bbi+Ai          <= t[-Bi+Ai+(Bw-Aw)bbi]
// M                  <= t*N
// N>0: M/N <= t
// N<0: M/N >= t
// N=0: stop when M>0

bool doesLineInterectSubFrustum_(in vec4 A,in vec4 B,in vec3 minCorner,in vec3 maxCorner){
  float tMin = 0.f;
  float tMax = 1.f;
  float M;
  float N;

  #define MINIMIZE()\
  if(N > 0.f)tMin = max(tMin,M/N);\
  if(N < 0.f)tMax = min(tMax,M/N);\
  if(N == 0.f && M > 0.f)tMin = 2.f
  
  M = +A.w*minCorner[0]-A[0];
  N = +B[0]-A[0]-(B.w-A.w)*minCorner[0];
  MINIMIZE();
  M = +A.w*minCorner[1]-A[1];
  N = +B[1]-A[1]-(B.w-A.w)*minCorner[1];
  MINIMIZE();
  M = +A.w*minCorner[2]-A[2];
  N = +B[2]-A[2]-(B.w-A.w)*minCorner[2];
  MINIMIZE();

  M = -A.w*maxCorner[0]+A[0];
  N = -B[0]+A[0]+(B.w-A.w)*maxCorner[0];
  MINIMIZE();
  M = -A.w*maxCorner[1]+A[1];
  N = -B[1]+A[1]+(B.w-A.w)*maxCorner[1];
  MINIMIZE();
  M = -A.w*maxCorner[2]+A[2];
  N = -B[2]+A[2]+(B.w-A.w)*maxCorner[2];
  MINIMIZE();
  
#undef MINIMIZE
  return tMin <= tMax;
}

bool doesLineInterectSubFrustum(in vec4 A,in vec4 B,in vec3 minCorner,in vec3 maxCorner){
  float tt[2] = {0.f,1.f};
  float M;
  float N;
  uint doMin;

  #define MINIMIZE()\
  M/=N;\
  doMin = uint(N<0.f);\
  N=(tt[doMin]-M)*(-1.f+2.f*doMin);\
  tt[doMin] = float(N<0)*tt[doMin] + float(N>=0)*M
  
  M = +A.w*minCorner[0]-A[0];
  N = +B[0]-A[0]-(B.w-A.w)*minCorner[0];
  MINIMIZE();
  M = +A.w*minCorner[1]-A[1];
  N = +B[1]-A[1]-(B.w-A.w)*minCorner[1];
  MINIMIZE();
  M = +A.w*minCorner[2]-A[2];
  N = +B[2]-A[2]-(B.w-A.w)*minCorner[2];
  MINIMIZE();

  M = -A.w*maxCorner[0]+A[0];
  N = -B[0]+A[0]+(B.w-A.w)*maxCorner[0];
  MINIMIZE();
  M = -A.w*maxCorner[1]+A[1];
  N = -B[1]+A[1]+(B.w-A.w)*maxCorner[1];
  MINIMIZE();
  M = -A.w*maxCorner[2]+A[2];
  N = -B[2]+A[2]+(B.w-A.w)*maxCorner[2];
  MINIMIZE();
  
#undef MINIMIZE
  return tt[0] <= tt[1];
}


// 2) one of main diagonals of frustum intersects shadow volume side
// corners of frustum in clip-space:
// C0 = (-1,-1,-1,1)
// C1 = (+1,-1,-1,1)
// C2 = (-1,+1,-1,1)
// C3 = (+1,+1,-1,1)
// C4 = (-1,-1,-1,1)
// C5 = (+1,-1,-1,1)
// C6 = (-1,+1,-1,1)
// C7 = (+1,+1,-1,1)
//
// C(i) = (-1+2*((i>>0)&1),-1+2*((i>>1)&1),-1+2*((i>>2)&1),1)
//
// If main diagonal intersect shadow volume side in point X then:
// ax+(bx-ax)*X.x ==  ay+(by-ay)*+X.y ==  az+(bz-az)*+X.z   C0->C7
// ax+(bx-ax)*X.x ==  ay+(by-ay)*-X.y ==  az+(bz-az)*-X.z   C1->C6
// X.x == -X.y ==  X.z   C2->C5
// X.x ==  X.y == -X.z   C3->C4
//
// edge: A->B, A=(Ax,Ay,Az,1), B=(Bx,By,Bz,1)
// shadow volume side: A->B, C = (Ax-Lx,Ay-Ly,Az-Lz,0), D = (Bx-Lx,By-Ly,Bz-Lz,0)
//
// M(t) = (1-t)*A+t*B
// O(t) = (1-t)*C+t*D
// X(t,l) = (1-l)*M(t) + l*O(t)
// X(t,l) = (1-l)*(1-t)*A + (1-l)*t*B + l*(1-t)*C + l*t*D
// X(t,l) = A -l*A -t*A + t*l*A + t*B - t*l*B + l*C - t*l*C + t*l*D
// X(t,l) = A + l*(C-A) + t*(B-A) + t*l*(A-B-C+D)
// X(t,l) = A + l*((Ax-Lx,Ay-Ly,Az-Lz,0)-(Ax,Ay,Az,1)) + t*(B-A) + t*l*(A-B-C+D)
// X(t,l) = A + l*((-Lx,-Ly,-Lz,-1)) + t*(B-A) + t*l*(A-B-C+D)
// X(t,l) = A - l*L + t*(B-A) + t*l*(A-B-C+D)
// X(t,l) = A - l*L + t*(B-A) + t*l*((Ax,Ay,Az,1)-(Bx,By,Bz,1)-(Ax-Lx,Ay-Ly,Az-Lz,0)+(Bx-Lx,By-Ly,Bz-Lz,0))
// X(t,l) = A - l*L + t*(B-A) + t*l*((Ax-Bx,Ay-By,Az-Bz,0)-(Ax-Lx,Ay-Ly,Az-Lz,0)+(Bx-Lx,By-Ly,Bz-Lz,0))
// X(t,l) = A - l*L + t*(B-A) + t*l*((Lx-Bx,Ly-By,Lz-Bz,0)+(Bx-Lx,By-Ly,Bz-Lz,0))
// X(t,l) = A - l*L + t*(B-A) + t*l*0
// X(t,l) = A - l*L + t*(B-A)
//
// X(t,l)x == X(t,l)y
//
// X(t,l)x ==  X(t,l)y && X(t,l)x ==  X(t,l)z    C0->C7
// X(t,l)x == -X(t,l)y && X(t,l)x == -X(t,l)z    C1->C6
// X(t,l)x == -X(t,l)y && X(t,l)x ==  X(t,l)z    C2->C5
// X(t,l)x ==  X(t,l)y && X(t,l)x == -X(t,l)z    C3->C4
//
// X(t,l)x*(bx-ax)+ax == H*X(t,l)y*(by-ay)+ay && X(t,l)x == G*X(t,l)z, a,b in {-1,1}
//
// [Ax - l*Lx + t*(Bx-Ax)]*(bx-ax)+ax == [Ay - l*Ly + t*(By-Ay)]*H*(by-ay)+ay
// [Ax - l*Lx + t*(Bx-Ax)]*sx+ax == [Ay - l*Ly + t*(By-Ay)]*H*sy+ay
// (Ax*sx-aAy*sy+ax-ay) + l*(HLy*sy-Lx*sx) + t*((Bx-Ax)*sx-(By-Ay)*H*sy) = 0
// t*((Bx-Ax)*sx-(By-Ay)*H*sy) + l*(HLy*sy-Lx*sx) = -(Ax*sx-HAy*sy+ax-ay)
//
// [Ax - l*Lx + t*(Bx-Ax)]*sy+ax == [Ay - l*Ly + t*(By-Ay)]*H*sx+ay
//
// t*[(Bx-Ax)*sy-(By-Ay)*H*sx] + l*[Ly*H*sx-Lx*sy] = Ay*H*sx-Ax*sy+ay-ax
//
//
// [Ax - l*Lx + t*(Bx-Ax)]*(bx-ax)+ax == [GAz - Gl*Lz + Gt*(Bz-Az)]*(bz-az)+az
// t*((Bx-Ay)*sx-(Bz-Az)*G*sz) + l*(GLz*sz-Lx*sz) = -(Ax*sx-G*Az*sz+ax-az)
//
// t*((Bx-Ax)*sx-(By-Ay)*H*sy) + l*(HLy*sy-Lx*sx) = -(Ax*sx-HAy*sy+ax-ay)
// t*((Bx-Ax)*sx-(Bz-Az)*G*sz) + l*(GLz*sz-Lx*sx) = -(Ax*sx-G*Az*sz+ax-az)
//
// t*u + l*v = w
// t*x + l*y = z
//
//     |w v|  / |u v|
// t = |z y| /  |x y|
//
// l = (w-t*u)/v
bool doesDiagonalIntersectShadowVolumeSide2(in vec4 A,in vec4 B,in vec4 L,in uint d,in vec3 minCorner,in vec3 maxCorner){
  vec3 s = maxCorner-minCorner;

  float H = -1.f + 2.f*float(d/2u);
  float G = -1.f + 2.f*float(d&1u);

  float u = (B.x - A.x)*s.y - (B.y - A.y)*H*s.x;
  float v = H*L.y*s.x - L.x*s.y;
  float w = -A.x*s.y + H*A.y*s.x - minCorner.x + minCorner.y;

  float x = (B.x - A.x)*s.z - (B.z - A.z)*G*s.x;
  float y = G*L.z*s.x - L.x*s.z;
  float z = -A.x*s.z + G*A.z*s.x - minCorner.x + minCorner.z;

  float divisor  = u*y - x*v;
  float dividend = w*y - z*v;
  if(divisor == 0.f)return false;
  float t = (w*y - z*v) / (u*y - x*v);
  if(t < 0.f || t > 1.f)return false;
  if(v == 0.f)return false;
  float l = (w-t*u)/v;
  if(l < 0.f || l > 1.f)return false;

  vec4 pp = mix(A,B,t)-L*l;
  return all(greaterThanEqual(pp.xyz,pp.www*minCorner))&&all(lessThanEqual(pp.xyz,pp.www*maxCorner));
  //return true;
}



bool pointInside(vec4 a){
  return all(greaterThanEqual(a.xyz,-a.www))&&all(lessThanEqual(a.xyz,+a.www));
}

bool silhouetteStatus(){//vec3 minCorner,vec3 aabbSize){
  mat4 prj = mat4(0);
  prj[0][0] = 2*NN/(RR-LL);
  prj[0][1] = 0;
  prj[0][2] = 0;
  prj[0][3] = 0;

  prj[1][0] = 0;
  prj[1][1] = 2*NN/(TT-BB);
  prj[1][2] = 0;
  prj[1][3] = 0;

  prj[2][0] = (RR+LL)/(RR-LL);
  prj[2][1] = (TT+BB)/(TT-BB);
  prj[2][2] = -(FF+NN)/(FF-NN);
  prj[2][3] = -1;

  prj[3][0] = 0;
  prj[3][1] = 0;
  prj[3][2] = -2*FF*NN/(FF-NN);
  prj[3][3] = 0;

  vec4 AA = prj*vec4(A,1);
  vec4 BB = prj*vec4(B,1);
  vec4 LI = prj*light;

  //vec2 leftRight = vec2(minCorner.x,minCorner.x+aabbSize.x)*.5+.5;
  //vec2 bottomTop = vec2(minCorner.y,minCorner.y+aabbSize.y)*.5+.5;
  //vec2 nearFar   = vec2(minCorner.z,minCorner.z+aabbSize.z)*.5+.5;

  //return pointInside(AA) || pointInside(BB) || pointInside(LI);

  vec4 AAA = movePointToSubfrustum(AA,vec2(ax,bx),vec2(ay,by),vec2(az,bz));
  vec4 BBB = movePointToSubfrustum(BB,vec2(ax,bx),vec2(ay,by),vec2(az,bz));
  vec4 LLL = movePointToSubfrustum(LI,vec2(ax,bx),vec2(ay,by),vec2(az,bz));

  //return pointInside(AAA) || pointInside(BBB) || pointInside(LLL);

  if(doesShadowVolumeSideIntersectsFrustum(AAA,BBB,LLL))
    return true;

  return false;
}

bool doesDiagonal(uint d){
  mat4 prj = mat4(0);
  prj[0][0] = 2*NN/(RR-LL);
  prj[0][1] = 0;
  prj[0][2] = 0;
  prj[0][3] = 0;

  prj[1][0] = 0;
  prj[1][1] = 2*NN/(TT-BB);
  prj[1][2] = 0;
  prj[1][3] = 0;

  prj[2][0] = (RR+LL)/(RR-LL);
  prj[2][1] = (TT+BB)/(TT-BB);
  prj[2][2] = -(FF+NN)/(FF-NN);
  prj[2][3] = -1;

  prj[3][0] = 0;
  prj[3][1] = 0;
  prj[3][2] = -2*FF*NN/(FF-NN);
  prj[3][3] = 0;

  vec3 n = normalize(cross(B-A,light.xyz-A));
  vec4 p = vec4(n,-dot(n,A));
  p = inverse(transpose(prj))*p;

  vec3 minCorner = -1+2*vec3(ax,ay,(1/az-1/NN)/(1/FF-1/NN));
  vec3 maxCorner = -1+2*vec3(bx,by,(1/bz-1/NN)/(1/FF-1/NN));


  if((d&1u) != 0u){
    float z = minCorner.x;
    minCorner.x = maxCorner.x;
    maxCorner.x = z;
  }
  if((d&2u) != 0u){
    float z = minCorner.y;
    minCorner.y = maxCorner.y;
    maxCorner.y = z;
  }

  float s = -dot(p,vec4(minCorner,1))/dot(maxCorner-minCorner,p.xyz);

  vec3 cc = minCorner+s*(maxCorner-minCorner);

  vec4 AA = prj*vec4(A,1);
  vec4 BB = prj*vec4(B,1);
  vec4 LI = prj*light;
#line 8000
  float u = (BB.x-AA.x)-(BB.w-AA.w)*cc.x;
  float v = -LI.x+LI.w*cc.x;
  float w = -AA.x+cc.x*AA.w;

  float x = (BB.y-AA.y)-(BB.w-AA.w)*cc.y;
  float y = -LI.y+LI.w*cc.y;
  float z = -AA.y+cc.y*AA.w;

  float divisor  = u*y - x*v;
  float dividend = w*y - z*v;
  if(divisor == 0.f)return false;
  float t = (w*y - z*v) / (u*y - x*v);
  if(t < 0.f || t > 1.f)return false;
  if(v == 0.f)return false;
  float l = (w-t*u)/v;
  if(l < 0.f || l > 1.f)return false;

  if((d&1u) != 0u){
    float z = minCorner.x;
    minCorner.x = maxCorner.x;
    maxCorner.x = z;
  }
  if((d&2u) != 0u){
    float z = minCorner.y;
    minCorner.y = maxCorner.y;
    maxCorner.y = z;
  }

  vec4 pp = mix(AA,BB,t)-LI*l;
  return all(greaterThanEqual(pp.xyz,pp.www*minCorner))&&all(lessThanEqual(pp.xyz,pp.www*maxCorner));
  //return true;
}

bool silhouetteStatus2(){
  mat4 prj = mat4(0);
  prj[0][0] = 2*NN/(RR-LL);
  prj[0][1] = 0;
  prj[0][2] = 0;
  prj[0][3] = 0;

  prj[1][0] = 0;
  prj[1][1] = 2*NN/(TT-BB);
  prj[1][2] = 0;
  prj[1][3] = 0;

  prj[2][0] = (RR+LL)/(RR-LL);
  prj[2][1] = (TT+BB)/(TT-BB);
  prj[2][2] = -(FF+NN)/(FF-NN);
  prj[2][3] = -1;

  prj[3][0] = 0;
  prj[3][1] = 0;
  prj[3][2] = -2*FF*NN/(FF-NN);
  prj[3][3] = 0;

  vec4 AA = prj*vec4(A,1);
  vec4 BB = prj*vec4(B,1);
  vec4 LI = prj*light;

  vec3 minCorner = -1+2*vec3(ax,ay,(1/az-1/NN)/(1/FF-1/NN));
  vec3 maxCorner = -1+2*vec3(bx,by,(1/bz-1/NN)/(1/FF-1/NN));
  if(doesLineInterectSubFrustum(AA,BB   ,minCorner,maxCorner))return true;
  if(doesLineInterectSubFrustum(AA,AA-LI,minCorner,maxCorner))return true;
  if(doesLineInterectSubFrustum(BB,BB-LI,minCorner,maxCorner))return true;
  if(doesDiagonal(0))return true;
  if(doesDiagonal(1))return true;
  if(doesDiagonal(2))return true;
  if(doesDiagonal(3))return true;

  return false;
}


  void drawLine(vec3 a,vec3 b){
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    gl_Position = proj*view*vec4(b,1);EmitVertex();
    EndPrimitive();
  }

  void drawLoop(vec3 a,vec3 b,vec3 c,vec3 d){
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    gl_Position = proj*view*vec4(b,1);EmitVertex();
    gl_Position = proj*view*vec4(c,1);EmitVertex();
    gl_Position = proj*view*vec4(d,1);EmitVertex();
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    EndPrimitive();
  }


  void main(){
    vec3 fp[8];
    fp[0+0] = vec3(LL,BB,-NN);
    fp[0+1] = vec3(RR,BB,-NN);
    fp[0+2] = vec3(LL,TT,-NN);
    fp[0+3] = vec3(RR,TT,-NN);
    fp[4+0] = vec3(LL*FF/NN,BB*FF/NN,-FF);
    fp[4+1] = vec3(RR*FF/NN,BB*FF/NN,-FF);
    fp[4+2] = vec3(LL*FF/NN,TT*FF/NN,-FF);
    fp[4+3] = vec3(RR*FF/NN,TT*FF/NN,-FF);

    gColor = vec3(1,1,1);
    drawLoop(fp[0],fp[1],fp[3],fp[2]);
    drawLoop(fp[4],fp[5],fp[7],fp[6]);
    drawLine(fp[0],fp[4]);
    drawLine(fp[1],fp[5]);
    drawLine(fp[2],fp[6]);
    drawLine(fp[3],fp[7]);

    fp[0+0] = vec3(mix(LL,RR,ax)*az/NN,mix(BB,TT,ay)*az/NN,-az);
    fp[0+1] = vec3(mix(LL,RR,bx)*az/NN,mix(BB,TT,ay)*az/NN,-az);
    fp[0+2] = vec3(mix(LL,RR,ax)*az/NN,mix(BB,TT,by)*az/NN,-az);
    fp[0+3] = vec3(mix(LL,RR,bx)*az/NN,mix(BB,TT,by)*az/NN,-az);
    fp[4+0] = vec3(mix(LL,RR,ax)*bz/NN,mix(BB,TT,ay)*bz/NN,-bz);
    fp[4+1] = vec3(mix(LL,RR,bx)*bz/NN,mix(BB,TT,ay)*bz/NN,-bz);
    fp[4+2] = vec3(mix(LL,RR,ax)*bz/NN,mix(BB,TT,by)*bz/NN,-bz);
    fp[4+3] = vec3(mix(LL,RR,bx)*bz/NN,mix(BB,TT,by)*bz/NN,-bz);

    if(silhouetteStatus2())
      gColor = vec3(1,0,0);
    else
      gColor = vec3(0,1,0);

    drawLoop(fp[0],fp[1],fp[3],fp[2]);
    drawLoop(fp[4],fp[5],fp[7],fp[6]);
    drawLine(fp[0],fp[4]);
    drawLine(fp[1],fp[5]);
    drawLine(fp[2],fp[6]);
    drawLine(fp[3],fp[7]);
    drawLine(fp[0],fp[7]);
    drawLine(fp[1],fp[6]);
    drawLine(fp[2],fp[5]);
    drawLine(fp[3],fp[4]);

  }

  ).";

  std::string const fsSrc = R".(

  #version 450
  layout(location=0)out vec4 fColor;
  layout(location=13)in vec3 gColor;
  void main(){
    fColor = vec4(gColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,gsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawFrustumProgram",vs,gs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawFrustumVAO");
}

void drawFrustum(vars::Vars&vars){
  prepareDrawFrustum(vars);

  auto prg = vars.get<ge::gl::Program>("drawFrustumProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawFrustumVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  auto light = *vars.get<glm::vec4>("light");
  auto A = *vars.get<glm::vec3>("A");
  auto B = *vars.get<glm::vec3>("B");

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));

  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set1f("ax",vars.getFloat("ax"));
  prg->set1f("bx",vars.getFloat("bx"));
  prg->set1f("ay",vars.getFloat("ay"));
  prg->set1f("by",vars.getFloat("by"));
  prg->set1f("az",vars.getFloat("az"));
  prg->set1f("bz",vars.getFloat("bz"));

  prg->set4fv("light",glm::value_ptr(light));
  prg->set3fv("A",glm::value_ptr(A));
  prg->set3fv("B",glm::value_ptr(B));

  ge::gl::glDrawArrays(GL_POINTS,0,1);

  vao->unbind();
}

void prepareDrawTriangle(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;
  std::string const vsSrc = R".(
  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);

  uniform vec3 tA = vec3(0,0,0);
  uniform vec3 tB = vec3(1,0,0);
  uniform vec3 tC = vec3(0,1,0);

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  uniform float ax = 0.f;
  uniform float bx = 1.f;
  uniform float ay = 0.f;
  uniform float by = 1.f;
  uniform float az = 1.f;
  uniform float bz = 10.f;

  out vec3 vColor;
  void main(){

    mat4 prj = mat4(0);
    prj[0][0] = 2*NN/(RR-LL);
    prj[0][1] = 0;
    prj[0][2] = 0;
    prj[0][3] = 0;

    prj[1][0] = 0;
    prj[1][1] = 2*NN/(TT-BB);
    prj[1][2] = 0;
    prj[1][3] = 0;

    prj[2][0] = (RR+LL)/(RR-LL);
    prj[2][1] = (TT+BB)/(TT-BB);
    prj[2][2] = -(FF+NN)/(FF-NN);
    prj[2][3] = -1;

    prj[3][0] = 0;
    prj[3][1] = 0;
    prj[3][2] = -2*FF*NN/(FF-NN);
    prj[3][3] = 0;

    vec3 minCorner = -1+2*vec3(ax,ay,(1/az-1/NN)/(1/FF-1/NN));
    vec3 maxCorner = -1+2*vec3(bx,by,(1/bz-1/NN)/(1/FF-1/NN));

    if(minCorner.x == 1337)return;
    if(all(lessThanEqual(tA,vec3(-1e10))))return;
    if(all(lessThanEqual(tB,vec3(-1e10))))return;
    if(all(lessThanEqual(tC,vec3(-1e10))))return;

    vec4 tAA = prj*vec4(tA,1);
    vec4 tBB = prj*vec4(tB,1);
    vec4 tCC = prj*vec4(tC,1);
    vColor = vec3(0,.5,0);
    if(doesLineInterectSubFrustum(tAA,tBB,minCorner,maxCorner))vColor = vec3(1,1,1);
    if(doesLineInterectSubFrustum(tBB,tCC,minCorner,maxCorner))vColor = vec3(1,1,1);
    if(doesLineInterectSubFrustum(tCC,tAA,minCorner,maxCorner))vColor = vec3(1,1,1);

    if(gl_VertexID == 0)gl_Position = proj*view*vec4(tA,1);
    if(gl_VertexID == 1)gl_Position = proj*view*vec4(tB,1);
    if(gl_VertexID == 2)gl_Position = proj*view*vec4(tC,1);
  }
  ).";

  std::string const fsSrc = R".(
  #version 450
  out vec4 fColor;
  in  vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";
  
  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
      "#version 450\n",
      doesLineInterectSubFrustum,
      vsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawTriangleProgram",vs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawTriangleVAO");
}
void drawTriangle(vars::Vars&vars){

  prepareDrawTriangle(vars);

  auto prg = vars.get<ge::gl::Program>("drawTriangleProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawTriangleVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));

  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set1f("ax",vars.getFloat("ax"));
  prg->set1f("bx",vars.getFloat("bx"));
  prg->set1f("ay",vars.getFloat("ay"));
  prg->set1f("by",vars.getFloat("by"));
  prg->set1f("az",vars.getFloat("az"));
  prg->set1f("bz",vars.getFloat("bz"));

  prg->set3fv("tA",(float*)vars.addOrGet<glm::vec3>("tA",0.f,0.f,-2.f));
  prg->set3fv("tB",(float*)vars.addOrGet<glm::vec3>("tB",1.f,0.f,-2.f));
  prg->set3fv("tC",(float*)vars.addOrGet<glm::vec3>("tC",0.f,1.f,-2.f));

  addVarsLimits3F(vars,"tA"    ,-10,10,0.1);
  addVarsLimits3F(vars,"tB"    ,-10,10,0.1);
  addVarsLimits3F(vars,"tC"    ,-10,10,0.1);

  ge::gl::glDrawArrays(GL_TRIANGLES,0,3);

  vao->unbind();
}

void prepareDrawSamples(vars::Vars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{}))return;

  std::string const vsSrc = R".(
  #version 450
  void main(){
  }
  ).";

  std::string const gsSrc = R".(
  #version 450

  layout(points)in;
  layout(line_strip,max_vertices=100)out;

  uniform mat4 view = mat4(1);
  uniform mat4 proj = mat4(1);

  uniform vec3 A;
  uniform vec3 B;
  uniform vec4 light;

  uniform vec3 SM;
  uniform vec3 SN;

  uniform float LL = -1.f;
  uniform float RR = +1.f;
  uniform float BB = -1.f;
  uniform float TT = +1.f;
  uniform float NN = +1.f;
  uniform float FF = +10.f;

  layout(location=13)out vec3 gColor;
  
  float depthToZ(float d){
    return -2*NN*FF / (d*(NN-FF) + FF + NN);
  }

  vec3 getSample3D(vec3 c){
    //float s = 1.f+(c.z*.5f+.5f)*(FF/NN-1);
    float z  = depthToZ(c.z);
    float s = z/(-NN);

    return vec3(
      LL*s+(.5f+.5f*c.x)*(RR*s-LL*s),
      BB*s+(.5f+.5f*c.y)*(TT*s-BB*s),
      depthToZ(c.z)
      );
  }


  mat4 getProj(){
    mat4 prj = mat4(0);
    prj[0][0] = 2*NN/(RR-LL);
    prj[0][1] = 0;
    prj[0][2] = 0;
    prj[0][3] = 0;

    prj[1][0] = 0;
    prj[1][1] = 2*NN/(TT-BB);
    prj[1][2] = 0;
    prj[1][3] = 0;

    prj[2][0] = (RR+LL)/(RR-LL);
    prj[2][1] = (TT+BB)/(TT-BB);
    prj[2][2] = -(FF+NN)/(FF-NN);
    prj[2][3] = -1;

    prj[3][0] = 0;
    prj[3][1] = 0;
    prj[3][2] = -2*FF*NN/(FF-NN);
    prj[3][3] = 0;
    return prj;
  }

vec4 getClipPlaneSkala(in vec4 A,in vec4 B,in vec4 C){
  float x1 = A.x;
  float x2 = B.x;
  float x3 = C.x;
  float y1 = A.y;
  float y2 = B.y;
  float y3 = C.y;
  float z1 = A.z;
  float z2 = B.z;
  float z3 = C.z;
  float w1 = A.w;
  float w2 = B.w;
  float w3 = C.w;

  float a =  y1*(z2*w3-z3*w2) - y2*(z1*w3-z3*w1) + y3*(z1*w2-z2*w1);
  float b = -x1*(z2*w3-z3*w2) + x2*(z1*w3-z3*w1) - x3*(z1*w2-z2*w1);
  float c =  x1*(y2*w3-y3*w2) - x2*(y1*w3-y3*w1) + x3*(y1*w2-y2*w1);
  float d = -x1*(y2*z3-y3*z2) + x2*(y1*z3-y3*z1) - x3*(y1*z2-y2*z1);
  return vec4(a,b,c,d);
}
#line 1198  
vec4 edgePlane      = vec4(0);
vec4 edgeAClipSpace = vec4(0);
vec4 edgeBClipSpace = vec4(0);
vec4 lightClipSpace = vec4(0);
int edgeMult = 1;

int computeBridge(in vec4 bridgeStart,in vec4 bridgeEnd){
  // m n c 
  // - - 0
  // 0 - 1
  // + - 1
  // - 0 1
  // 0 0 0
  // + 0 0
  // - + 1
  // 0 + 0
  // + + 0
  //
  // m<0 && n>=0 || m>=0 && n<0
  // m<0 xor n<0

  int result = edgeMult;
  float ss = dot(edgePlane,bridgeStart);
  float es = dot(edgePlane,bridgeEnd  );
  if((ss<0)==(es<0))return 0;
  result *= 1-2*int(ss<0.f);

  vec4 samplePlane    = getClipPlaneSkala(bridgeStart,bridgeEnd,lightClipSpace);
  ss = dot(samplePlane,edgeAClipSpace);
  es = dot(samplePlane,edgeBClipSpace);
  ss*=es;
  if(ss>0.f)return 0;
  result *= 1+int(ss<0.f);

  vec4 trianglePlane  = getClipPlaneSkala(bridgeStart,bridgeEnd,bridgeStart + (edgeBClipSpace-edgeAClipSpace));
  trianglePlane *= sign(dot(trianglePlane,lightClipSpace));
  if(dot(trianglePlane,edgeAClipSpace)<=0)return 0;

  return result;

}

int compBrid(
      in vec3 startSample,
      in vec3 endSample  ,
      in vec4 aa         ,
      in vec4 bb         ,
      in vec4 ll         ){
  edgePlane       = getClipPlaneSkala(aa,bb,ll);
  edgeAClipSpace  = aa;
  edgeBClipSpace  = bb;
  lightClipSpace  = ll;
  return computeBridge(vec4(startSample,1),vec4(endSample,1));
}

  bool sampleCollision(
      in vec3 startSample,
      in vec3 endSample  ,
      in vec4 aa         ,
      in vec4 bb         ,
      in vec4 ll         ){
    

    vec3 edgeNormal     = normalize(cross(bb.xyz*aa.w-aa.xyz*bb.w,ll.xyz*aa.w-aa.xyz*ll.w));
    vec4 edgePlane      = vec4(edgeNormal,-dot(edgeNormal,aa.xyz/aa.w));


    vec3 sampleNormal   = normalize(cross(endSample-startSample,ll.xyz-startSample*ll.w));
    vec4 samplePlane    = vec4(sampleNormal,-dot(sampleNormal,startSample));

    vec4 trianglePlane  = getClipPlaneSkala(vec4(startSample,1),vec4(endSample,1),vec4(startSample,1) + (bb-aa));
    //vec3 triangleNormal = normalize(cross(endSample-startSample,bb.xyz*aa.w-aa.xyz*bb.w));
    //vec4 trianglePlane  = vec4(triangleNormal,-dot(triangleNormal,startSample));

    // m n c 
    // - - 0
    // 0 - 1
    // + - 1
    // - 0 1
    // 0 0 0
    // + 0 0
    // - + 1
    // 0 + 0
    // + + 0
    //
    // m<0 && n>=0 || m>=0 && n<0
    // m<0 xor n<0

    if((dot(edgePlane,vec4(startSample,1))<0) == (dot(edgePlane,vec4(endSample,1))<0))return false;

    if(dot(trianglePlane,aa)<=0)return false;

    return dot(samplePlane,aa)*dot(samplePlane,bb) <= 0;
  }

  void drawLine(vec3 a,vec3 b){
    gl_Position = proj*view*vec4(a,1);EmitVertex();
    gl_Position = proj*view*vec4(b,1);EmitVertex();
    EndPrimitive();
  }

  void main(){
    gColor = vec3(1,1,0);
    if(light == vec4(0) || A == vec3(0) || B == vec3(0))gColor = vec3(1);
#if 0
    if(sampleCollision(SM,SN,getProj()*vec4(A,1),getProj()*vec4(B,1),getProj()*light))
      gColor = vec3(0,1,1);
#else
    int mult = compBrid(SM,SN,getProj()*vec4(A,1),getProj()*vec4(B,1),getProj()*light);
    if(mult > 0)gColor = vec3(.5,0,0);
    if(mult < 0)gColor = vec3(0,0,.5);
    if(mult == 0)gColor = vec3(0.2);
#endif
    drawLine(getSample3D(SM),getSample3D(SN));

    gColor = vec3(.3);
    drawLine(light.xyz,getSample3D(SM));
    drawLine(light.xyz,getSample3D(SN));
  }

  ).";

  std::string const fsSrc = R".(

  #version 450
  layout(location=0)out vec4 fColor;
  layout(location=13)in vec3 gColor;
  void main(){
    fColor = vec4(gColor,1);
  }
  ).";

  auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,vsSrc);
  auto gs = std::make_shared<ge::gl::Shader>(GL_GEOMETRY_SHADER,gsSrc);
  auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,fsSrc);

  vars.reCreate<ge::gl::Program>("drawSamplesProgram",vs,gs,fs);
  
  vars.reCreate<ge::gl::VertexArray>("drawSamplesVAO");
}

void drawSamples(vars::Vars&vars){
  prepareDrawSamples(vars);

  auto prg = vars.get<ge::gl::Program>("drawSamplesProgram");
  auto vao = vars.get<ge::gl::VertexArray>("drawSamplesVAO");

  auto view = vars.getReinterpret<basicCamera::CameraTransform>("view")->getView();
  auto proj = vars.get<basicCamera::PerspectiveCamera>("projection")->getProjection();

  auto light = *vars.get<glm::vec4>("light");
  auto A = *vars.get<glm::vec3>("A");
  auto B = *vars.get<glm::vec3>("B");

  auto SM = *vars.get<glm::vec3>("SM");
  auto SN = *vars.get<glm::vec3>("SN");

  vao->bind();
  prg->use();
  prg->setMatrix4fv("view",glm::value_ptr(view));
  prg->setMatrix4fv("proj",glm::value_ptr(proj));

  prg->set1f("LL",vars.getFloat("LL"));
  prg->set1f("RR",vars.getFloat("RR"));
  prg->set1f("BB",vars.getFloat("BB"));
  prg->set1f("TT",vars.getFloat("TT"));
  prg->set1f("NN",vars.getFloat("NN"));
  prg->set1f("FF",vars.getFloat("FF"));

  prg->set3fv("SM",glm::value_ptr(SM));
  prg->set3fv("SN",glm::value_ptr(SN));

  prg->set4fv("light",glm::value_ptr(light));
  prg->set3fv("A",glm::value_ptr(A));
  prg->set3fv("B",glm::value_ptr(B));

  ge::gl::glLineWidth(3);
  ge::gl::glDrawArrays(GL_POINTS,0,1);
  ge::gl::glLineWidth(1);

  vao->unbind();
}

class EmptyProject: public simple3DApp::Application{
 public:
  EmptyProject(int argc, char* argv[]) : Application(argc, argv,330) {}
  virtual ~EmptyProject(){}
  virtual void draw() override;

  DVars vars;

  virtual void                init() override;
  virtual void                mouseMove(SDL_Event const& event) override;
  virtual void                key(SDL_Event const& e, bool down) override;
  virtual void                resize(uint32_t x,uint32_t y) override;
};

void createView(DVars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"useOrbitCamera"}))return;

  if(vars.getBool("useOrbitCamera"))
    vars.reCreate<basicCamera::OrbitCamera>("view");
  else
    vars.reCreate<basicCamera::FreeLookCamera>("view");
}

void createProjection(DVars&vars){
  if(notChanged(vars,"all",__FUNCTION__,{"windowSize","camera.fovy","camera.near","camera.far"}))return;

  auto windowSize = vars.getUVec2("windowSize");
  auto width = windowSize.x;
  auto height = windowSize.y;
  auto aspect = (float)width/(float)height;
  auto nearv = vars.getFloat("camera.near");
  auto farv  = vars.getFloat("camera.far" );
  auto fovy = vars.getFloat("camera.fovy");

  vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,nearv,farv);
}

void createCamera(DVars&vars){
  createProjection(vars);
  createView(vars);
}

void EmptyProject::init(){
  vars.add<ge::gl::VertexArray>("emptyVao");
  vars.addFloat("input.sensitivity",0.01f);
  vars.addUVec2("windowSize",window->getWidth(),window->getHeight());
  vars.addFloat("camera.fovy",glm::half_pi<float>());
  vars.addFloat("camera.near",.1f);
  vars.addFloat("camera.far",1000.f);
  vars.addMap<SDL_Keycode, bool>("input.keyDown");
  vars.addBool("useOrbitCamera",false);

  vars.add<glm::vec4>("light",glm::vec4(10,30,0,1));
  vars.add<glm::vec3>("A",glm::vec3(-2.5,1.1,-5.7));
  vars.add<glm::vec3>("B",glm::vec3(1.3,.6,-3.4));
  addVarsLimits3F(vars,"A"    ,-10,10,0.1);
  addVarsLimits3F(vars,"B"    ,-10,10,0.1);
  addVarsLimits3F(vars,"light",-100,100,0.1);


  vars.add<glm::vec3>("SM",glm::vec3(-0.272,0.248,0.333));
  vars.add<glm::vec3>("SN",glm::vec3(0.515,0.283,+1));
  addVarsLimits3F(vars,"SM"    ,-1,1,0.001);
  addVarsLimits3F(vars,"SN"    ,-1,1,0.001);


  vars.addFloat("LL",-1.f);
  vars.addFloat("RR",+1.f);
  vars.addFloat("BB",-1.f);
  vars.addFloat("TT",+1.f);
  vars.addFloat("NN",1.f);
  vars.addFloat("FF",10.f);

  vars.addFloat("ax",0.3);
  vars.addFloat("bx",0.5);
  vars.addFloat("ay",0.3);
  vars.addFloat("by",0.5);
  vars.addFloat("az",2.f);
  vars.addFloat("bz",6.f);

  addVarsLimitsF(vars,"ax",0,1,0.01);
  addVarsLimitsF(vars,"bx",0,1,0.01);
  addVarsLimitsF(vars,"ay",0,1,0.01);
  addVarsLimitsF(vars,"by",0,1,0.01);
  addVarsLimitsF(vars,"az",1,10,0.01);
  addVarsLimitsF(vars,"bz",1,10,0.01);

  createCamera(vars);
  window->setSize(1920,1080);
}

void EmptyProject::draw(){
  createCamera(vars);
  basicCamera::CameraTransform*view;

  if(vars.getBool("useOrbitCamera"))
    view = vars.getReinterpret<basicCamera::CameraTransform>("view");
  else{
    auto freeView = vars.get<basicCamera::FreeLookCamera>("view");
    float freeCameraSpeed = 0.01f;
    auto& keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
    for (int a = 0; a < 3; ++a)
      freeView->move(a, float(keys["d s"[a]] - keys["acw"[a]]) *
                            freeCameraSpeed);
    view = freeView;
  }

  ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
  ge::gl::glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  vars.get<ge::gl::VertexArray>("emptyVao")->bind();

  drawGrid(vars);

  vars.get<ge::gl::VertexArray>("emptyVao")->unbind();

  if(vars.addOrGetBool("drawTriangle"  ,true))drawTriangle  (vars);
  if(vars.addOrGetBool("drawPointCloud",false))drawPointCloud(vars);
  if(vars.addOrGetBool("drasSide"      ,false))drawSide      (vars);
  if(vars.addOrGetBool("drawFrustum"   ,true))drawFrustum   (vars);
  if(vars.addOrGetBool("drawSamples"   ,false))drawSamples   (vars);

  drawImguiVars(vars);

  swap();
}

void EmptyProject::key(SDL_Event const& event, bool DOWN) {
  auto&keys = vars.getMap<SDL_Keycode, bool>("input.keyDown");
  keys[event.key.keysym.sym] = DOWN;
}

void orbitManipulator(DVars&vars,SDL_Event const&e){
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.getUVec2("windowSize");
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
                                float(windowSize.x) * 2.f);
      orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                float(windowSize.y) * 2.f);
    }
}

void freeLookManipulator(DVars&vars,SDL_Event const&e){
  auto const xrel = static_cast<float>(e.motion.xrel);
  auto const yrel = static_cast<float>(e.motion.yrel);
  auto view = vars.get<basicCamera::FreeLookCamera>("view");
  auto sensitivity = vars.getFloat("input.sensitivity");
  if (e.motion.state & SDL_BUTTON_LMASK) {
    view->setAngle(
        1, view->getAngle(1) + xrel * sensitivity);
    view->setAngle(
        0, view->getAngle(0) + yrel * sensitivity);
  }
}

void EmptyProject::mouseMove(SDL_Event const& e) {
  if(vars.getBool("useOrbitCamera"))
    orbitManipulator(vars,e);
  else
    freeLookManipulator(vars,e);
}

void EmptyProject::resize(uint32_t x,uint32_t y){
  auto&windowSize = vars.getUVec2("windowSize");
  windowSize.x = x;
  windowSize.y = y;
  vars.updateTicks("windowSize");
  ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[]){
  EmptyProject app{argc, argv};
  app.start();
  return EXIT_SUCCESS;
}
