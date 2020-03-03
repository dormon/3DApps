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

std::string getConfigShader(){
  std::stringstream configShader;
    
  configShader << R".(
#ifndef WARP
#define WARP 64
#endif//WARP

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

#ifndef NEAR
#define NEAR 0.01f
#endif//NEAR

#ifndef FAR
#define FAR 1000.f
#endif//FAR

#ifndef FOVY
#define FOVY 1.5707963267948966f
#endif//FOVY


#define DIV_ROUND_UP(x,y) uint(uint(uint(x)/uint(y)) + uint((uint(x) % uint(y)) != 0u))
#define BITS_REQUIRED(x) uint(ceil(log2(float(x))))
#line 31
const uint tileBitsX       = BITS_REQUIRED(TILE_X);
const uint tileBitsY       = BITS_REQUIRED(TILE_Y);
const uint tileMaskX       = uint(TILE_X-1u);
const uint tileMaskY       = uint(TILE_Y-1u);
const uint warpBits        = BITS_REQUIRED(WARP);
const uint clustersX       = DIV_ROUND_UP(WINDOW_X,TILE_X);
const uint clustersY       = DIV_ROUND_UP(WINDOW_Y,TILE_Y);
const uint xBits           = BITS_REQUIRED(clustersX);
const uint yBits           = BITS_REQUIRED(clustersY);
const uint zBits           = MIN_Z_BITS>0?MIN_Z_BITS:max(max(xBits,yBits),MIN_Z_BITS);
const uint clustersZ       = 1u << zBits;
const uint allBits         = xBits + yBits + zBits;
const uint nofLevels       = DIV_ROUND_UP(allBits,warpBits);
const uint uintsPerWarp    = uint(WARP/32u);
const uint noAxis          = 3u;
#line 47
const uint bitLength[3] = {
  min(min(xBits,yBits),zBits),
  max(max(min(xBits,yBits),min(xBits,zBits)),min(yBits,zBits)),
  max(max(xBits,yBits),zBits),
};
#line 53
const uint bitTogether[3] = {
  bitLength[0]                   ,
  uint(bitLength[1]-bitLength[0]),
  uint(bitLength[2]-bitLength[1]),
};

const uint longestAxis  = 
  bitLength[2]==zBits?2u:
  bitLength[2]==yBits?1u:
  0u;

const uint shortestAxis = 
  bitLength[0]==xBits?0u:
  bitLength[0]==yBits?1u:
  2u;
const uint middleAxis   = 
  shortestAxis==0u?(longestAxis==1u?2u:1u):
  (shortestAxis==1u?(longestAxis==0u?2u:0u):
  (longestAxis==0u?1u:0u));

const uint twoLongest[] = {
  0==shortestAxis?1u:0u,
  2==shortestAxis?1u:2u,
};

#define QUANTIZE_Z(z) clamp(uint(log(-z/NEAR) / log(1.f+2.f*tan(FOVY/2.f)/clustersY)),0u,clustersZ-1u)
#define CLUSTER_TO_Z(i) (-NEAR * exp((i)*log(1.f + 2.f*tan(FOVY/2.f)/clustersY)))

// | 2n/(R-L)  0          (R+L)/(R-L)  0          |   |x|
// | 0         2n/(T-B)   (T+B)/(T-B)  0          | * |y|
// | 0         0         -(f+n)/(f-n)  -2fn/(f-n) |   |z|
// | 0         0         -1            0          |   |1|
//
// ndcdepth<-1,1> = (-(f+n)/(f-n)*z  -2fn/(f-n)*1)/(-z)
// d = (-(f+n)/(f-n)*z  -2fn/(f-n)*1)/(-z)
// d = (f+n)/(f-n) + 2fn/(f-n)/z
// d-(f+n)/(f-n) = 2fn/(f-n)/z
// z = 2fn/(f-n) / (d-(f+n)/(f-n))
// z = 2fn/(f-n) / ((d*(f-n)-(f+n))/(f-n))
// z = 2fn / ((d*(f-n)-(f+n)))
// z = 2fn / ((d*(f-n)-f-n)))
//

#ifdef FAR_IS_INFINITE
  #define DEPTH_TO_Z(d) (2.f*NEAR    /((d) - 1.f))
  #define Z_TO_DEPTH(z) ((2.f*NEAR)/(z)+1.f)
#else
  #define DEPTH_TO_Z(d) (2.f*NEAR*FAR/((d)*(FAR-NEAR)-FAR-NEAR))
  #define Z_TO_DEPTH(z) (((2.f*NEAR*FAR/(z))+FAR+NEAR)/(FAR-NEAR))
#endif

#define WHICH_AXIS_ON_BIT(b)   uint(                                           \
    b<bitTogether[0]*3u                  ?b%3u                                :\
    b<bitTogether[0]*3u+bitTogether[1]*2u?twoLongest[(b-bitTogether[0]*3u)%2u]:\
    b<allBits                            ?longestAxis                         :\
    noAxis)
