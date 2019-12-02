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

  std::vector<uint32_t>inputData(1000*5);
  auto wrong = std::make_shared<ge::gl::Buffer>(inputData);
  wrong->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto counter = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t));
  wrong->bindBase(GL_SHADER_STORAGE_BUFFER,1);
 
  uint32_t const WARP       = 64u ;
  uint32_t const WINDOW_X   = 512u;
  uint32_t const WINDOW_Y   = 512u;
  uint32_t const MIN_Z_BITS = 9u  ;
  uint32_t const TILE_X     = 8u  ;
  uint32_t const TILE_Y     = 8u  ;

  std::stringstream ss;
  ss << "#version 450" << std::endl;



  ss << "#line " << __LINE__ << std::endl;
  ss << "layout(local_size_x=16,local_size_y=16)in;" << std::endl;
  ss << "layout(binding=0)buffer Wrong   {uint wrong   [];};" << std::endl;
  ss << "layout(binding=1)buffer Counter {uint counter [];};" << std::endl;

  ss << "#define WARP     " << WARP       << "u" << std::endl;
  ss << "#define WINDOW_X " << WINDOW_X   << "u" << std::endl;
  ss << "#define WINDOW_Y " << WINDOW_Y   << "u" << std::endl;
  ss << "#define MIN_Z    " << MIN_Z_BITS << "u" << std::endl;

  ss << R".(
#ifndef WINDOW_X
#define WINDOW_X 512
#endif//WINDOW_X

#ifndef WINDOW_Y
#define WINDOW_Y 512
#endif//WINDOW_Y

#ifndef TILE_X
#define TILE_X 8
#endif//TILE_X

#ifndef TILE_Y
#define TILE_Y 8
#endif//TILE_Y

#ifndef MIN_Z_BITS
#define MIN_Z_BITS 9
#endif//MIN_Z_BITS

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

uint morton2(uvec3 v){
  const uint clustersX     = uint(WINDOW_X/TILE_X) + uint(WINDOW_X%TILE_X != 0u);
  const uint clustersY     = uint(WINDOW_Y/TILE_Y) + uint(WINDOW_Y%TILE_Y != 0u);
  const uint xBits         = uint(ceil(log2(float(clustersX))));
  const uint yBits         = uint(ceil(log2(float(clustersY))));
  const uint zBits         = MIN_Z_BITS>0?MIN_Z_BITS:max(max(xBits,yBits),MIN_Z_BITS);
  const uint allBits = xBits + yBits + zBits;

  uint res = 0;
  uint xb[3] = {0,0,0};
  uint mb[3] = {xBits,yBits,zBits};
  uint a = 0;
  for(uint b=0;b<allBits;++b){
    res |= ((v[a]>>xb[a])&1u) << b;
    xb[a]++;
    a = (a+1u)%3u;
    if(xb[a] >= mb[a])a = (a+1u)%3u;
    if(xb[a] >= mb[a])a = (a+1u)%3u;
  }
  return res;
}

uint morton(uvec3 v){
  const uint clustersX     = uint((WINDOW_X)/(TILE_X)) + uint(((WINDOW_X)%(TILE_X)) != 0u);
  const uint clustersY     = uint((WINDOW_Y)/(TILE_Y)) + uint(((WINDOW_Y)%(TILE_Y)) != 0u);
  const uint xBits         = uint(ceil(log2(float(clustersX))));
  const uint yBits         = uint(ceil(log2(float(clustersY))));
  const uint zBits         = (MIN_Z_BITS)>0u?(MIN_Z_BITS):max(max(xBits,yBits),(MIN_Z_BITS));
  const uint shortest      = min(min(xBits,yBits),zBits);
  const uint middle        = max(max(min(xBits,yBits),min(xBits,zBits)),min(yBits,zBits));
  const uint longest       = max(max(xBits,yBits),zBits);
  const uint bits3Length   = shortest;
  const uint bits2Length   = uint(middle-shortest);
  const uint bits1Length   = uint(longest-middle);
  const uint shortestAxis  = uint(uint(shortest == yBits) + uint(shortest == zBits)*2u);
  const uint longestAxis   = clamp(uint(uint(longest  == yBits) + uint(longest  == zBits)*2u),0u,2u);
  const uint shortestZ     = uint(shortestAxis == 2u);
  const uint shortestY     = uint(shortestAxis == 1u);
  const uint longestZ      = uint(longestAxis == 2u);
  const uint longestX      = uint(longestAxis == 0u);
  const uint isMiddle      = uint(middle > 0u);
  const uint isLongest     = uint(longest > 0u);
  const uint bits2Shifts   = uint(uint(bits2Length - uint(shortestZ | (shortestY & longestZ))) * isMiddle);

  const uint bits2OffsetB   = bits3Length*3u + shortestAxis;
  const uint bits2Offset00  = bits2OffsetB + 2u* 0u;
  const uint bits2Offset01  = bits2OffsetB + 2u* 1u;
  const uint bits2Offset02  = bits2OffsetB + 2u* 2u;
  const uint bits2Offset03  = bits2OffsetB + 2u* 3u;
  const uint bits2Offset04  = bits2OffsetB + 2u* 4u;
  const uint bits2Offset05  = bits2OffsetB + 2u* 5u;
  const uint bits2Offset06  = bits2OffsetB + 2u* 6u;
  const uint bits2Offset07  = bits2OffsetB + 2u* 7u;
  const uint bits2Offset08  = bits2OffsetB + 2u* 8u;
  const uint bits2Offset09  = bits2OffsetB + 2u* 9u;
  const uint bits2Offset10  = bits2OffsetB + 2u*10u;

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
).";

  ss << "#line " << __LINE__ << std::endl;
  ss << R".(
  void main(){
    uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
    uint mm0 = morton(pos);
    uint mm1 = morton2(pos);
    if(mm0 != mm1){
      uint w = atomicAdd(counter[0],1);
      if(w < 1000){
        wrong[w*5 + 0] = pos[0];
        wrong[w*5 + 1] = pos[1];
        wrong[w*5 + 2] = pos[2];
        wrong[w*5 + 3] = mm0;
        wrong[w*5 + 4] = mm1;
      }
    }
  }
  ).";

  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

  counter->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  wrong->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);

  prg->use();
  glDispatchCompute(512/16,512/16,512);
  glFinish();

  std::vector<uint32_t>wData;
  std::vector<uint32_t>cData;
  wrong->getData(wData);
  counter->getData(cData);

  for(size_t i=0;i<10;++i){
    std::cerr << "x: " << wData[i*5+0] << " y: " << wData[i*5+1] << " z: " << wData[i*5+2] << " - m: " << wData[i*5+3] << " m2: " << wData[i*5+4] << std::endl;
  }




  return EXIT_SUCCESS;
}
