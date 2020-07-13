#include <ArgumentViewer/ArgumentViewer.h>
#include<Simple3DApp/Application.h>
#include<Vars/Vars.h>
#include<geGL/StaticCalls.h>
#include<geGL/geGL.h>
#include<BasicCamera/FreeLookCamera.h>
#include<BasicCamera/PerspectiveCamera.h>
#include <BasicCamera/OrbitCamera.h>
#include<Barrier.h>
#include<imguiSDL2OpenGL/imgui.h>
#include<imguiVars/imguiVars.h>
#include<drawGrid.h>
#include <FunctionPrologue.h>
/*#include <assimp/Importer.hpp>
#include <assimp/scene.h>*/
#include <SDL2CPP/Exception.h>
#include <Timer.h>
#include <gpuDecoder.h>
#include<string>
#include<thread>
#include<mutex>
#include<condition_variable>
#include <fstream>

constexpr float FRAME_LIMIT{1.0f/24};
constexpr uint32_t FOCUSMAP_DIV{1};
constexpr bool MEASURE_TIME{1};
constexpr bool STATISTICS{0};
constexpr bool TEXTURE_STATISTICS{0};

//TODO not multiple of local size resolution cases
constexpr int WARP_SIZE{32};
constexpr int LOCAL_SIZE_X{WARP_SIZE};
constexpr int LOCAL_SIZE_Y{8};

class LightFields: public simple3DApp::Application
{
public:
    LightFields(int argc, char* argv[]) : Application(argc, argv) {}
    virtual ~LightFields(){}
    virtual void draw() override;

    vars::Vars vars;

    virtual void                init() override;
    virtual void                deinit() override {vars.reCreate<bool>("mainRuns", false);}
    virtual void                mouseMove(SDL_Event const& event) override;
    virtual void                key(SDL_Event const& e, bool down) override;
    virtual void                resize(uint32_t x,uint32_t y) override;
};

void addProgram(vars::Vars&vars, std::string name, std::string code1, std::string code2)
{
    std::map<std::string, std::string> substitutions ={
        {"LSX",std::to_string(LOCAL_SIZE_X)},
        {"LSY",std::to_string(LOCAL_SIZE_Y)},
        {"GSX",std::to_string(vars.get<glm::ivec2>("gridSize")->x)}
    };
    for(const auto& [key, value] : substitutions)
    {
        size_t pos = code1.find(key);
        if(pos != std::string::npos)
            code1.replace(code1.find(key), key.length(), value);
        pos = code1.find(key);
        if(pos != std::string::npos)
            code2.replace(code2.find(key), key.length(), value);
    }
    if(!code2.empty())
    {
        auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER, "#version 450\n", code1);
        auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER, "#version 450\n", code2);
        auto program = vars.reCreate<ge::gl::Program>(name,vs,fs);
        if(program->getLinkStatus() == GL_FALSE)
            throw std::runtime_error("Cannot link shader program.");
    }
    else
    {
        auto cs = std::make_shared<ge::gl::Shader>(GL_COMPUTE_SHADER, "#version 450\n", code1);
        auto program = vars.reCreate<ge::gl::Program>(name,cs);
        if(program->getLinkStatus() == GL_FALSE)
            throw std::runtime_error("Cannot link shader program.");
    }
}

