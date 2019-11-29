#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <TxtUtils/TxtUtils.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>

class CSCompiler: public simple3DApp::Application{
 public:
  CSCompiler(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~CSCompiler(){}
};

using namespace ge::gl;

int main(int argc,char*argv[]){
  CSCompiler app{argc, argv};

  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  auto N = args->getu64("-N",10,"how many times are tests executed");
  auto DATA = args->getu64("-D",1024*1024*8,"nof of elements");
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  std::vector<uint32_t>inputData(DATA*3);
  auto inputBuf = std::make_shared<ge::gl::Buffer>(inputData);
  inputBuf->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto outputBuf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*DATA);
  outputBuf->bindBase(GL_SHADER_STORAGE_BUFFER,1);

  auto const measure = [&](std::string const&name,std::string const&src,size_t line,size_t warp = 32,size_t windowX = 512,size_t windowY = 512,size_t minZ = 9){
    std::stringstream ss;
    ss << "#version 450" << std::endl;
    ss << "#line " << __LINE__ << std::endl;
    ss << "layout(local_size_x=256)in;" << std::endl;
    ss << "layout(binding=0)buffer InputBuffer {uint inputData [];};" << std::endl;
    ss << "layout(binding=1)buffer OutputBuffer{uint outputData[];};" << std::endl;

    ss << "#define WARP     " << warp    << "u" << std::endl;
    ss << "#define WINDOW_X " << windowX << "u" << std::endl;
    ss << "#define WINDOW_Y " << windowY << "u" << std::endl;
    ss << "#define MIN_Z    " << minZ    << "u" << std::endl;

    ss << "#line " << line << std::endl;
    ss << src;
    ss << R".(
    void main(){
      uvec3 v;
      v.x = inputData[gl_GlobalInvocationID.x*3+0];
      v.y = inputData[gl_GlobalInvocationID.x*3+1];
      v.z = inputData[gl_GlobalInvocationID.x*3+2];
      uint m = morton(v);

      outputData [gl_GlobalInvocationID.x] = m;
    }
    ).";

    auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

    auto timer = Timer<float>();
    glFinish();
    timer.reset();
    prg->use();
    for(size_t i=0;i<N;++i)
      prg->dispatch(DATA/256);
    glFinish();
    auto time = timer.elapsedFromStart()/N;
    std::cerr << name << ": " << time << std::endl;
  };