#define IS_BIT_THIS_AXIS(b,a)  uint(WHICH_AXIS_ON_BIT(b) == a)
#define SET_BIT(b,a)           uint(IS_BIT_THIS_AXIS(b,a) << b)


#line 730
const uvec3 bitPosition = uvec3(
  SET_BIT( 0u,0u)| 
  SET_BIT( 1u,0u)| 
  SET_BIT( 2u,0u)| 
  SET_BIT( 3u,0u)| 
  SET_BIT( 4u,0u)| 
  SET_BIT( 5u,0u)| 
  SET_BIT( 6u,0u)| 
  SET_BIT( 7u,0u)| 
  SET_BIT( 8u,0u)| 
  SET_BIT( 9u,0u)| 
  SET_BIT(10u,0u)| 
  SET_BIT(11u,0u)| 
  SET_BIT(12u,0u)| 
  SET_BIT(13u,0u)| 
  SET_BIT(14u,0u)| 
  SET_BIT(15u,0u)| 
  SET_BIT(16u,0u)| 
  SET_BIT(17u,0u)| 
  SET_BIT(18u,0u)| 
  SET_BIT(19u,0u)| 
  SET_BIT(20u,0u)| 
  SET_BIT(21u,0u)| 
  SET_BIT(22u,0u)| 
  SET_BIT(23u,0u)| 
  SET_BIT(24u,0u)| 
  SET_BIT(25u,0u)| 
  SET_BIT(26u,0u)| 
  SET_BIT(27u,0u)| 
  SET_BIT(28u,0u)| 
  SET_BIT(29u,0u)| 
  SET_BIT(30u,0u)| 
  SET_BIT(31u,0u),
#line 107
  SET_BIT( 0u,1u)| 
  SET_BIT( 1u,1u)| 
  SET_BIT( 2u,1u)| 
  SET_BIT( 3u,1u)| 
  SET_BIT( 4u,1u)| 
  SET_BIT( 5u,1u)| 
  SET_BIT( 6u,1u)| 
  SET_BIT( 7u,1u)| 
  SET_BIT( 8u,1u)| 
  SET_BIT( 9u,1u)| 
  SET_BIT(10u,1u)| 
  SET_BIT(11u,1u)| 
  SET_BIT(12u,1u)| 
  SET_BIT(13u,1u)| 
  SET_BIT(14u,1u)| 
  SET_BIT(15u,1u)| 
  SET_BIT(16u,1u)| 
  SET_BIT(17u,1u)| 
  SET_BIT(18u,1u)| 
  SET_BIT(19u,1u)| 
  SET_BIT(20u,1u)| 
  SET_BIT(21u,1u)| 
  SET_BIT(22u,1u)| 
  SET_BIT(23u,1u)| 
  SET_BIT(24u,1u)| 
  SET_BIT(25u,1u)| 
  SET_BIT(26u,1u)| 
  SET_BIT(27u,1u)| 
  SET_BIT(28u,1u)| 
  SET_BIT(29u,1u)| 
  SET_BIT(30u,1u)| 
  SET_BIT(31u,1u),
  SET_BIT( 0u,2u)| 
  SET_BIT( 1u,2u)| 
  SET_BIT( 2u,2u)| 
  SET_BIT( 3u,2u)| 
  SET_BIT( 4u,2u)| 
  SET_BIT( 5u,2u)| 
  SET_BIT( 6u,2u)| 
  SET_BIT( 7u,2u)| 
  SET_BIT( 8u,2u)| 
  SET_BIT( 9u,2u)| 
  SET_BIT(10u,2u)| 
  SET_BIT(11u,2u)| 
  SET_BIT(12u,2u)| 
  SET_BIT(13u,2u)| 
  SET_BIT(14u,2u)| 
  SET_BIT(15u,2u)| 
  SET_BIT(16u,2u)| 
  SET_BIT(17u,2u)| 
  SET_BIT(18u,2u)| 
  SET_BIT(19u,2u)| 
  SET_BIT(20u,2u)| 
  SET_BIT(21u,2u)| 
  SET_BIT(22u,2u)| 
  SET_BIT(23u,2u)| 
  SET_BIT(24u,2u)| 
  SET_BIT(25u,2u)| 
  SET_BIT(26u,2u)| 
  SET_BIT(27u,2u)| 
  SET_BIT(28u,2u)| 
  SET_BIT(29u,2u)| 
  SET_BIT(30u,2u)| 
  SET_BIT(31u,2u)
);
#line 172
const uvec3 levelTileBits[] = {
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-1,0))))-1u)),
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-2,0))))-1u)),
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-3,0))))-1u)),
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-4,0))))-1u)),
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-5,0))))-1u)),
  bitCount(bitPosition&((1u<<(warpBits*uint(max(int(nofLevels)-6,0))))-1u)),
};
#line 8000
const uvec3 levelTileSize[] = {                                            
  uvec3(1u)<<levelTileBits[0],
  uvec3(1u)<<levelTileBits[1],
  uvec3(1u)<<levelTileBits[2],
  uvec3(1u)<<levelTileBits[3],
  uvec3(1u)<<levelTileBits[4],
  uvec3(1u)<<levelTileBits[5],
};