void createProgram(vars::Vars&vars)
{
    std::string const vsSrc = R".(
    uniform mat4 mvp;
    uniform float aspect = 1.f;
    out vec2 vCoord;

    void main()
    {
        vCoord = vec2(gl_VertexID&1,gl_VertexID>>1);
        gl_Position = mvp*vec4((-1+2*vCoord)*vec2(aspect,1),0,1);
    }
    ).";

    //#################################################################

    std::string fsSrc = R".(
    #extension GL_ARB_gpu_shader_int64 : require
    #extension GL_ARB_bindless_texture : require

    uniform sampler2D lf;
    out vec4 fColor;
    in vec2 vCoord;

    void main()
    {
        fColor = texture(lf, vCoord);
    }
    ).";
    //#################################################################

    std::string csLfSrc = R".(
    #extension GL_ARB_gpu_shader_int64 : require
    #extension GL_ARB_bindless_texture : require

    const ivec2 gridSize = ivec2(GSX);
    const uint lfSize = gridSize.x*gridSize.x;
    const uint focusLevels = 32; //warp size
    uniform uint64_t lfTextures[lfSize];
    layout(bindless_image)uniform layout(r8ui) readonly uimage2D focusMap;
    layout(local_size_x=LSX*LSY)in;
    layout(bindless_image)uniform layout(rgba8) writeonly image2D lf;
    TEXTURE_CHECK_INIT
    
    uniform int showFocusMap;
    uniform int useMedian;
    uniform vec2 viewCoord;
    uniform float focusStep;
    uniform float focus;
    uniform float aspect;
    uniform float gridSampleDistanceR;
    uniform int searchSubdiv;
   
    vec4 lfTexture(int i, vec2 coord)
    {
        TEXTURE_CHECK_EXEC
        return texture(sampler2D(lfTextures[i]),coord);
    }

    void loadPixels(in ivec2 dt, out ivec4 e[3])
    {
        int i = 0;
        ivec2 offset;
        for (offset.y = -1; offset.y <= 1; ++offset.y)
            {
                for (offset.x = -1; offset.x <= 1; ++offset.x)
                {
                    e[i / 4][i % 4] = int(imageLoad(focusMap, dt + offset).x);
                    ++i;
                }
            }
        }

        void minmax(inout int u, inout int v)
        {
            int save = u;
            u = min(save, v);
            v = max(save, v);
        }

        void minmax(inout ivec2 u, inout ivec2 v)
        {
            ivec2 save = u;
            u = min(save, v);
            v = max(save, v);
        }

        void minmax(inout ivec4 u, inout ivec4 v)
        {
            ivec4 save = u;
            u = min(save, v);
            v = max(save, v);
        }

        void minmax3(inout ivec4 e[3])
        {
            minmax(e[0].x, e[0].y);     
            minmax(e[0].x, e[0].z);     
            minmax(e[0].y, e[0].z);     
        }

        void minmax4(inout ivec4 e[3])
        {
            minmax(e[0].xy, e[0].zw);   
            minmax(e[0].xz, e[0].yw);   
        }

        void minmax5(inout ivec4 e[3])
        {
            minmax(e[0].xy, e[0].zw);   
            minmax(e[0].xz, e[0].yw);   
            minmax(e[0].x, e[1].x);     
            minmax(e[0].w, e[1].x);     
        }

        void minmax6(inout ivec4 e[3])
        {
            minmax(e[0].xy, e[0].zw);   
            minmax(e[0].xz, e[0].yw);   
            minmax(e[1].x, e[1].y);     
            minmax(e[0].xw, e[1].xy);   
        }
     
        void main()
        {    
            const ivec2 size = imageSize(lf);
            const ivec2 outCoords = ivec2(gl_GlobalInvocationID.x%size.x, gl_GlobalInvocationID.x/size.x);
            const vec2 inCoords = outCoords/vec2(size);
            int focusLvl = 0;
            if(useMedian != 0)
            { 
                ivec4 e[3]; 
                loadPixels(ivec2(inCoords*imageSize(focusMap)), e);
                minmax6(e);         
                e[0].x = e[2].x;   
                minmax5(e);       
                e[0].x = e[1].w; 
                minmax4(e);      
                e[0].x = e[1].z;
                minmax3(e);   
                focusLvl = e[0].y;
            }
            else
                focusLvl = int(imageLoad(focusMap, ivec2(inCoords*imageSize(focusMap))).x);
            float focusValue = focus+focusLvl*focusStep;
            //float focusValue = focus+dot(textureGather(sampler2D(focusMap), inCoords*imageSize(focusMap), vec4(0.25)))*focusStep;
     
            vec4 color = vec4(0,0,0,0); 
            int n=0;
            for(int x=0; x<gridSize.x; x++)
                for(int y=0; y<gridSize.y; y++)
                {   
                    float dist = distance(viewCoord, vec2(x,y));
                    if(dist > gridSampleDistanceR) continue;

                    n++;
                    int slice = y*int(gridSize.x)+x;
                    vec2 offset = viewCoord-vec2(x,y);
                    offset.y *=-1;
                    vec2 focusedCoords = clamp(inCoords+offset*focusValue*vec2(1.0,aspect), 0.0, 1.0); 
                    color += lfTexture(slice,focusedCoords);

                }
            color /= n;
            color.w = 1.0;
                
            if(showFocusMap != 0)
                color = vec4(vec3(focusLvl/float(focusLevels*(searchSubdiv+1))),1.0);

            //fColor = texture(sampler2D(lfTextures[48]),vCoord);         
            /*if(outCoords.y == 1080-319 && outCoords.x == 894)
                color = vec4(1.0,0.0,0.0,1.0);*/
            imageStore(lf, outCoords, color);
            //imageStore(lf, outCoords, vec4(100,100,100,100));
            
        }
        ).";
        
        //#################################################################

        std::string csMapSrc = R".(  
        #extension GL_ARB_gpu_shader_int64 : require
        #extension GL_NV_shader_thread_shuffle : require
        #extension GL_NV_gpu_shader5 : require
        #extension GL_ARB_bindless_texture : require
        #extension GL_ARB_shader_ballot : require 
        #extension GL_ARB_shader_image_load_store : require

        const ivec2 gridSize = ivec2(GSX);
        const uint lfSize = gridSize.x*gridSize.x;
        const uint focusLevels = 32; //warp size
        const float MAX_M2 = 999999999999999.0;
     
        layout(local_size_x=LSX, local_size_y=LSY)in;
        layout(bindless_image)uniform layout(r8ui) writeonly uimage2D focusMap;
        STATS_INIT
        TEXTURE_CHECK_INIT
        
        uniform uint64_t lfTextures[lfSize];
        uniform vec2 viewCoord;
        uniform float focusStep;
        uniform float focus;
        uniform float aspect;
        uniform int blockRadius;
        uniform float gridSampleDistance;
        uniform float saddleImpact;
        uniform int colMetric;
        uniform int searchSubdiv;
        uniform int sampleBlock;
        uniform int checkSaddle;
        uniform int checkBorders;
        uniform int sampleMode;
        uniform int borderSize;

        vec4 lfTexture(int i, vec2 coord)
        {
            TEXTURE_CHECK_EXEC
            return texture(sampler2D(lfTextures[i]),coord);
        }

        const float PI = 3.141592653589793238462643;
        vec3 lab2lch(vec3 Lab)
        {
            return vec3(
            Lab[0],
            sqrt( ( Lab[1] * Lab[1] ) + ( Lab[2] * Lab[2] ) ),
            atan( Lab[1], Lab[2] ));
        }

        float deltaE2000l( vec3 lch1, vec3 lch2 )
        {
            float avg_L = ( lch1[0] + lch2[0] ) * 0.5;
            float delta_L = lch2[0] - lch1[0];
            float avg_C = ( lch1[1] + lch2[1] ) * 0.5;
            float delta_C = lch1[1] - lch2[1];
            float avg_H = ( lch1[2] + lch2[2] ) * 0.5;
            if( abs( lch1[2] - lch2[2] ) > PI )
                avg_H += PI;
            float delta_H = lch2[2] - lch1[2];
            if( abs( delta_H ) > PI )
            {
                if( lch2[2] <= lch1[2] )
                    delta_H += PI * 2.0;
                else
                    delta_H -= PI * 2.0;
            }

            delta_H = sqrt( lch1[1] * lch2[1] ) * sin( delta_H ) * 2.0;
            float T = 1.0 -
                    0.17 * cos( avg_H - PI / 6.0 ) +
                    0.24 * cos( avg_H * 2.0 ) +
                    0.32 * cos( avg_H * 3.0 + PI / 30.0 ) -
                    0.20 * cos( avg_H * 4.0 - PI * 7.0 / 20.0 );
            float SL = avg_L - 50.0;
            SL *= SL;
            SL = SL * 0.015 / sqrt( SL + 20.0 ) + 1.0;
            float SC = avg_C * 0.045 + 1.0;
            float SH = avg_C * T * 0.015 + 1.0;
            float delta_Theta = avg_H / 25.0 - PI * 11.0 / 180.0;
            delta_Theta = exp( delta_Theta * -delta_Theta ) * ( PI / 6.0 );
            float RT = pow( avg_C, 7.0 );
            RT = sqrt( RT / ( RT + 6103515625.0 ) ) * sin( delta_Theta ) * -2.0; // 6103515625 = 25^7
            delta_L /= SL;
            delta_C /= SC;
            delta_H /= SH;
            return sqrt( delta_L * delta_L + delta_C * delta_C + delta_H * delta_H + RT * delta_C * delta_H );
        }

        float deltaE2000( vec3 lab1, vec3 lab2 )
        {
            vec3 lch1, lch2;
            lch1 = lab2lch( lab1);
            lch2 = lab2lch( lab2);
            return deltaE2000l( lch1, lch2 );
        }

        vec3 rgb2hsv(vec3 c)
        {
            vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
            vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }
            
        vec3 hsvTrans(vec3 hsv)
        {
            float sv = hsv.y*hsv.z;
            float pih = hsv.x*2*PI;
            return vec3(sv*cos(pih), sv*sin(pih), hsv.z);
        }
     
        vec3 rgb2lab(in vec3 rgb){
            float R = rgb.x;
            float G = rgb.y;
            float B = rgb.z;
            // threshold
            float T = 0.008856;

            float X = R * 0.412453 + G * 0.357580 + B * 0.180423;
            float Y = R * 0.212671 + G * 0.715160 + B * 0.072169;
            float Z = R * 0.019334 + G * 0.119193 + B * 0.950227;

            // Normalize for D65 white point
            X = X / 0.950456;
            Y = Y;
            Z = Z / 1.088754;

            bool XT, YT, ZT;
            XT = false; YT=false; ZT=false;
            if(X > T) XT = true;
            if(Y > T) YT = true;
            if(Z > T) ZT = true;

            float Y3 = pow(Y,1.0/3.0);
            float fX, fY, fZ;
            if(XT){ fX = pow(X, 1.0/3.0);} else{ fX = 7.787 * X + 16.0/116.0; }
            if(YT){ fY = Y3; } else{ fY = 7.787 * Y + 16.0/116.0 ; }
            if(ZT){ fZ = pow(Z,1.0/3.0); } else{ fZ = 7.787 * Z + 16.0/116.0; }

            float L; if(YT){ L = (116.0 * Y3) - 16.0; }else { L = 903.3 * Y; }
            float a = 500.0 * ( fX - fY );
            float b = 200.0 * ( fY - fZ );

            return vec3(L,a,b);
        }

        vec3 rgb2yuv(vec3 rgba)
                {
            vec3 yuv = vec3(0.0);

            yuv.x = rgba.r * 0.299 + rgba.g * 0.587 + rgba.b * 0.114;
            yuv.y = rgba.r * -0.169 + rgba.g * -0.331 + rgba.b * 0.5 + 0.5;
            yuv.z = rgba.r * 0.5 + rgba.g * -0.419 + rgba.b * -0.081 + 0.5;
            return yuv;
        }

        float colorDistance(vec3 c1, vec3 c2)
        {
            if(colMetric == 0)
                return distance(c1,c2);
            else if(colMetric == 1)
                return (abs(c1.r-c2.r)+abs(c1.g-c2.g)+abs(c1.b-c2.b))/(c1.r+c2.r+c1.g+c2.g+c1.b+c2.b); //canberra
            else if(colMetric == 2)
                return max(max(abs(c1.r-c2.r), abs(c1.g-c2.g)), abs(c1.b-c2.b)); //chebyshev
            else if(colMetric == 3)
                return pow(pow(abs(c1.r-c2.r),5) + pow(abs(c1.g-c2.g),5) + pow(abs(c1.b-c2.b),5), 1.0/5.0); //minkowski
            else if(colMetric == 4)
                return distance(hsvTrans(rgb2hsv(c1)), hsvTrans(rgb2hsv(c2)));
            else if(colMetric == 5)
                return distance(rgb2yuv(c1), rgb2yuv(c2));
            else if(colMetric == 6)
                return sqrt(3*pow(c1.r-c2.r,2) + 4*pow(c1.g-c2.g,2) + 2*pow(c1.b-c2.b,2)); //weighted euclidan
            else if(colMetric == 7)
                return abs(c1.r-c2.r)+abs(c1.g-c2.g)+abs(c1.b-c2.b); //manhattan
            else if(colMetric == 8)
                return deltaE2000(rgb2lab(c1), rgb2lab(c2));
            else 
                return max(max(3*abs(c1.r-c2.r), 4*abs(c1.g-c2.g)), 2*abs(c1.b-c2.b)); //weighted chebyshev
        }

        void main()
        {
            const uint pixelId = gl_WorkGroupID.x*gl_WorkGroupSize.y + gl_LocalInvocationID.y;
            const ivec2 size = imageSize(focusMap);
            const ivec2 outCoords = ivec2(pixelId%size.x, pixelId/size.x);
            const vec2 inCoords = outCoords/vec2(size);
            const vec2 halfPixelOffset = (vec2(0.5)/textureSize(sampler2D(lfTextures[0]),0))*blockRadius;
            const vec2 pixelOffset = (vec2(1.0)/textureSize(sampler2D(lfTextures[0]),0))*blockRadius;

            float minM2=MAX_M2;
            int subDivLvl = 0;
            for(int i=0; i<=searchSubdiv; i++)
            {
                float m2=0.0;
                vec3 mean = vec3(0);
                int n=0;
                const vec2 focusValue = (focus + (i*focusLevels+gl_LocalInvocationID.x)*focusStep)*vec2(1.0,aspect);
                for(int x=0; x<gridSize.x; x++)
                    for(int y=0; y<gridSize.y; y++)
                    {   
                        float dist = distance(viewCoord, vec2(x,y));
                        if(dist > gridSampleDistance) continue;
     
                        int slice = y*int(gridSize.x)+x;
                        vec2 offset = viewCoord-vec2(x,y);
                        offset.y *=-1;
                        vec2 focusedCoords = clamp(inCoords+offset*focusValue, 0.0, 1.0); 
                        vec4 color = vec4(0);

                        if(sampleBlock == 1)
                        {
                            if(sampleMode == 0)
                            {
                                vec2 offset = halfPixelOffset;
                                if(gl_GlobalInvocationID.x%2 == 0)
                                {
                                    color = lfTexture(slice, focusedCoords+offset); 
                                    color += lfTexture(slice, focusedCoords-offset);
                                }
                                else
                                {
                                    offset *= vec2(-1,1); 
                                    color = lfTexture(slice, focusedCoords+offset);
                                    color += lfTexture(slice, focusedCoords-offset);
                                }
                                color /= 2.0;
                            } 
                            else if(sampleMode == 1)
                            {
                                color = lfTexture(slice,focusedCoords);
                                color += lfTexture(slice,focusedCoords+pixelOffset);
                                color += lfTexture(slice,focusedCoords-pixelOffset);
                                color += lfTexture(slice,focusedCoords+vec2(1, -1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords+vec2(-1, 1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords+vec2(0,1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords-vec2(0,1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords+vec2(1, 0)*pixelOffset);
                                color += lfTexture(slice,focusedCoords-vec2(1, 0)*pixelOffset);
                                color /= 9.0;
                            }
                            else if(sampleMode == 2)
                            {
                                uint modulo = gl_GlobalInvocationID.x%4;
                                color = lfTexture(slice,focusedCoords);
                                if(modulo == 0)
                                {
                                color += lfTexture(slice,focusedCoords+pixelOffset);
                                color += lfTexture(slice,focusedCoords-pixelOffset);
                                }
                                else if(modulo == 1)
                                {
                                color += lfTexture(slice,focusedCoords+vec2(1, -1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords+vec2(-1, 1)*pixelOffset);
                                }
                                else if(modulo == 2)
                                { 
                                color += lfTexture(slice,focusedCoords+vec2(0,1)*pixelOffset);
                                color += lfTexture(slice,focusedCoords-vec2(0,1)*pixelOffset);
                                }
                                else if(modulo == 3)
                                {
                                color += lfTexture(slice,focusedCoords+vec2(1, 0)*pixelOffset);
                                color += lfTexture(slice,focusedCoords-vec2(1, 0)*pixelOffset);
                                }                        
                                color /= 3.0;
                            }
                        }
                        else 
                            color = lfTexture(slice,focusedCoords);
                    
                        n++;
                        vec3 delta = color.xyz-mean;
                        float d = colorDistance(color.xyz, mean);
                        mean += delta/n;
                        m2 += d*colorDistance(color.xyz,mean);
                    } 
                m2 /= n-1;
                if(m2<minM2)
                {
                    minM2 = m2;
                    subDivLvl = i;
                }
            }
               
            if(checkSaddle == 1)
            {
                //normalize 
                float maxM2 = minM2;
                for (uint offset=focusLevels>>1; offset>0; offset>>=1)
                    maxM2 = max(maxM2,shuffleDownNV(maxM2,offset,focusLevels));
                float totalMax2=readFirstInvocationARB(maxM2);
                float originalM2 = minM2;
                minM2 /= totalMax2;

                //TODO fix when using subdivisions
                vec2 neighbours = vec2(minM2);
                if(gl_LocalInvocationID.x != 0)
                    neighbours.x = shuffleUpNV(minM2,1,focusLevels);
                if(gl_LocalInvocationID.x != 31)
                    neighbours.y = shuffleDownNV(minM2,1,focusLevels); 
                float saddle = abs(minM2-neighbours.x) + abs(minM2-neighbours.y);
                bool saddled = false;
                if(minM2 <= neighbours.x && minM2 <= neighbours.y)
                {
                    minM2 -= saddle*saddleImpact;
                    saddled = true;
                }
           
                //OR do it always when id=0? no MIN? 
                if(checkBorders == 1 && anyThreadNV(saddled)) 
                {
                    float MIN=0.01;
                    float DIF=0.006;
                    float difference = 0;
                    if(gl_LocalInvocationID.x == borderSize-1)
                    {
                        for(int i=1; i<borderSize; i++)
                            difference += abs(minM2 - shuffleDownNV(minM2, i, focusLevels));
                        if(difference < DIF && minM2 < MIN)
                            minM2 -= 1;
                    }
                    if(gl_LocalInvocationID.x == focusLevels-borderSize)
                    {
                        float difference = 0;
                        for(int i=1; i<borderSize; i++)
                            difference += abs(minM2 - shuffleUpNV(minM2, i, focusLevels));
                        if(difference < DIF && minM2 < MIN)
                            minM2 -= 1;
                    }   
                }
            }

            float origM2 = minM2;
            for (uint offset=focusLevels>>1; offset>0; offset>>=1)
                minM2 = min(minM2,shuffleDownNV(minM2,offset,focusLevels));
            float totalMinM2=readFirstInvocationARB(minM2);
            if(totalMinM2 == origM2)
                imageStore(focusMap, outCoords, uvec4(subDivLvl*focusLevels+gl_LocalInvocationID.x));
            
            STATS_EXEC
        }
            ).";

        //#################################################################
        std::string csPostSrc = R".(
        #extension GL_ARB_gpu_shader_int64 : require
        #extension GL_ARB_bindless_texture : require
        layout(local_size_x=LSX*LSY)in;
        uniform sampler2D lf;
        layout(bindless_image)uniform layout(rgba8) writeonly image2D postLf;
        layout(bindless_image)uniform layout(r8ui) readonly uimage2D focusMap;

        const int focusLevels = 32;
        uniform float dofDistance;
        uniform float dofRange;
        uniform int searchSubdiv;
        uniform int pass;

        void main()
        { 
            const ivec2 size = textureSize(lf,0);
            const ivec2 coords = ivec2(gl_GlobalInvocationID.x%size.x, gl_GlobalInvocationID.x/size.x);
            const vec2 normalizedCoords = coords/vec2(size);
            vec2 halfPixelOffset = 0.5/size;
            
            int totalLevels = focusLevels*(searchSubdiv+1);
            int focusLevel = int(round(dofDistance*totalLevels));
            int kernelSize = abs(focusLevel - int(imageLoad(focusMap, ivec2(normalizedCoords*imageSize(focusMap))).x));
            kernelSize = int(round(kernelSize*dofRange));
            vec4 color = texture(lf, normalizedCoords)*(kernelSize+1);
            int weights = kernelSize+1;
            for(int i=0; i<kernelSize; i++)
            {
                vec2 pixelOffset = vec2(3*halfPixelOffset + 4*i*halfPixelOffset);
                if(pass==0)
                    pixelOffset.y = 0;
                else
                    pixelOffset.x = 0;
                color += texture(lf, normalizedCoords + pixelOffset)*(kernelSize-i);
                color += texture(lf, normalizedCoords - pixelOffset)*(kernelSize-i);
                weights += 2*(kernelSize-i);
            }
            if(kernelSize>0)
                color /= float(weights);
            color.w = 1.0;
            imageStore(postLf, coords, color);
        }
        ).";    

            std::string statsInitCode{""}; 
            std::string statsExecCode{""}; 
            if constexpr (STATISTICS)
            {
                statsInitCode = 
                R".(layout(std430, binding = 2) buffer statisticsLayout
                    {
                       float statistics[];
                    };).";
                statsExecCode = 
                R".(
                   //if(pixelId==(1080-319)*1920+894)
                   if(outCoords.y==1080-320 && outCoords.x == 894)

                    statistics[gl_LocalInvocationID.x] = origM2;
                ).";
            }
            
            csMapSrc.replace(csMapSrc.find("STATS_INIT"), 10, statsInitCode); 
            csMapSrc.replace(csMapSrc.find("STATS_EXEC"), 10, statsExecCode); 
            
            std::string textureStatsInitCode{""}; 
            std::string textureStatsExecCode{""}; 
            if constexpr (TEXTURE_STATISTICS)
            {
                textureStatsInitCode = 
                R".(layout(std430, binding = 3) buffer textureStatisticsLayout
                    {
                       int textureStatistics[];
                    };).";
                textureStatsExecCode = 
                R".(
                    ivec2 lfSize = textureSize(sampler2D(lfTextures[0]), 0);
                    ivec2 pixelCoord = ivec2(round(lfSize*coord));
                    if(isMap)
                        atomicAdd(textureStatistics[i*lfSize.x*lfSize.y + pixelCoord.y*lfSize.x+pixelCoord.x], 1);
                    else
                        atomicAdd(textureStatistics[gridSize.x*gridSize.y*lfSize.x*lfSize.y+i*lfSize.x*lfSize.y + pixelCoord.y*lfSize.x+pixelCoord.x], 1);
                ).";
            }
            
            csMapSrc.replace(csMapSrc.find("TEXTURE_CHECK_INIT"), 18, textureStatsInitCode); 
            csMapSrc.replace(csMapSrc.find("TEXTURE_CHECK_EXEC"), 18, "bool isMap=true;"+textureStatsExecCode); 
            csLfSrc.replace(csLfSrc.find("TEXTURE_CHECK_INIT"), 18, textureStatsInitCode); 
            csLfSrc.replace(csLfSrc.find("TEXTURE_CHECK_EXEC"), 18, "bool isMap=false;"+textureStatsExecCode); 

        addProgram(vars, "mapProgram", csMapSrc, "");
        addProgram(vars, "lfProgram", csLfSrc, "");
        addProgram(vars, "postProgram", csPostSrc, "");
        addProgram(vars, "drawProgram", vsSrc, fsSrc);
    }

    void savePGM(int *buffer, int offset, glm::ivec2 resolution, std::string path)
    {
        std::ofstream fs(path, std::ios::out | std::ios::binary);
        fs << "P5" << std::endl;
        fs << resolution.x << " " << resolution.y << std::endl;
        fs << 255 << std::endl;
        int size = resolution.x*resolution.y;
        for(int i=0; i<size; i++)
        {
            char number = static_cast<char>(buffer[offset+i]);
            fs.write(&number,1);
        }
        //fs.write(reinterpret_cast<char*> (buffer+offset), size*4);
        fs.close();
    }

    void createView(vars::Vars&vars)
    {
        if(notChanged(vars,"all",__FUNCTION__, {}))return;
        vars.add<basicCamera::OrbitCamera>("view");
    }

    void createProjection(vars::Vars&vars)
    {
        if(notChanged(vars,"all",__FUNCTION__, {"windowSize","camera.fovy","camera.near","camera.far"}))return;

        auto windowSize = vars.get<glm::uvec2>("windowSize");
        auto width = windowSize->x;
        auto height = windowSize->y;
        auto aspect = (float)width/(float)height;
        auto near = vars.getFloat("camera.near");
        auto far  = vars.getFloat("camera.far" );
        auto fovy = vars.getFloat("camera.fovy");

        vars.reCreate<basicCamera::PerspectiveCamera>("projection",fovy,aspect,near,far);
    }

    void createCamera(vars::Vars&vars)
    {
        createProjection(vars);
        createView(vars);
    }

    std::vector<bool> createFramesMask(vars::Vars&vars)
    {
        std::vector<bool> mask(vars.getInt32("lfCount"),true);
        auto gridSize = vars.get<glm::ivec2>("gridSize");
        auto viewCoord = vars.get<glm::vec2>("viewCoord");
        auto gridSampleDistance = vars.getFloat("gridSampleDistance");
        for(int x=0; x<gridSize->x; x++)
            for(int y=0; y<gridSize->y; y++)
            {   
                int slice = y*int(gridSize->x)+x;
                float dist = glm::distance(*viewCoord, glm::vec2(x,y));
                if(dist > gridSampleDistance)
                    mask[slice] = false;
            }
        return mask;
    }

    void recalculateViewCoords(vars::Vars&vars)
    {
        //TODO kdyz se pohne tak nacist vic nebo vsechny
        FUNCTION_PROLOGUE("all","decodingOptimization");//, "view"); 

        auto gridIndex = *vars.get<glm::ivec2>("gridSize")-1;
        float aspect = vars.getFloat("texture.aspect");
        glm::vec3 lfPos(0,0,0);
        auto view = vars.get<basicCamera::OrbitCamera>("view");
        glm::vec3 camPos(view->getView()[3]);
        glm::vec3 centerRay = glm::normalize(lfPos - camPos); 
        float range = .2;
        float coox = (glm::clamp(centerRay.x*-1,-range*aspect,range*aspect)/(range*aspect)+1)/2.;
        float cooy = (glm::clamp(centerRay.y,-range,range)/range+1)/2.;
        glm::vec2 viewCoord = glm::vec2(gridIndex)-glm::vec2(glm::clamp(coox*(gridIndex.x),0.0f,static_cast<float>(gridIndex.x)),glm::clamp(cooy*(gridIndex.y),0.0f,static_cast<float>(gridIndex.y)));
        vars.reCreate<glm::vec2>("viewCoord", viewCoord);

        vars.reCreateVector<bool>("framesMask", vars.getInt32("lfCount"), true);
        if(vars.getBool("decodingOptimization"))
            vars.reCreate<std::vector<bool>>("framesMask", createFramesMask(vars));
    }

    void asyncVideoLoading(vars::Vars &vars)
    {
        SDL_GLContext c = SDL_GL_CreateContext(*vars.get<SDL_Window*>("mainWindow")); 
        SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),c);
        ge::gl::init(SDL_GL_GetProcAddress);