#define MEASURE(name,src) measure(name,src,__LINE__)

  {MEASURE("warm up",R".(
  uint morton(uvec3 v){
    return v.x+v.y+v.z;
  }
  
  ).");}

  {MEASURE("onlyMerge",R".(

#define REQUIRED_BITS(x)  uint(ceil(log2(float(x))))
#define DIV_ROUND_UP(x,y) (uint(x/y) + uint(x%y != 0u))

#define EXPAND_BITS0(V,O) ((uint(V) * (0x00010001u<<uint(O))) & (0xFF0000FFu<<uint(O)))
#define EXPAND_BITS1(V,O) ((uint(V) * (0x00000101u<<uint(O))) & (0x0F00F00Fu<<uint(O)))
#define EXPAND_BITS2(V,O) ((uint(V) * (0x00000011u<<uint(O))) & (0xC30C30C3u<<uint(O)))
#define EXPAND_BITS3(V,O) ((uint(V) * (0x00000005u<<uint(O))) & (0x49249249u<<uint(O)))

#define EXPAND_BITS(V,O) EXPAND_BITS3(EXPAND_BITS2(EXPAND_BITS1(EXPAND_BITS0(V,0),0),0),O)

#define MERGE_BITS(A,B,C) (EXPAND_BITS((A),0) | EXPAND_BITS((B),1) | EXPAND_BITS((C),2))

uint morton(uvec3 v){
  uint res = MERGE_BITS(v[0],v[1],v[2]);

  return res;
}

  ).");}

  {MEASURE("valuesInRegisters",R".(

// m - length of 3 bits together
// n - length of 2 bits together
// o - length of 1 bit  alone
//
//                      bits1Count|bits2shifts|1bit_offset|2bit_offset
// ..*|   .**|.**|.**|       o    |     n-1   |   2n+3m   |  2i+2+3m
// .*.|   .**|.**|.**|       o    |     n-1   |   2n+3m   |  2i+2+3m
// ...|   .**|.**|.**|       o    |     n-1   |   2n+3m   |  2i+2+3m
// ###################################################################
// ..*|   *.*|*.*|*.*|       o-1  |     n     |   2n+3m+1 |  2i+1+3m
// *..|   *.*|*.*|*.*|       o+1  |     n-1   |   2n+3m-1 |  2i+1+3m
// ...|   *.*|*.*|*.*|       o    |     n     |   2n+3m   |  2i+1+3m
// ###################################################################
// .*.|   **.|**.|**.|       o    |     n     |   2n+3m   |  2i+0+3m
// *..|   **.|**.|**.|       o    |     n     |   2n+3m   |  2i+0+3m
// ...|   **.|**.|**.|       o    |     n     |   2n+3m   |  2i+0+3m
// ###################################################################
// ..*|   ...|...|...|       o    |     n     |   2n+3m   |  xxxxxxx
// .*.|   ...|...|...|       o    |     n     |   2n+3m   |  xxxxxxx
// ...|   ...|...|...|       o    |     n     |   2n+3m   |  xxxxxxx

uint morton(uvec3 v){
  const uint warpBits      = uint(ceil(log2(float(WARP))));
  const uint warpBitsX     = uint(warpBits/2u) + uint(warpBits%2 != 0u);
  const uint warpBitsY     = uint(warpBits-warpBitsX);
  const uint warpX         = uint(1u<<warpBitsX);
  const uint warpY         = uint(1u<<warpBitsY);
  const uint clustersX     = uint(WINDOW_X/warpX) + uint(WINDOW_X%warpX != 0u);
  const uint clustersY     = uint(WINDOW_Y/warpY) + uint(WINDOW_Y%warpY != 0u);
  const uint xBits         = uint(ceil(log2(float(clustersX))));
  const uint yBits         = uint(ceil(log2(float(clustersY))));
  const uint zBits         = MIN_Z>0?MIN_Z:max(max(xBits,yBits),MIN_Z);
  const uint shortest      = min(min(xBits,yBits),zBits);
  const uint middle        = max(max(min(xBits,yBits),min(xBits,zBits)),min(yBits,zBits));
  const uint longest       = max(max(xBits,yBits),zBits);
  const uint bits3Length   = shortest;
  const uint bits2Length   = uint(middle-shortest);
  const uint bits1Length   = uint(longest-middle);
  const uint shortestAxis  = uint(uint(shortest == yBits) + uint(shortest == zBits)*2u);
  const uint longestAxis   = uint(uint(longest  == yBits) + uint(longest  == zBits)*2u);
  const uint shortestZ     = uint(shortestAxis == 2u);
  const uint shortestY     = uint(shortestAxis == 1u);
  const uint longestZ      = uint(longestAxis == 2u);
  const uint longestX      = uint(longestAxis == 0u);
  const uint isMiddle      = uint(middle > 0);
  const uint isLongest     = uint(longest > 0);
  const uint bits2Shifts   = uint(uint(bits2Length - uint(shortestZ | (shortestY & longestZ))) * isMiddle);

  const uint bits2OffsetB   = bits3Length*3u + shortestAxis;
  const uint bits2Offset00  = bits2OffsetB + 2* 0;
  const uint bits2Offset01  = bits2OffsetB + 2* 1;
  const uint bits2Offset02  = bits2OffsetB + 2* 2;
  const uint bits2Offset03  = bits2OffsetB + 2* 3;
  const uint bits2Offset04  = bits2OffsetB + 2* 4;
  const uint bits2Offset05  = bits2OffsetB + 2* 5;
  const uint bits2Offset06  = bits2OffsetB + 2* 6;
  const uint bits2Offset07  = bits2OffsetB + 2* 7;
  const uint bits2Offset08  = bits2OffsetB + 2* 8;
  const uint bits2Offset09  = bits2OffsetB + 2* 9;
  const uint bits2Offset10  = bits2OffsetB + 2*10;

  const uint bits2LMask00 = uint((1u << bits2Offset00)-1u);
  const uint bits2LMask01 = uint((1u << bits2Offset01)-1u);
  const uint bits2LMask02 = uint((1u << bits2Offset02)-1u);
  const uint bits2LMask03 = uint((1u << bits2Offset03)-1u);
  const uint bits2LMask04 = uint((1u << bits2Offset04)-1u);
  const uint bits2LMask05 = uint((1u << bits2Offset05)-1u);
  const uint bits2LMask06 = uint((1u << bits2Offset06)-1u);
  const uint bits2LMask07 = uint((1u << bits2Offset07)-1u);
  const uint bits2LMask08 = uint((1u << bits2Offset08)-1u);
  const uint bits2LMask09 = uint((1u << bits2Offset09)-1u);
  const uint bits2LMask10 = uint((1u << bits2Offset10)-1u);

  const uint bits2HMask00 = ~bits2LMask00;
  const uint bits2HMask01 = ~bits2LMask01;
  const uint bits2HMask02 = ~bits2LMask02;
  const uint bits2HMask03 = ~bits2LMask03;
  const uint bits2HMask04 = ~bits2LMask04;
  const uint bits2HMask05 = ~bits2LMask05;
  const uint bits2HMask06 = ~bits2LMask06;
  const uint bits2HMask07 = ~bits2LMask07;
  const uint bits2HMask08 = ~bits2LMask08;
  const uint bits2HMask09 = ~bits2LMask09;
  const uint bits2HMask10 = ~bits2LMask10;

  uint res = 0;
  uint vv;
  vv   = (v[0] * (0x00010001u<<0u)) & (0xFF0000FFu<<0u);
  vv   = (vv   * (0x00000101u<<0u)) & (0x0F00F00Fu<<0u);
  vv   = (vv   * (0x00000011u<<0u)) & (0xC30C30C3u<<0u);
  res |= (vv   * (0x00000005u<<0u)) & (0x49249249u<<0u);

  vv   = (v[1] * (0x00010001u<<0u)) & (0xFF0000FFu<<0u);
  vv   = (vv   * (0x00000101u<<0u)) & (0x0F00F00Fu<<0u);
  vv   = (vv   * (0x00000011u<<0u)) & (0xC30C30C3u<<0u);
  res |= (vv   * (0x00000005u<<1u)) & (0x49249249u<<1u);

  vv   = (v[2] * (0x00010001u<<0u)) & (0xFF0000FFu<<0u);
  vv   = (vv   * (0x00000101u<<0u)) & (0x0F00F00Fu<<0u);
  vv   = (vv   * (0x00000011u<<0u)) & (0xC30C30C3u<<0u);
  res |= (vv   * (0x00000005u<<2u)) & (0x49249249u<<2u);

  if(0  < bits2Shifts)res = ((res & bits2HMask00)>>1u) | (res & bits2LMask00);
  if(1  < bits2Shifts)res = ((res & bits2HMask01)>>1u) | (res & bits2LMask01);
  if(2  < bits2Shifts)res = ((res & bits2HMask02)>>1u) | (res & bits2LMask02);
  if(3  < bits2Shifts)res = ((res & bits2HMask03)>>1u) | (res & bits2LMask03);
  if(4  < bits2Shifts)res = ((res & bits2HMask04)>>1u) | (res & bits2LMask04);
  if(5  < bits2Shifts)res = ((res & bits2HMask05)>>1u) | (res & bits2LMask05);
  if(6  < bits2Shifts)res = ((res & bits2HMask06)>>1u) | (res & bits2LMask06);
  if(7  < bits2Shifts)res = ((res & bits2HMask07)>>1u) | (res & bits2LMask07);
  if(8  < bits2Shifts)res = ((res & bits2HMask08)>>1u) | (res & bits2LMask08);
  if(9  < bits2Shifts)res = ((res & bits2HMask09)>>1u) | (res & bits2LMask09);
  if(10 < bits2Shifts)res = ((res & bits2HMask10)>>1u) | (res & bits2LMask10);
  
  const uint bits1Count    = uint(bits1Length - uint(shortestY & longestX) + uint(shortestY & longestZ)) * isLongest;
  const uint bits1used     = longest - bits1Count;
  const uint bits1DstMask  = uint((1u<<(bits3Length*3u + bits2Length*2u + uint(shortestY & longestX)/*- longestZ*/)) -1u);
  const uint bits1SrcShift = bits3Length*3u + bits2Length*2u - uint(shortestY & longestZ) + uint(shortestY & longestX)  - bits1used;
  const uint bits1SrcMask  = ~((1u<<bits1used)-1u);


  if(bits1Count != 0)
    res = uint(res & bits1DstMask) | uint((v[longestAxis]&bits1SrcMask)<<bits1SrcShift);

  return res;
}
  ).");}


  {MEASURE("defines",R".(

// m - length of 3 bits together
// n - length of 2 bits together
// o - length of 1 bit  alone
//
//                      adding_1bit|2bit_shifts|1bit_offset|2bit_offset
// ..*|   .**|.**|.**|       o     |     n-1   |   2n+3m   |  2i+2+3m
// .*.|   .**|.**|.**|       o     |     n-1   |   2n+3m   |  2i+2+3m
// ...|   .**|.**|.**|       o     |     n-1   |   2n+3m   |  2i+2+3m
// ####################################################################
// ..*|   *.*|*.*|*.*|       o-1   |     n     |   2n+3m   |  2i+1+3m
// *..|   *.*|*.*|*.*|       o+1   |     n-1   |   2n+3m-1 |  2i+1+3m
// ...|   *.*|*.*|*.*|       0     |     n     |   2n+3m   |  2i+1+3m
// ####################################################################
// .*.|   **.|**.|**.|       o     |     n     |   2n+3m   |  2i+0+3m
// *..|   **.|**.|**.|       o     |     n     |   2n+3m   |  2i+0+3m
// ...|   **.|**.|**.|       o     |     n     |   2n+3m   |  2i+0+3m
// ####################################################################
// ..*|   ...|...|...|       o     |     n     |   2n+3m   |  xxxxxxx
// .*.|   ...|...|...|       o     |     n     |   2n+3m   |  xxxxxxx
// ...|   ...|...|...|       o     |     n     |   2n+3m   |  xxxxxxx

#line 409
#define REQUIRED_BITS(x)  uint(ceil(log2(float(x))))
#define DIV_ROUND_UP(x,y) (uint(x/y) + uint(x%y != 0u))

#define WARP_BITS(      w        ) REQUIRED_BITS(w)
#define WARP_BITS_X(    w        ) DIV_ROUND_UP(WARP_BITS(w),2u)
#define WARP_BITS_Y(    w        ) uint(WARP_BITS(w)-WARP_BITS_X(w))
#define WARP_X(         w        ) uint(1u<<WARP_BITS_X(w))
#define WARP_Y(         w        ) uint(1u<<WARP_BITS_Y(w))
#define UINTS_PER_WARP( w        ) uint((w)/32u)
#define CLUSTERS_X(     w,x      ) DIV_ROUND_UP((x),WARP_X(w))
#define CLUSTERS_Y(     w,  y    ) DIV_ROUND_UP((y),WARP_Y(w))
#define X_BITS(         w,x      ) REQUIRED_BITS(CLUSTERS_X(w,x))
#define Y_BITS(         w,  y    ) REQUIRED_BITS(CLUSTERS_Y(w,y))
#define Z_BITS(         w,x,y,z  ) max(max(X_BITS(w,x),Y_BITS(w,y)),z)
#define CLUSTERS_Z(     w,x,y,z  ) uint(1u<<Z_BITS(w,x,y,z))
#define ALL_BITS(       w,x,y,z  ) uint(X_BITS(w,x) + Y_BITS(w,y) + Z_BITS(w,x,y,z))
#define NOF_LEVELS(     w,x,y,z  ) DIV_ROUND_UP(ALL_BITS(w,x,y,z),WARP_BITS(w))
#define SHORTEST(       w,x,y,z  ) min(min(X_BITS(w,x),Y_BITS(w,y)),Z_BITS(w,x,y,z))
#define MIDDLE(         w,x,y,z  ) max(max(min(X_BITS(w,x),Y_BITS(w,y)),min(X_BITS(w,x),Z_BITS(w,x,y,z))),min(Y_BITS(w,y),Z_BITS(w,x,y,z)))
#define LONGEST(        w,x,y,z  ) max(max(X_BITS(w,x),Y_BITS(w,y)),Z_BITS(w,x,y,z))
#define BITS3_LENGTH(   w,x,y,z  ) SHORTEST(w,x,y,z)
#define BITS2_LENGTH(   w,x,y,z  ) uint(MIDDLE(w,x,y,z)-SHORTEST(w,x,y,z))
#define BITS1_LENGTH(   w,x,y,z  ) uint(LONGEST(w,x,y,z)-MIDDLE(w,x,y,z))
#define SHORTEST_LMASK( w,x,y,z  ) uint((1u<<(SHORTEST(w,x,y,z)*3u))-1u)
#define SHORTEST_HMASK( w,x,y,z  ) uint(~SHORTEST_LMASK(w,x,y,z))
#define MIDDLE_LMASK(   w,x,y,z  ) uint((1u<<(SHORTEST(w,x,y,z)*3u + MIDDLE(w,x,y,z)*2u))-1u)
#define MIDDLE_HMASK(   w,x,y,z  ) uint(~MIDDLE_LMASK(w,x,y,z))
#define SHORTEST_AXIS(  w,x,y,z  ) uint(uint(SHORTEST(w,x,y,z) == Y_BITS(w,y)) + uint(SHORTEST(w,x,y,z) == Z_BITS(w,x,y,z))*2u)
#define LONGEST_AXIS(   w,x,y,z  ) uint(uint(LONGEST( w,x,y,z) == Y_BITS(w,y)) + uint(LONGEST( w,x,y,z) == Z_BITS(w,x,y,z))*2u)

#line 440
#define BITS2_SHIFTS(   w,x,y,z  ) uint(uint(BITS2_LENGTH(w,x,y,z) - uint(SHORTEST_AXIS(w,x,y,z)==2u || (SHORTEST_AXIS(w,x,y,z)==1u && LONGEST_AXIS(w,x,y,z) == 2u))) * uint(MIDDLE(w,x,y,z)>0))
#define BITS2_OFFSET(   w,x,y,z,i) uint(BITS3_LENGTH(w,x,y,z)*3u + SHORTEST_AXIS(w,x,y,z) + uint(i)*2u)
#define BITS2_LMASK(    w,x,y,z,i) uint((1u << BITS2_OFFSET(w,x,y,z,i))-1u)
#define BITS2_HMASK(    w,x,y,z,i) uint(~ BITS2_LMASK(w,x,y,z,i))

#define BITS1_COUNT(    w,x,y,z  ) uint(BITS1_LENGTH(w,x,y,z) - uint(SHORTEST_AXIS(w,x,y,z)==1u && LONGEST_AXIS(w,x,y,z)==0u) + uint(SHORTEST_AXIS(w,x,y,z)==1u && LONGEST_AXIS(w,x,y,z)==2u))
#define BITS1_OFFSET(   w,x,y,z  ) uint(BITS2_LENGTH(w,x,y,z)*2u + BITS3_LENGTH(w,x,y,z)*3u - uint(SHORTEST_AXIS(w,x,y,z)==1u && LONGEST_AXIS(w,x,y,z) == 2u))
#define BITS1_DST_MASK( w,x,y,z  ) uint((1u<<(BITS3_LENGTH(w,x,y,z)*3u + 2_BITS_LENGTH(w,x,y,z)*2u - uint(LONGEST_AXIS(w,x,y,z)==2u))) -1u)
#define BITS1_SRC_SHIFT(w,x,y,z  ) uint(LONGEST)
#define BITS1_SRC_MASK   

#line 452
#define EXPAND_BITS0(V,O) ((uint(V) * (0x00010001u<<uint(O))) & (0xFF0000FFu<<uint(O)))
#define EXPAND_BITS1(V,O) ((uint(V) * (0x00000101u<<uint(O))) & (0x0F00F00Fu<<uint(O)))
#define EXPAND_BITS2(V,O) ((uint(V) * (0x00000011u<<uint(O))) & (0xC30C30C3u<<uint(O)))
#define EXPAND_BITS3(V,O) ((uint(V) * (0x00000005u<<uint(O))) & (0x49249249u<<uint(O)))

#define EXPAND_BITS(V,O) EXPAND_BITS3(EXPAND_BITS2(EXPAND_BITS1(EXPAND_BITS0(V,0),0),0),O)

#define MERGE_BITS(A,B,C) (EXPAND_BITS((A),0) | EXPAND_BITS((B),1) | EXPAND_BITS((C),2))

#line 171

uint morton(uvec3 v){

  uint res = MERGE_BITS(v[0],v[1],v[2]);

  //if(0  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 0))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 0));
  //if(1  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 1))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 1));
  //if(2  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 2))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 2));
  //if(3  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 3))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 3));
  //if(4  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 4))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 4));
  //if(5  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 5))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 5));
  //if(6  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 6))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 6));
  //if(7  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 7))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 7));
  //if(8  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 8))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 8));
  //if(9  < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 9))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z, 9));
  //if(10 < BITS2_SHIFTS(WARP,WINDOW_X,WINDOW_Y,MIN_Z))res = ((res & BITS2_HMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z,10))>>1u) | (res & BITS2_LMASK(WARP,WINDOW_X,WINDOW_Y,MIN_Z,10));

  //if(BITS1_COUNT != 0)
  //  res = uint(res & MIDDLE_LMASK) | uint((v[LONGEST_AXIS]<<uint(SHORTEST*2u + MIDDLE)) & MIDDLE_HMASK);
  
  return res;
}

  ).");}


  return EXIT_SUCCESS;
}
