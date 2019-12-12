#include <Simple3DApp/Application.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <TxtUtils/TxtUtils.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <Timer.h>
#include <bitset>
#include <iomanip>

class CSCompiler: public simple3DApp::Application{
 public:
  CSCompiler(int argc, char* argv[]) : Application(argc, argv) {}
  virtual ~CSCompiler(){}
};

using namespace ge::gl;

int main(int argc,char*argv[]){
  CSCompiler app{argc, argv};

  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  uint32_t const WINDOW_X = args->getu32("-WX",512,"windowX size");
  uint32_t const WINDOW_Y = args->getu32("-WY",512,"windowY size");
  uint32_t const MIN_Z_BITS = args->getu32("-MZ",9u,"min Z bits");
  bool printHelp = args->isPresent("-h", "prints this help");
  if (printHelp || !args->validate()) {
    std::cerr << args->toStr();
    exit(0);
  }

  auto wrong = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t)*1000*5);
  wrong->bindBase(GL_SHADER_STORAGE_BUFFER,0);

  auto counter = std::make_shared<ge::gl::Buffer>(sizeof(uint32_t));
  counter->bindBase(GL_SHADER_STORAGE_BUFFER,1);
 
  uint32_t const TILE_X     = 8u  ;
  uint32_t const TILE_Y     = 8u  ;

  auto max = [](uint32_t y,uint32_t x){return x>y?x:y;};
  const uint32_t clustersX     = uint((WINDOW_X)/(TILE_X)) + uint(((WINDOW_X)%(TILE_X)) != 0u);
  const uint32_t clustersY     = uint((WINDOW_Y)/(TILE_Y)) + uint(((WINDOW_Y)%(TILE_Y)) != 0u);
  const uint32_t xBits         = uint(ceil(log2(float(clustersX))));
  const uint32_t yBits         = uint(ceil(log2(float(clustersY))));
  const uint32_t zBits         = (MIN_Z_BITS)>0u?(MIN_Z_BITS):max(max(xBits,yBits),(MIN_Z_BITS));
  const uint32_t clustersZ = 1u << zBits;

  std::stringstream ss;
  ss << "#version 450" << std::endl;



  ss << "#line " << __LINE__ << std::endl;
  ss << "layout(local_size_x=8,local_size_y=8,local_size_z=8)in;" << std::endl;
  ss << "layout(binding=0)buffer Wrong   {uint wrong   [];};" << std::endl;
  ss << "layout(binding=1)buffer Counter {uint counter [];};" << std::endl;

  ss << "#define WINDOW_X   " << WINDOW_X   << "u" << std::endl;
  ss << "#define WINDOW_Y   " << WINDOW_Y   << "u" << std::endl;
  ss << "#define TILE_X     " << TILE_X     << "u" << std::endl;
  ss << "#define TILE_Y     " << TILE_Y     << "u" << std::endl;
  ss << "#define MIN_Z_BITS " << MIN_Z_BITS << "u" << std::endl;

  ss << R".(
//#ifndef WINDOW_X
//#define WINDOW_X 512
//#endif//WINDOW_X
//
//#ifndef WINDOW_Y
//#define WINDOW_Y 512
//#endif//WINDOW_Y
//
//#ifndef TILE_X
//#define TILE_X 8
//#endif//TILE_X
//
//#ifndef TILE_Y
//#define TILE_Y 8
//#endif//TILE_Y
//
//#ifndef MIN_Z_BITS
//#define MIN_Z_BITS 9
//#endif//MIN_Z_BITS

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
  uint res = 0;
  uint counters[3] = {0,0,0};
  const uint limits[3] = {xBits,yBits,zBits};
  const uint allBits = xBits + yBits + zBits;
  uint a = 0;
  for(uint b=0;b<allBits;++b){
    res |= ((v[a]>>counters[a])&1u) << b;
    counters[a]++;
    a = (a+1u)%3u;
    if(counters[a] >= limits[a])a = (a+1u)%3u;
    if(counters[a] >= limits[a])a = (a+1u)%3u;
  }
  return res;
}