//        ge::gl::setHighDebugMessage();

        auto decoder = std::make_unique<GpuDecoder>(vars.getString("videoFile").c_str());
        auto lfSize = glm::uvec2(*vars.reCreate<unsigned int>("lf.width",decoder->getWidth()), *vars.reCreate<unsigned int>("lf.height", decoder->getHeight()));
        vars.reCreate<float>("texture.aspect",decoder->getAspect());
        //TODO correct framerate etc
        auto length = vars.addUint32("length",static_cast<int>(decoder->getLength()/vars.getInt32("lfCount")));
        
        //Must be here since all textures are created in this context and handles might overlap
        auto focusMapSize = glm::uvec2(vars.addUint32("focusMap.width", lfSize.x/FOCUSMAP_DIV), vars.addUint32("focusMap.height", lfSize.y/FOCUSMAP_DIV));
        auto focusMapTexture = ge::gl::Texture(GL_TEXTURE_2D,GL_R8UI,1,focusMapSize.x,focusMapSize.y);
        auto focusMap = vars.add<GLuint64>("focusMap.image",ge::gl::glGetImageHandleARB(focusMapTexture.getId(), 0, GL_FALSE, 0, GL_R8UI));      
        auto lfTexture = ge::gl::Texture(GL_TEXTURE_2D,GL_RGBA8,1,lfSize.x,lfSize.y);
        vars.add<GLuint64>("lf.image",ge::gl::glGetImageHandleARB(lfTexture.getId(), 0, GL_FALSE, 0, GL_RGBA8)); 
        vars.add<GLuint64>("lf.texture",ge::gl::glGetTextureHandleARB(lfTexture.getId())); 

        auto postLfTexture = ge::gl::Texture(GL_TEXTURE_2D,GL_RGBA8,1,lfSize.x,lfSize.y);
        vars.add<GLuint64>("lf.postTexture",ge::gl::glGetTextureHandleARB(postLfTexture.getId()));
        vars.add<GLuint64>("lf.postImage",ge::gl::glGetImageHandleARB(postLfTexture.getId(), 0, GL_FALSE, 0, GL_RGBA8));

        auto rdyMutex = vars.get<std::mutex>("rdyMutex");
        auto rdyCv = vars.get<std::condition_variable>("rdyCv");
        int frameNum = 0; 
           
        auto timer = vars.addOrGet<Timer<double>>("decoderTimer");
        
        while(vars.getBool("mainRuns"))
        {    
            auto seekFrame = vars.getInt32("seekFrame");
            if(seekFrame > -1)
            {
                decoder->seek(seekFrame*vars.getInt32("lfCount"));
                frameNum = seekFrame;
                vars.getInt32("seekFrame") = -1;
            }

           if(frameNum > length)
                frameNum = 0;
           vars.reCreate<int>("frameNum", frameNum);

           if(!vars.getBool("pause"))
           {
                int index = decoder->getActiveBufferIndex();
                std::string nextTexturesName = "lfTextures" + std::to_string(index);

                if constexpr (MEASURE_TIME)     
                    timer->reset();

                if(vars.getBool("decodingOptimization"))
                    decoder->maskFrames(vars.getVector<bool>("framesMask"));
                else
                    decoder->maskFrames(std::vector<bool>()); 
                
                vars.reCreateVector<GLuint64>(nextTexturesName, decoder->getFrames(vars.getInt32("lfCount")));
                GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
                ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);

                if constexpr (MEASURE_TIME)     
                    vars.addOrGetFloat("elapsed.decoding") = timer->elapsedFromStart();

                vars.getUint32("lfTexturesIndex") = index;

                //TODO without waiting? async draw to increase responsitivity
                frameNum++; 
                //vars.getBool("pause") = true;
           }
            vars.getBool("loaded") = true;
            rdyCv->notify_all();
        }
    }

    void LightFields::init()
    {
        if constexpr (MEASURE_TIME)
            ge::gl::setDebugMessage(nullptr, nullptr);

        auto args = vars.add<argumentViewer::ArgumentViewer>("args",argc,argv);
        auto const videoFile = args->gets("--video","","h265 compressed LF video (pix_fmt yuv420p)");
        auto const gridSize = args->getu32("--grid",8,"number of cameras in row/column (assuming a rectangle)");
        auto const focus = args->getf32("--focus",0,"focus level");
        auto const focusStep = args->getf32("--focusStep",0,"focus search step relative to focus level");
        auto const showHelp = args->isPresent("-h","shows help");
        if (showHelp || !args->validate()) {
            std::cerr << args->toStr();
            exit(0);
        }
        vars.addString("videoFile", videoFile);
        vars.add<glm::ivec2>("gridSize",glm::ivec2(glm::uvec2(gridSize)));
        vars.addInt32("lfCount",glm::pow(gridSize,2)); 

        window->createContext("loading", 450u, sdl2cpp::Window::CORE, sdl2cpp::Window::DEBUG);
        vars.add<SDL_GLContext>("loadingContext", window->getContext("loading"));
        vars.add<SDL_GLContext>("renderingContext", window->getContext("rendering"));
        vars.add<SDL_Window*>("mainWindow", window->getWindow());
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);  
       
        vars.addInt32("seekFrame",-1); 
        vars.addBool("mainRuns", true);
        vars.add<glm::vec2>("viewCoord", glm::vec2(0,0));
        vars.addBool("pause", false);
        vars.addBool("printTimes", false);
        vars.addBool("decodingOptimization", false);
        vars.addUint32("lfTexturesIndex", 0);
        vars.add<ge::gl::VertexArray>("emptyVao");
        vars.addFloat("input.sensitivity",0.01f);
        vars.addFloat("camera.fovy",glm::half_pi<float>());
        vars.addFloat("camera.near",.1f);
        vars.addFloat("camera.far",1000.f);
        vars.addFloat("focus",focus);
        vars.addFloat("inputFocusStep",focusStep);
        vars.addFloat("focusStep",0.0f);
        vars.addFloat("saddleImpact",0.1f);
        vars.addFloat("gridSampleDistance",2.0f);
        vars.addFloat("gridSampleDistanceR",2.0f);
        vars.addBool("showFocusMap", false);
        vars.addBool("useMedian", false);
        vars.addBool("checkSaddle", false);
        vars.addBool("checkBorders", false);
        vars.addBool("sampleBlock", false);
        vars.addBool("dof", false);
        vars.addFloat("dofDistance",0.0f);
        vars.addFloat("dofRange",0.0f);
        vars.addInt32("blockRadius",1);
        vars.addInt32("sampleMode",2);
        vars.addInt32("colMetric", 2);
        vars.addInt32("searchSubdiv", 0);
        vars.addInt32("borderSize", 3);
        vars.add<std::map<SDL_Keycode, bool>>("input.keyDown");
        vars.addUint32("frame",0);
        
        auto rdyMutex = vars.add<std::mutex>("rdyMutex");
        auto rdyCv = vars.add<std::condition_variable>("rdyCv");
        vars.addBool("loaded", false);
        vars.add<std::thread>("loadingThread",asyncVideoLoading, std::ref(vars));
        {
            std::unique_lock<std::mutex> lck(*rdyMutex);
            rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
        }
        ge::gl::glMakeImageHandleResidentARB(*vars.get<GLuint64>("focusMap.image"), GL_READ_WRITE);
        ge::gl::glMakeImageHandleResidentARB(*vars.get<GLuint64>("lf.image"), GL_READ_WRITE);
        ge::gl::glMakeImageHandleResidentARB(*vars.get<GLuint64>("lf.postImage"), GL_READ_WRITE);
        ge::gl::glMakeTextureHandleResidentARB(*vars.get<GLuint64>("lf.texture"));
        ge::gl::glMakeTextureHandleResidentARB(*vars.get<GLuint64>("lf.postTexture"));

        SDL_SetWindowSize(window->getWindow(),vars.getInt32("lf.width"), vars.getInt32("lf.height"));
        SDL_GL_MakeCurrent(*vars.get<SDL_Window*>("mainWindow"),window->getContext("rendering"));
        vars.add<glm::uvec2>("windowSize",window->getWidth(),window->getHeight());

        createProgram(vars);
        createCamera(vars);

        ge::gl::glEnable(GL_DEPTH_TEST);

        if constexpr (STATISTICS)
        { 
            auto stats = vars.add<ge::gl::Buffer>("statistics", 1000*4);
            stats->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
        }
        
        if constexpr (TEXTURE_STATISTICS)
        { 
            auto stats = vars.add<ge::gl::Buffer>("textureStatistics", vars.getInt32("lf.width")*vars.getInt32("lf.height")*gridSize*gridSize*4*2);
            stats->bindBase(GL_SHADER_STORAGE_BUFFER, 3);
        }
    }

    SDL_Surface* flipSurface(SDL_Surface* sfc)
    {
         SDL_Surface* result = SDL_CreateRGBSurface(sfc->flags, sfc->w, sfc->h,
             sfc->format->BytesPerPixel * 8, sfc->format->Rmask, sfc->format->Gmask,
             sfc->format->Bmask, sfc->format->Amask);
         const auto pitch = sfc->pitch;
         const auto pxlength = pitch*(sfc->h - 1);
         auto pixels = static_cast<unsigned char*>(sfc->pixels) + pxlength;
         auto rpixels = static_cast<unsigned char*>(result->pixels) ;
         for(auto line = 0; line < sfc->h; ++line) {
             memcpy(rpixels,pixels,pitch);
             pixels -= pitch;
             rpixels += pitch;
         }
         return result;
    }

    void screenShot(std::string filename, int w, int h)
    {
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        Uint32 rmask = 0xff000000;
        Uint32 gmask = 0x00ff0000;
        Uint32 bmask = 0x0000ff00;
        Uint32 amask = 0x000000ff;  
    #else
        Uint32 rmask = 0x000000ff;
        Uint32 gmask = 0x0000ff00;
        Uint32 bmask = 0x00ff0000;
        Uint32 amask = 0xff000000;
    #endif
        SDL_Surface *ss = SDL_CreateRGBSurface(0, w, h, 24, rmask, gmask, bmask, amask);
        ge::gl::glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, ss->pixels);
        SDL_Surface *s = flipSurface(ss);
        SDL_SaveBMP(s, filename.c_str());
        SDL_FreeSurface(s); 
        SDL_FreeSurface(ss); 
    }

    void LightFields::draw()
    {
        recalculateViewCoords(vars);
        /*for(const auto &u : vars.getVector<bool>("framesMask"))
            std::cerr << ((u) ? "t" : "f");
        std::cerr << std::endl;*/

        auto timer = vars.addOrGet<Timer<double>>("timer");
        timer->reset();
        
        std::string currentTexturesName = "lfTextures" + std::to_string(vars.getUint32("lfTexturesIndex"));
        auto framesMask = vars.getVector<bool>("framesMask");
        std::vector<GLuint64> t = vars.getVector<GLuint64>(currentTexturesName);
        for(int i=0; i<t.size(); i++)
            if(framesMask[i])
                ge::gl::glMakeTextureHandleResidentARB(t[i]);
 
        createCamera(vars);
        auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");

        ge::gl::glClearColor(0.1f,0.1f,0.1f,1.f);
        ge::gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");

        drawGrid(vars);
        
        auto aspect = vars.getFloat("texture.aspect");
        auto viewCoord = vars.get<glm::vec2>("viewCoord");
        vars.get<ge::gl::VertexArray>("emptyVao")->bind();
        
        GLuint perPixelCSSize = (*vars.get<unsigned int>("lf.width") * *vars.get<unsigned int>("lf.height"))/(LOCAL_SIZE_X*LOCAL_SIZE_Y);        

        auto program = vars.get<ge::gl::Program>("mapProgram");
        program
        ->set1f("focus",vars.getFloat("focus"))
        ->set1f("focusStep",vars.getFloat("focusStep"))
        ->set1f("saddleImpact",vars.getFloat("saddleImpact"))
        ->set1i("blockRadius",vars.getInt32("blockRadius"))
        ->set1i("sampleMode",vars.getInt32("sampleMode"))
        ->set1f("gridSampleDistance",vars.getFloat("gridSampleDistance"))
        ->set2fv("viewCoord",value_ptr(*viewCoord))
        ->set1i("colMetric",vars.getInt32("colMetric"))
        ->set1i("searchSubdiv",vars.getInt32("searchSubdiv"))
        ->set1i("checkSaddle",vars.getBool("checkSaddle"))
        ->set1i("checkBorders",vars.getBool("checkBorders"))
        ->set1i("sampleBlock",vars.getBool("sampleBlock"))
        ->set1i("borderSize",vars.getInt32("borderSize"))
        ->set1f("aspect",aspect)
        ->use();
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lfTextures"), vars.getInt32("lfCount"), vars.getVector<GLuint64>(currentTexturesName).data());
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("focusMap"), 1, vars.get<GLuint64>("focusMap.image"));

        if constexpr (STATISTICS)
            vars.get<ge::gl::Buffer>("statistics")->bind(GL_SHADER_STORAGE_BUFFER);
        if constexpr (TEXTURE_STATISTICS)
            vars.get<ge::gl::Buffer>("textureStatistics")->bind(GL_SHADER_STORAGE_BUFFER);

        auto query = ge::gl::AsynchronousQuery(GL_TIME_ELAPSED, GL_QUERY_RESULT, ge::gl::AsynchronousQuery::UINT64);
        if constexpr (MEASURE_TIME) query.begin();

        ge::gl::glDispatchCompute(*vars.get<unsigned int>("focusMap.width") * *vars.get<unsigned int>("focusMap.height")/LOCAL_SIZE_Y,1,1);    
        ge::gl::glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        ge::gl::glFinish();
        
        if constexpr (MEASURE_TIME)
        {
            query.end();
            vars.reCreate<float>("elapsed.map", query.getui64()/1000000.0);
        } 
        
        if constexpr (STATISTICS)
        {
            vars.get<ge::gl::Buffer>("statistics")->unbind(GL_SHADER_STORAGE_BUFFER);
            ge::gl::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            GLfloat *ptr = reinterpret_cast<GLfloat*>(vars.get<ge::gl::Buffer>("statistics")->map());
            for(int i=0; i<32; i++)
                std::cerr << i*vars.getFloat("focusStep")+vars.getFloat("focus") << "\t" << ptr[i] << std::endl;
            vars.get<ge::gl::Buffer>("statistics")->unmap();
        }
        
        auto lfImage = vars.get<GLuint64>("lf.image"); 
        auto postLfImage = vars.get<GLuint64>("lf.postImage"); 
        auto lfTexture = vars.get<GLuint64>("lf.texture"); 
        auto postLfTexture = vars.get<GLuint64>("lf.postTexture"); 

        program = vars.get<ge::gl::Program>("lfProgram");
        program
        ->set1f("focus",vars.getFloat("focus"))
        ->set1f("focusStep",vars.getFloat("focusStep"))
        ->set2fv("viewCoord",glm::value_ptr(*viewCoord))
        ->set1f("aspect",aspect)
        ->set1i("showFocusMap",vars.getBool("showFocusMap"))
        ->set1i("useMedian",vars.getBool("useMedian"))
        ->set1i("searchSubdiv",vars.getInt32("searchSubdiv"))
        ->set1f("gridSampleDistanceR",vars.getFloat("gridSampleDistanceR"))
        ->use();
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lfTextures"), vars.getInt32("lfCount"), vars.getVector<GLuint64>(currentTexturesName).data());
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("focusMap"), 1, vars.get<GLuint64>("focusMap.image"));
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lf"), 1, lfImage);
        
        if constexpr (TEXTURE_STATISTICS)
            vars.get<ge::gl::Buffer>("textureStatistics")->bind(GL_SHADER_STORAGE_BUFFER);

        auto query2 = ge::gl::AsynchronousQuery(GL_TIME_ELAPSED, GL_QUERY_RESULT, ge::gl::AsynchronousQuery::UINT64);
        if constexpr (MEASURE_TIME) query2.begin();

        ge::gl::glDispatchCompute(perPixelCSSize,1,1);    
        ge::gl::glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        ge::gl::glFinish();

        if constexpr (MEASURE_TIME)
        {
            query2.end();
            vars.reCreate<float>("elapsed.lightfield", query2.getui64()/1000000.0);
        } 

        if(vars.getBool("dof"))
        {
            program = vars.get<ge::gl::Program>("postProgram");
            program
            ->set1i("searchSubdiv",vars.getInt32("searchSubdiv"))
            ->set1f("dofDistance",vars.getFloat("dofDistance"))
            ->set1f("dofRange",vars.getFloat("dofRange"))
            ->use();
            ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("focusMap"), 1, vars.get<GLuint64>("focusMap.image"));
            
            auto query3 = ge::gl::AsynchronousQuery(GL_TIME_ELAPSED, GL_QUERY_RESULT, ge::gl::AsynchronousQuery::UINT64);
            if constexpr (MEASURE_TIME) query3.begin();

            for(int i=0; i<2; i++)
            {  
                ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lf"), 1, lfTexture);
                ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("postLf"), 1, postLfImage);
                program->set1i("pass", i);
                ge::gl::glDispatchCompute(perPixelCSSize,1,1);
                ge::gl::glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                ge::gl::glFinish();
                std::swap(lfImage, postLfImage);
                std::swap(lfTexture, postLfTexture);
            }

            if constexpr (MEASURE_TIME)
            {
                query3.end();
                vars.reCreate<float>("elapsed.post", query3.getui64()/1000000.0);
            }
        }

        program = vars.get<ge::gl::Program>("drawProgram");
        program
        ->set1f("aspect",aspect)
        ->setMatrix4fv("mvp",glm::value_ptr(projection->getProjection()*view->getView()));
        ge::gl::glProgramUniformHandleui64vARB(program->getId(), program->getUniformLocation("lf"), 1, lfTexture);
        program->use();

        auto query4 = ge::gl::AsynchronousQuery(GL_TIME_ELAPSED, GL_QUERY_RESULT, ge::gl::AsynchronousQuery::UINT64);
        if constexpr (MEASURE_TIME) query4.begin();

        ge::gl::glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
         
        if constexpr (MEASURE_TIME)
        {
            query4.end();
            vars.reCreate<float>("elapsed.draw", query4.getui64()/1000000.0);
        }

        drawImguiVars(vars);
        ImGui::Begin("Playback");
        if(ImGui::SliderInt("Timeline", vars.get<int>("frameNum"), 1, vars.getUint32("length")))
            vars.getInt32("seekFrame") = vars.getInt32("frameNum");
        ImGui::Selectable("Pause", &vars.getBool("pause"));
        ImGui::DragFloat("Focus", &vars.getFloat("focus"),0.00001f, -10, 10, "%.5f");
        ImGui::DragFloat("Focus step", &vars.getFloat("inputFocusStep"),0.000001f,-10, 10, "%.6f");
        ImGui::DragFloat("Focus sample distance", &vars.getFloat("gridSampleDistance"),0.1f,0,vars.get<glm::ivec2>("gridSize")->x); 
        if(!ImGui::Checkbox("Decoding optimization", &vars.getBool("decodingOptimization"))){
            ImGui::DragFloat("Render sample distance", &vars.getFloat("gridSampleDistanceR"),0.1f,0,vars.get<glm::ivec2>("gridSize")->x);
            vars.updateTicks("decodingOptimization");
        }else
            vars.getFloat("gridSampleDistanceR") = vars.getFloat("gridSampleDistance"); 
        ImGui::Checkbox("Show focus map", &vars.getBool("showFocusMap"));
        ImGui::Checkbox("Use median", &vars.getBool("useMedian"));
        ImGui::Checkbox("Check saddle", &vars.getBool("checkSaddle"));
        if(vars.getBool("checkSaddle"))
        {
            ImGui::DragFloat("Saddle impact", &vars.getFloat("saddleImpact"), 0.01f, 0.0f, 1.0, "%.3f");
            ImGui::Checkbox("Check borders", &vars.getBool("checkBorders"));
            ImGui::DragInt("Border size", &vars.getInt32("borderSize"),1, 1, 10);
        }
        ImGui::Checkbox("Sample block", &vars.getBool("sampleBlock"));
        if(vars.getBool("sampleBlock"))
        {
            ImGui::DragInt("Block radius", &vars.getInt32("blockRadius"),2, 1, 17);
            ImGui::InputInt("Sample mode", &vars.getInt32("sampleMode"));
        }
        ImGui::InputInt("Alternative col metric", &vars.getInt32("colMetric"));
        ImGui::DragInt("Search subdivisions", &vars.getInt32("searchSubdiv"),0,0,7); 
        vars.getFloat("focusStep")=vars.getFloat("inputFocusStep")/(vars.getInt32("searchSubdiv")+1);
        ImGui::Checkbox("Depth of field", &vars.getBool("dof"));
        if(vars.getBool("dof"))
        {
            ImGui::DragFloat("DOF distance", &vars.getFloat("dofDistance"), 0.01f, 0.0f, 1.0, "%.6f");
            ImGui::DragFloat("DOF range", &vars.getFloat("dofRange"), 0.01f, 0.0f, 1.0, "%.6f");

        }
        if (ImGui::Button("Shot"))    
            screenShot("/home/ichlubna/Workspace/lf/data/shot.bmp", window->getWidth(), window->getHeight());
        if constexpr (MEASURE_TIME)
            ImGui::Checkbox("Print times", &vars.getBool("printTimes"));
        if constexpr (TEXTURE_STATISTICS)
        {
            auto glBuffer = vars.get<ge::gl::Buffer>("textureStatistics");
            if (ImGui::Button("Texture stats"))
            {
                ge::gl::glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                auto grid = vars.get<glm::ivec2>("gridSize");
                int gridSize = grid->x*grid->y;
                glm::ivec2 lfSize{vars.getUint32("lf.width"), vars.getUint32("lf.height")};
                int *buffer = reinterpret_cast<int*>(glBuffer->map());
                for(int i=0; i<gridSize*2; i++)
                    savePGM(buffer, i*lfSize.x*lfSize.y, lfSize, "/home/ichlubna/Workspace/lf/data/stats/"+std::to_string(i)+".pgm");  
                glBuffer->unmap();
            }
            GLubyte zero = 0;
            glBuffer->clear(GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &zero); 
        }

    ImGui::End();
    swap();

    GLsync fence = ge::gl::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    ge::gl::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 9999999);
    
    for(int i=0; i<t.size(); i++)
        if(framesMask[i])
            ge::gl::glMakeTextureHandleNonResidentARB(t[i]);

    auto rdyMutex = vars.get<std::mutex>("rdyMutex");
    auto rdyCv = vars.get<std::condition_variable>("rdyCv");
    {
        std::unique_lock<std::mutex> lck(*rdyMutex);
        rdyCv->wait(lck, [this]{return vars.getBool("loaded");});
    }
    vars.getBool("loaded") = false;

    ge::gl::glFinish();
    auto time = vars.get<Timer<double>>("timer")->elapsedFromStart();
    vars.addOrGetFloat("elapsed.total") = time;
    if(time > FRAME_LIMIT)
    { 
        //std::cerr << "Lag" << std::endl;
    }
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long>((FRAME_LIMIT-time)*1000)));

    if(vars.getBool("printTimes")) 
        std::cerr << vars.getFloat("elapsed.map") << "\t" << vars.getFloat("elapsed.lightfield") << std::endl;
}