const uvec3 levelTileSizeInPixels[] = {
  levelTileSize[0] << uvec3(tileBitsX,tileBitsY,0u),
  levelTileSize[1] << uvec3(tileBitsX,tileBitsY,0u),
  levelTileSize[2] << uvec3(tileBitsX,tileBitsY,0u),
  levelTileSize[3] << uvec3(tileBitsX,tileBitsY,0u),
  levelTileSize[4] << uvec3(tileBitsX,tileBitsY,0u),
  levelTileSize[5] << uvec3(tileBitsX,tileBitsY,0u),
};

const vec3 levelTileSizeClipSpace[] = {
  vec3(2.f * vec2(levelTileSizeInPixels[0].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
  vec3(2.f * vec2(levelTileSizeInPixels[1].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
  vec3(2.f * vec2(levelTileSizeInPixels[2].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
  vec3(2.f * vec2(levelTileSizeInPixels[3].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
  vec3(2.f * vec2(levelTileSizeInPixels[4].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
  vec3(2.f * vec2(levelTileSizeInPixels[5].xy) / vec2(WINDOW_X,WINDOW_Y),CLUSTER_TO_Z(levelTileSizeInPixels[0].z)),
};

const uint warpMask        = uint(WARP - 1u);
const uint floatsPerAABB   = 6u;

const uint halfWarp        = WARP / 2u;
const uint halfWarpMask    = uint(halfWarp - 1u);

const uint nodesPerLevel[6] = {
  1u << uint(max(int(allBits) - int((nofLevels-1u)*warpBits),0)),
  1u << uint(max(int(allBits) - int((nofLevels-2u)*warpBits),0)),
  1u << uint(max(int(allBits) - int((nofLevels-3u)*warpBits),0)),
  1u << uint(max(int(allBits) - int((nofLevels-4u)*warpBits),0)),
  1u << uint(max(int(allBits) - int((nofLevels-5u)*warpBits),0)),
  1u << uint(max(int(allBits) - int((nofLevels-6u)*warpBits),0)),
};
#line 70

const uint nodeLevelOffset[6] = {
  0,
  0 + nodesPerLevel[0],
  0 + nodesPerLevel[0] + nodesPerLevel[1],
  0 + nodesPerLevel[0] + nodesPerLevel[1] + nodesPerLevel[2],
  0 + nodesPerLevel[0] + nodesPerLevel[1] + nodesPerLevel[2] + nodesPerLevel[3],
  0 + nodesPerLevel[0] + nodesPerLevel[1] + nodesPerLevel[2] + nodesPerLevel[3] + nodesPerLevel[4],
};

const uint nodeLevelSizeInUints[6] = {
  max(nodesPerLevel[0] >> warpBits,1u) * uintsPerWarp,
  max(nodesPerLevel[1] >> warpBits,1u) * uintsPerWarp,
  max(nodesPerLevel[2] >> warpBits,1u) * uintsPerWarp,
  max(nodesPerLevel[3] >> warpBits,1u) * uintsPerWarp,
  max(nodesPerLevel[4] >> warpBits,1u) * uintsPerWarp,
  max(nodesPerLevel[5] >> warpBits,1u) * uintsPerWarp,
};

const uint nodeLevelOffsetInUints[6] = {
  0,
  0 + nodeLevelSizeInUints[0],
  0 + nodeLevelSizeInUints[0] + nodeLevelSizeInUints[1],
  0 + nodeLevelSizeInUints[0] + nodeLevelSizeInUints[1] + nodeLevelSizeInUints[2],
  0 + nodeLevelSizeInUints[0] + nodeLevelSizeInUints[1] + nodeLevelSizeInUints[2] + nodeLevelSizeInUints[3],
  0 + nodeLevelSizeInUints[0] + nodeLevelSizeInUints[1] + nodeLevelSizeInUints[2] + nodeLevelSizeInUints[3] + nodeLevelSizeInUints[4],
};

const uint aabbLevelSizeInFloats[6] = {
  nodesPerLevel[0] * floatsPerAABB,
  nodesPerLevel[1] * floatsPerAABB,
  nodesPerLevel[2] * floatsPerAABB,
  nodesPerLevel[3] * floatsPerAABB,
  nodesPerLevel[4] * floatsPerAABB,
  nodesPerLevel[5] * floatsPerAABB,
};

const uint aabbLevelOffsetInFloats[6] = {
  0,
  0 + aabbLevelSizeInFloats[0],
  0 + aabbLevelSizeInFloats[0] + aabbLevelSizeInFloats[1],
  0 + aabbLevelSizeInFloats[0] + aabbLevelSizeInFloats[1] + aabbLevelSizeInFloats[2],
  0 + aabbLevelSizeInFloats[0] + aabbLevelSizeInFloats[1] + aabbLevelSizeInFloats[2] + aabbLevelSizeInFloats[3],
  0 + aabbLevelSizeInFloats[0] + aabbLevelSizeInFloats[1] + aabbLevelSizeInFloats[2] + aabbLevelSizeInFloats[3] + aabbLevelSizeInFloats[4],
};

  ).";

  return configShader.str();
}

std::string getMortonShader(){
const std::string mortonShader = R".(

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

#line 46
uint morton2(uvec3 v){
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

uint morton(uvec3 v){
  const uint shortestZ     = uint(shortestAxis == 2u);
  const uint shortestY     = uint(shortestAxis == 1u);
  const uint isMiddle      = uint(bitTogether[1] > 0u);
  const uint isLongest     = uint(bitTogether[2] > 0u);
  const uint longestZ      = uint(longestAxis == 2u) * isLongest;
  const uint longestX      = uint(longestAxis == 0u) * isLongest;

  const uint bits2Shifts   = uint(uint(bitTogether[1] - uint(shortestZ | (shortestY & longestZ))) * isMiddle);

  const uint bits2OffsetB   = bitTogether[0]*3u + shortestAxis;
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

  const uint bits1Count    = uint(bitTogether[2] - uint(shortestY & longestX) + uint(shortestY & longestZ)) * isLongest;
  const uint bits1used     = bitLength[2] - bits1Count;
  const uint bits1DstMask  = uint((1u<<(bitTogether[0]*3u + bitTogether[1]*2u + uint(shortestY & longestX) - uint(longestZ & shortestY))) -1u);
  const uint bits1SrcShift = bitTogether[0]*3u + bitTogether[1]*2u - uint(shortestY & longestZ) + uint(shortestY & longestX)  - bits1used;
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

).";
return mortonShader;
}

std::string getDemortonShader(){
const std::string demortonShader = R".(
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

uvec3 demorton2(uint v){
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

uvec3 demorton(uint res){
  const uint shortestZ     = uint(shortestAxis == 2u);
  const uint shortestY     = uint(shortestAxis == 1u);
  const uint isMiddle      = uint(bitTogether[1] > 0u);
  const uint isLongest     = uint(bitTogether[2] > 0u);
  const uint longestZ      = uint(longestAxis == 2u) * isLongest;
  const uint longestX      = uint(longestAxis == 0u) * isLongest;

  const uint bits2Shifts   = uint(uint(bitTogether[1] - uint(shortestZ | (shortestY & longestZ))) * isMiddle);
  
  const uint bits2OffsetB   = bitTogether[0]*3u + shortestAxis;
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

  const uint bits1Count    = uint(bitTogether[2] - uint(shortestY & longestX) + uint(shortestY & longestZ)) * isLongest;
  const uint bits1used     = bitLength[2] - bits1Count;
  const uint bits1DstMask  = uint((1u<<(bitTogether[0]*3u + bitTogether[1]*2u + uint(shortestY & longestX) - uint(longestZ & shortestY))) -1u);
  const uint bits1SrcShift = bitTogether[0]*3u + bitTogether[1]*2u - uint(shortestY & longestZ) + uint(shortestY & longestX)  - bits1used;
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
return demortonShader;
}

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

  ss << getConfigShader();
  ss << getMortonShader();
  ss << getDemortonShader();

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