uvec3 demorton2(uint v){
  const uint clustersX     = uint(WINDOW_X/TILE_X) + uint(WINDOW_X%TILE_X != 0u);
  const uint clustersY     = uint(WINDOW_Y/TILE_Y) + uint(WINDOW_Y%TILE_Y != 0u);
  const uint xBits         = uint(ceil(log2(float(clustersX))));
  const uint yBits         = uint(ceil(log2(float(clustersY))));
  const uint zBits         = MIN_Z_BITS>0?MIN_Z_BITS:max(max(xBits,yBits),MIN_Z_BITS);
  uvec3 res = uvec3(0);
  uint counters[3] = {0,0,0};
  const uint limits[3] = {xBits,yBits,zBits};
  const uint allBits = xBits + yBits + zBits;
  uint a = 0;
  for(uint b=0;b<allBits;++b){
    res[a] |= ((v>>b)&1u) << counters[a];
    counters[a]++;
    a = (a+1u)%3u;
    if(counters[a] >= limits[a])a = (a+1u)%3u;
    if(counters[a] >= limits[a])a = (a+1u)%3u;
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
  const uint longestAxis   = uint(uint(longest  == yBits) + uint(longest  == zBits)*2u);
  const uint shortestZ     = uint(shortestAxis == 2u);
  const uint shortestY     = uint(shortestAxis == 1u);
  const uint isMiddle      = uint(bits2Length > 0u);
  const uint isLongest     = uint(bits1Length > 0u);
  const uint longestZ      = uint(longestAxis == 2u) * isLongest;
  const uint longestX      = uint(longestAxis == 0u) * isLongest;
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

  const uint bits2HMask00 = (~bits2LMask00)<<1u;
  const uint bits2HMask01 = (~bits2LMask01)<<1u;
  const uint bits2HMask02 = (~bits2LMask02)<<1u;
  const uint bits2HMask03 = (~bits2LMask03)<<1u;
  const uint bits2HMask04 = (~bits2LMask04)<<1u;
  const uint bits2HMask05 = (~bits2LMask05)<<1u;
  const uint bits2HMask06 = (~bits2LMask06)<<1u;
  const uint bits2HMask07 = (~bits2LMask07)<<1u;
  const uint bits2HMask08 = (~bits2LMask08)<<1u;
  const uint bits2HMask09 = (~bits2LMask09)<<1u;
  const uint bits2HMask10 = (~bits2LMask10)<<1u;

  const uint bits1Count    = uint(bits1Length - uint(shortestY & longestX) + uint(shortestY & longestZ)) * isLongest;
  const uint bits1used     = longest - bits1Count;
  const uint bits1DstMask  = uint((1u<<(bits3Length*3u + bits2Length*2u + uint(shortestY & longestX) - uint(longestZ & shortestY))) -1u);
  const uint bits1SrcShift = bits3Length*3u + bits2Length*2u - uint(shortestY & longestZ) + uint(shortestY & longestX)  - bits1used;
  const uint bits1SrcMask  = ~((1u<<bits1used)-1u);

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

  if(bits1Count != 0)
    res = uint(res & bits1DstMask) | uint((v[longestAxis]&bits1SrcMask)<<bits1SrcShift);

  return res;
}

uvec3 demorton(uint res){
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
  const uint isMiddle      = uint(bits2Length > 0u);
  const uint isLongest     = uint(bits1Length > 0u);
  const uint longestZ      = uint(longestAxis == 2u) * isLongest;
  const uint longestX      = uint(longestAxis == 0u) * isLongest;
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

  const uint bits2HMask00 = (~bits2LMask00)<<1u;
  const uint bits2HMask01 = (~bits2LMask01)<<1u;
  const uint bits2HMask02 = (~bits2LMask02)<<1u;
  const uint bits2HMask03 = (~bits2LMask03)<<1u;
  const uint bits2HMask04 = (~bits2LMask04)<<1u;
  const uint bits2HMask05 = (~bits2LMask05)<<1u;
  const uint bits2HMask06 = (~bits2LMask06)<<1u;
  const uint bits2HMask07 = (~bits2LMask07)<<1u;
  const uint bits2HMask08 = (~bits2LMask08)<<1u;
  const uint bits2HMask09 = (~bits2LMask09)<<1u;
  const uint bits2HMask10 = (~bits2LMask10)<<1u;

  const uint bits1Count    = uint(bits1Length - uint(shortestY & longestX) + uint(shortestY & longestZ)) * isLongest;
  const uint bits1used     = longest - bits1Count;
  const uint bits1DstMask  = uint((1u<<(bits3Length*3u + bits2Length*2u + uint(shortestY & longestX) - uint(longestZ & shortestY))) -1u);
  const uint bits1SrcShift = bits3Length*3u + bits2Length*2u - uint(shortestY & longestZ) + uint(shortestY & longestX)  - bits1used;
  const uint bits1SrcMask  = ~((1u<<bits1used)-1u);

  uvec3 v = uvec3(0);

  uint last = 0;
  if(bits1Count != 0){
    last |= (res >> bits1SrcShift) & bits1SrcMask;
    res &= bits1DstMask;
  }

  if(10 < bits2Shifts)res = ((res<<1u)&bits2HMask10) | (res & bits2LMask10);
  if(9  < bits2Shifts)res = ((res<<1u)&bits2HMask09) | (res & bits2LMask09);
  if(8  < bits2Shifts)res = ((res<<1u)&bits2HMask08) | (res & bits2LMask08);
  if(7  < bits2Shifts)res = ((res<<1u)&bits2HMask07) | (res & bits2LMask07);
  if(6  < bits2Shifts)res = ((res<<1u)&bits2HMask06) | (res & bits2LMask06);
  if(5  < bits2Shifts)res = ((res<<1u)&bits2HMask05) | (res & bits2LMask05);
  if(4  < bits2Shifts)res = ((res<<1u)&bits2HMask04) | (res & bits2LMask04);
  if(3  < bits2Shifts)res = ((res<<1u)&bits2HMask03) | (res & bits2LMask03);
  if(2  < bits2Shifts)res = ((res<<1u)&bits2HMask02) | (res & bits2LMask02);
  if(1  < bits2Shifts)res = ((res<<1u)&bits2HMask01) | (res & bits2LMask01);
  if(0  < bits2Shifts)res = ((res<<1u)&bits2HMask00) | (res & bits2LMask00);

  v[2] = (res & 0x24924924u)>>2u;
  v[1] = (res & 0x92492492u)>>1u;
  v[0] = (res & 0x49249249u)>>0u;

  v[2] = (v[2] | (v[2]>> 2u)) & 0xc30c30c3u;
  v[2] = (v[2] | (v[2]>> 4u)) & 0x0f00f00fu;
  v[2] = (v[2] | (v[2]>> 8u)) & 0xff0000ffu;
  v[2] = (v[2] | (v[2]>>16u)) & 0x0000ffffu;

  v[1] = (v[1] | (v[1]>> 2u)) & 0xc30c30c3u;
  v[1] = (v[1] | (v[1]>> 4u)) & 0x0f00f00fu;
  v[1] = (v[1] | (v[1]>> 8u)) & 0xff0000ffu;
  v[1] = (v[1] | (v[1]>>16u)) & 0x0000ffffu;

  v[0] = (v[0] | (v[0]>> 2u)) & 0xc30c30c3u;
  v[0] = (v[0] | (v[0]>> 4u)) & 0x0f00f00fu;
  v[0] = (v[0] | (v[0]>> 8u)) & 0xff0000ffu;
  v[0] = (v[0] | (v[0]>>16u)) & 0x0000ffffu;
 
  if(bits1Count != 0){
  v[longestAxis] |= last;
  }

  return v;
}

).";

  ss << "#line " << __LINE__ << std::endl;
  ss << R".(
  uniform uint mode = 0;
  void main(){
    uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
    
    if(mode == 0){
      uvec3 apos = pos;
      uvec3 bpos = pos;
      uint mm0 = morton (apos);
      uint mm1 = morton2(bpos);
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


    if(mode == 1){
      uint mm = morton2(pos);
      uvec3 de = demorton2(mm);
      if(de != pos){
        uint w = atomicAdd(counter[0],1);
        if(w < 1000){
          wrong[w*7 + 0] = pos[0];
          wrong[w*7 + 1] = pos[1];
          wrong[w*7 + 2] = pos[2];
          wrong[w*7 + 3] = mm ;
          wrong[w*7 + 4] = de [0];
          wrong[w*7 + 5] = de [1];
          wrong[w*7 + 6] = de [2];
        }
      }
    }

    if(mode == 2){
      uint mm = morton(pos);
      uvec3 de = demorton(mm);
      if(de != pos){
        uint w = atomicAdd(counter[0],1);
        if(w < 1000){
          wrong[w*7 + 0] = pos[0];
          wrong[w*7 + 1] = pos[1];
          wrong[w*7 + 2] = pos[2];
          wrong[w*7 + 3] = mm ;
          wrong[w*7 + 4] = de [0];
          wrong[w*7 + 5] = de [1];
          wrong[w*7 + 6] = de [2];
        }
      }
    }

    //bool dem  = demorton (morton (bpos)) != bpos;
  }
  ).";

  auto prg = std::make_shared<ge::gl::Program>(std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER,ss.str()));

  counter->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  wrong->clear(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
  glFinish();

  prg->use();
  auto divRoundUp = [](uint32_t x,uint32_t y){return (x/y) + (uint32_t)(x%y>0);};
  std::cerr << "clustersX: " << divRoundUp(clustersX,8)<< std::endl;
  std::cerr << "clustersY: " << divRoundUp(clustersY,8)<< std::endl;
  std::cerr << "clustersZ: " << divRoundUp(clustersZ,8)<< std::endl;

 
  uint32_t mode = 0;


  auto mortonCPU =[&](uint32_t v[3]){
    const uint clustersX     = uint(WINDOW_X/TILE_X) + uint(WINDOW_X%TILE_X != 0u);
    const uint clustersY     = uint(WINDOW_Y/TILE_Y) + uint(WINDOW_Y%TILE_Y != 0u);
    const uint xBits         = uint(ceil(log2(float(clustersX))));
    const uint yBits         = uint(ceil(log2(float(clustersY))));
    const uint zBits         = MIN_Z_BITS>0?MIN_Z_BITS:max(max(xBits,yBits),MIN_Z_BITS);
    const uint allBits = xBits + yBits + zBits;

    uint32_t res = 0;
    uint32_t xb[3] = {0,0,0};
    uint32_t mb[3] = {xBits,yBits,zBits};
    //std::cerr << "xBits: " << xBits << std::endl;
    //std::cerr << "yBits: " << yBits << std::endl;
    //std::cerr << "zBits: " << zBits << std::endl;
    uint32_t a = 0;
    for(uint b=0;b<allBits;++b){
      res |= ((v[a]>>xb[a])&1u) << b;
      xb[a]++;

      a = (a+1u)%3u;
      if(xb[a] >= mb[a])a = (a+1u)%3u;
      if(xb[a] >= mb[a])a = (a+1u)%3u;
    }
    return res;
  };


  if(mode == 0){
    prg->set1ui("mode",mode);
    glDispatchCompute(divRoundUp(clustersX,8),divRoundUp(clustersY,8),divRoundUp(clustersZ,8));
    glFinish();

    std::vector<uint32_t>wData;
    std::vector<uint32_t>cData;
    wrong->getData(wData);
    counter->getData(cData);



    std::cerr << "counter: " << cData[0] << std::endl;
    for(size_t i=0;i<100&&i<cData[0];++i){
      uint32_t v[3];
      v[0] = wData[i*5+0];
      v[1] = wData[i*5+1];
      v[2] = wData[i*5+2];

      std::cerr << "x: ";
      std::cerr << std::setw(4) << std::setfill(' ') << v[0] << " " << std::bitset<10>(v[0]);
      std::cerr << " y: ";
      std::cerr << std::setw(4) << std::setfill(' ') << v[1] << " " << std::bitset<10>(v[1]);
      std::cerr << " z: ";
      std::cerr << std::setw(4) << std::setfill(' ') << v[2] << " " << std::bitset<10>(v[2]);
      std::cerr << " - m: " << std::bitset<32>(wData[i*5+3]) << " m2: " << std::bitset<32>(wData[i*5+4]);
      std::cerr << " mc: " << std::bitset<32>(mortonCPU(v)) << std::endl;;

    }
  }

  mode = 1;
  if(mode == 1){
    prg->set1ui("mode",mode);
    glDispatchCompute(divRoundUp(clustersX,8),divRoundUp(clustersY,8),divRoundUp(clustersZ,8));
    glFinish();

    std::vector<uint32_t>wData;
    std::vector<uint32_t>cData;
    wrong->getData(wData);
    counter->getData(cData);

    std::cerr << "counter: " << cData[0] << std::endl;
    for(size_t i=0;i<100&&i<cData[0];++i){
      uint32_t v[3];
      v[0] = wData[i*7+0];
      v[1] = wData[i*7+1];
      v[2] = wData[i*7+2];
      uint32_t d[3];
      d[0] = wData[i*7+4];
      d[1] = wData[i*7+5];
      d[2] = wData[i*7+6];
      uint32_t m = wData[i*7+3];

      std::cerr << "v: " << v[0] << " " << v[1] << " " << v[2] << std::endl;
      std::cerr << "d: " << d[0] << " " << d[1] << " " << d[2] << std::endl;
      std::cerr << std::bitset<32>(m) << std::endl;

    }
  }

  mode = 2;
  if(mode == 2){
    prg->set1ui("mode",mode);
    glDispatchCompute(divRoundUp(clustersX,8),divRoundUp(clustersY,8),divRoundUp(clustersZ,8));
    glFinish();

    std::vector<uint32_t>wData;
    std::vector<uint32_t>cData;
    wrong->getData(wData);
    counter->getData(cData);

    std::cerr << "counter: " << cData[0] << std::endl;
    for(size_t i=0;i<100&&i<cData[0];++i){
      uint32_t v[3];
      v[0] = wData[i*7+0];
      v[1] = wData[i*7+1];
      v[2] = wData[i*7+2];
      uint32_t d[3];
      d[0] = wData[i*7+4];
      d[1] = wData[i*7+5];
      d[2] = wData[i*7+6];
      uint32_t m = wData[i*7+3];

      std::cerr << "v: " << v[0] << " " << v[1] << " " << v[2] << std::endl;
      std::cerr << "d: " << d[0] << " " << d[1] << " " << d[2] << std::endl;
      std::cerr << std::bitset<32>(m) << std::endl;

    }
  }




  return EXIT_SUCCESS;
}
