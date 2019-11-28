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
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  size_t const DATA = 1024*1024;

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

#define UINTS_PER_WARP( w        ) uint((w)/32u)

#line 452
#define EXPAND_BITS0(V,O) ((uint(V) * (0x00010001u<<uint(O))) & (0xFF0000FFu<<uint(O)))
#define EXPAND_BITS1(V,O) ((uint(V) * (0x00000101u<<uint(O))) & (0x0F00F00Fu<<uint(O)))
#define EXPAND_BITS2(V,O) ((uint(V) * (0x00000011u<<uint(O))) & (0xC30C30C3u<<uint(O)))
#define EXPAND_BITS3(V,O) ((uint(V) * (0x00000005u<<uint(O))) & (0x49249249u<<uint(O)))

#define EXPAND_BITS(V,O) EXPAND_BITS3(EXPAND_BITS2(EXPAND_BITS1(EXPAND_BITS0(V,0),0),0),O)

#define MERGE_BITS(A,B,C) (EXPAND_BITS((A),0) | EXPAND_BITS((B),1) | EXPAND_BITS((C),2))

#line 123

uint morton(uvec3 v){

  const uint warpBits      = REQUIRED_BITS(WARP);
  const uint warpBitsX     = DIV_ROUND_UP(warpBits,2u);
  const uint warpBitsY     = uint(warpBits-warpBitsX);
  const uint warpX         = uint(1u<<warpBitsX);
  const uint warpY         = uint(1u<<warpBitsY);
  const uint clustersX     = DIV_ROUND_UP(WINDOW_X,warpX);
  const uint clustersY     = DIV_ROUND_UP(WINDOW_X,warpY);
  const uint xBits         = REQUIRED_BITS(clustersX);
  const uint yBits         = REQUIRED_BITS(clustersY);
  const uint zBits         = max(max(xBits,yBits),MIN_Z);
  const uint clustersZ     = uint(1u<<zBits);
  const uint allBits       = uint(xBits + yBits + zBits);
  const uint shortest      = min(min(xBits,yBits),zBits);
  const uint middle        = max(max(min(xBits,yBits),min(xBits,zBits)),min(yBits,zBits));
  const uint longest       = max(max(xBits,yBits),zBits);
  const uint bits3Length   = shortest;
  const uint bits2Length   = uint(middle-shortest);
  const uint bits1Length   = uint(longest-middle);
  const uint shortestLMask = uint((1u<<(shortest*3u))-1u);
  const uint shortestHMask = uint(~shortestLMask);
  const uint middleLMask   = uint((1u<<(shortest*3u + middle*2u))-1u);
  const uint middleHMask   = uint(~middleLMask);
  const uint shortestAxis  = uint(uint(shortest == yBits) + uint(shortest == zBits)*2u);
  const uint longestAxis   = uint(uint(longest  == yBits) + uint(longest  == zBits)*2u);
  const uint bits2Shifts   = uint(uint(bits2Length - uint(shortestAxis==2u || (shortestAxis==1u && longestAxis == 2u))) * uint(middle>0));
  #define BITS2_OFFSET(i) uint(bits3Length*3u + shortestAxis + uint(i)*2u)
  #define BITS2_LMASK( i) uint((1u << BITS2_OFFSET(i))-1u)
  #define BITS2_HMASK( i) uint(~ BITS2_LMASK(i))
  
  const uint bits1Count    = uint(bits1Length - uint(shortestAxis == 1u && longestAxis == 0u) + uint(shortestAxis == 1u && longestAxis == 2u));
  const uint bits1DstMask  = uint((1u<<(bits3Length*3u + bits2Length*2u - uint(longestAxis == 2u))) -1u);
  const uint bits1SrcShift = uint(longest);
  const uint bits1SrcMask  = 0; 

  uint res = MERGE_BITS(v[0],v[1],v[2]);

  if(0  < bits2Shifts)res = ((res & BITS2_HMASK( 0))>>1u) | (res & BITS2_LMASK( 0));
  if(1  < bits2Shifts)res = ((res & BITS2_HMASK( 1))>>1u) | (res & BITS2_LMASK( 1));
  if(2  < bits2Shifts)res = ((res & BITS2_HMASK( 2))>>1u) | (res & BITS2_LMASK( 2));
  if(3  < bits2Shifts)res = ((res & BITS2_HMASK( 3))>>1u) | (res & BITS2_LMASK( 3));
  if(4  < bits2Shifts)res = ((res & BITS2_HMASK( 4))>>1u) | (res & BITS2_LMASK( 4));
  if(5  < bits2Shifts)res = ((res & BITS2_HMASK( 5))>>1u) | (res & BITS2_LMASK( 5));
  if(6  < bits2Shifts)res = ((res & BITS2_HMASK( 6))>>1u) | (res & BITS2_LMASK( 6));
  if(7  < bits2Shifts)res = ((res & BITS2_HMASK( 7))>>1u) | (res & BITS2_LMASK( 7));
  if(8  < bits2Shifts)res = ((res & BITS2_HMASK( 8))>>1u) | (res & BITS2_LMASK( 8));
  if(9  < bits2Shifts)res = ((res & BITS2_HMASK( 9))>>1u) | (res & BITS2_LMASK( 9));
  if(10 < bits2Shifts)res = ((res & BITS2_HMASK(10))>>1u) | (res & BITS2_LMASK(10));

  if(bits1Count != 0)
    res = uint(res & bits1DstMask) | uint((v[longestAxis]<<bits1SrcShift) & bits1SrcMask);
  
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