void LightFields::key(SDL_Event const& event, bool DOWN)
{
    auto keys = vars.get<std::map<SDL_Keycode, bool>>("input.keyDown");
    (*keys)[event.key.keysym.sym] = DOWN;

    if(event.key.keysym.sym == SDLK_ESCAPE)
        mainLoop->stop();
}

void LightFields::mouseMove(SDL_Event const& e)
{
    auto sensitivity = vars.getFloat("input.sensitivity");
    auto orbitCamera =
        vars.getReinterpret<basicCamera::OrbitCamera>("view");
    auto const windowSize     = vars.get<glm::uvec2>("windowSize");
    auto const orbitZoomSpeed = 0.1f;
    auto const xrel           = static_cast<float>(e.motion.xrel);
    auto const yrel           = static_cast<float>(e.motion.yrel);
    auto const mState         = e.motion.state;
    if (mState & SDL_BUTTON_LMASK)
    {
        if (orbitCamera)
        {
            orbitCamera->addXAngle(yrel * sensitivity);
            orbitCamera->addYAngle(xrel * sensitivity);
        }
    }
    if (mState & SDL_BUTTON_RMASK)
    {
        if (orbitCamera) orbitCamera->addDistance(yrel * orbitZoomSpeed);
    }
    if (mState & SDL_BUTTON_MMASK)
    {
        orbitCamera->addXPosition(+orbitCamera->getDistance() * xrel /
                                  float(windowSize->x) * 2.f);
        orbitCamera->addYPosition(-orbitCamera->getDistance() * yrel /
                                  float(windowSize->y) * 2.f);
        vars.updateTicks("view");
    }
    
}

void LightFields::resize(uint32_t x,uint32_t y)
{
    auto windowSize = vars.get<glm::uvec2>("windowSize");
    windowSize->x = x;
    windowSize->y = y;
    vars.updateTicks("windowSize");
    ge::gl::glViewport(0,0,x,y);
}


int main(int argc,char*argv[])
{
    LightFields app{argc, argv};
    try
    {
    app.start();
    }
    catch(sdl2cpp::ex::Exception &e)
    {
    std::cerr << e.what();
   }
    app.vars.get<std::thread>("loadingThread")->join();
    return EXIT_SUCCESS;
}
