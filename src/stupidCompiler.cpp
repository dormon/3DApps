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

  size_t const DATA = 1024*1024*4;
  auto buf = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*DATA);
  buf->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto const measure = [&](std::string const&name,std::string const&src,size_t line){
    std::stringstream ss;
    ss << "#version 450" << std::endl;
    ss << "#line " << line << std::endl;
    ss << src;
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
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");}

  //std::vector<uint32_t>d(10);
  //buf->getData(d);
  //for(auto x:d)
  //  std::cerr << x << std::endl;

  {MEASURE("simple",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = 32;
  }
  ).");}



  {MEASURE("constLoop",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a = uint(32*sin(a*32));
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("constLoop2",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    
    for(uint i=0;i<1000;++i)
      a += i;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("constLoop3",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;
    for(uint i=0;i<1000;++i)
      a += uint(sin(i*32)*32);
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("const array marked as const",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    const uint sw[10] = {
    6,
    2,
    3,
    7,
    0,
    8,
    9,
    1,
    4,
    5,
    };

    uint a = 3;
    DUP4096(a = sw[a];)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("const array not marked as const",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    uint sw[10];
    sw[0] = 6;
    sw[1] = 2;
    sw[2] = 3;
    sw[3] = 7;
    sw[4] = 0;
    sw[5] = 8;
    sw[6] = 9;
    sw[7] = 1;
    sw[8] = 4;
    sw[9] = 5;

    uint a = 3;
    DUP4096(a = sw[a];)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("reading from const array",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    const uint sw[10] = {
    6,
    2,
    3,
    7,
    0,
    8,
    9,
    1,
    4,
    5,
    };

    uint a = 3;
    DUP4096(a+=sw[3];a+=sw[8];a+=sw[3];a+=sw[1];a+=sw[0];)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("if(false) a lot",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    uint a  = 32;
    DUP4096(if(0>1)a += 1;)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("for(uint i=0;i<10000;++i){if(0>1)a+=1;}",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;

    for(uint i=0;i<10000;++i)
      if(0>1)a += 1;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("for(uint i=0;i<10000;++i){if(i>100000)a+=1;}",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;

    for(uint i=0;i<10000;++i)
      if(i>100000)a += 1;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("a+=1; once",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a  = 32;

    a += 1;
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("a+=1;a+=1;a+=1;... a lot",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    uint a  = 32;

    DUP4096(a += 1;)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("a+=1;a+=1;a+=1;... A LOT!",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define DUP0(x) x x
  #define DUP1(x) DUP0(x) DUP0(x)
  #define DUP2(x) DUP1(x) DUP1(x)
  #define DUP4(x) DUP2(x) DUP2(x)
  #define DUP8(x) DUP4(x) DUP4(x)
  #define DUP16(x) DUP8(x) DUP8(x)
  #define DUP32(x) DUP16(x) DUP16(x)
  #define DUP64(x) DUP32(x) DUP32(x)
  #define DUP128(x) DUP64(x) DUP64(x)
  #define DUP256(x) DUP128(x) DUP128(x)
  #define DUP512(x) DUP256(x) DUP256(x)
  #define DUP1024(x) DUP512(x) DUP512(x)
  #define DUP2048(x) DUP1024(x) DUP1024(x)
  #define DUP4096(x) DUP2048(x) DUP2048(x)

  void main(){
    uint a  = 32;

    DUP4096(a += 1;)
    DUP4096(a += 1;)
    DUP4096(a += 1;)
      
    data[gl_GlobalInvocationID.x] = a;
  }
  ).");}

  {MEASURE("a lot of addition",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  #define C1(x,y)  (x      +y      )
  #define C2(x,y)  (C1(x,y)+C1(x,y))
  #define C4(x,y)  (C2(x,y)+C2(x,y))
  #define C8(x,y)  (C4(x,y)+C4(x,y))
  #define C16(x,y) (C8(x,y)+C8(x,y))
  #define C32(x,y) (C16(x,y)+C16(x,y))
  #define C64(x,y) (C32(x,y)+C32(x,y))
  #define C128(x,y) (C64(x,y)+C64(x,y))
  #define C256(x,y) (C128(x,y)+C128(x,y))
  #define C512(x,y) (C256(x,y)+C256(x,y))
  #define C1024(x,y) (C512(x,y)+C512(x,y))

  void main(){
    data[gl_GlobalInvocationID.x] = C1024(23,10);
  }

  ).");}

  {MEASURE("function",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    return a*2;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("function loop",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    for(uint i=0;i<1000;++i)
      a += uint(sin(i*32)*32);
    return a;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("function",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  uint get(uint a){
    uint b = a*2;
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    b += 1;
    b = uint(sin(float(b))*32);
    b = uint(sin(float(b))*32);
    return b;
  }

  void main(){
    data[gl_GlobalInvocationID.x] = get(32);
  }

  ).");}

  {MEASURE("variables",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    uint a = 32;
    a *= 32;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a += 2;
    a *= 4;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    a = uint(sin(float(a)))*32;
    data[gl_GlobalInvocationID.x] = a;
  }

  ).");}

  {MEASURE("const <",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

  void main(){
    data[gl_GlobalInvocationID.x] = uint(23*sin(21*float(gl_WorkGroupSize.x < 300)));
  }

  ).");}

  {MEASURE("morton",R".(
  layout(local_size_x=256)in;

  layout(binding=0)buffer Data{uint data[];};

#ifndef WARP
#define WARP 64u
#endif//WARP

#ifndef WINDOW_X
#define WINDOW_X 512u
#endif//WINDOW_X

#ifndef WINDOW_Y
#define WINDOW_Y 512u
#endif//WINDOW_Y

#ifndef MIN_Z
#define MIN_Z 9u
#endif//MIN_Z

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

/*
uint morton(uint3 v){

  //uint res = MERGE_BITS(v[0],v[1],v[2]);

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
  return 3;
}
*/


  void main(){

    data[gl_GlobalInvocationID.x] = BITS2_OFFSET(WARP,WINDOW_X,WINDOW_Y,MIN_Z,3);
  }

  ).");}

  return EXIT_SUCCESS;
}
