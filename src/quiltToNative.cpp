#include <ArgumentViewer/ArgumentViewer.h>
#include <FreeImagePlus.h>
#include <glm/glm.hpp>

bool checkInputName(std::string const&n){
  if(n == "")return false;
  if(n.rfind(".") == std::string::npos)return false;
  return true;
}

bool checkOutputName(std::string const&n){
  if(n == "")return true;
  if(n.rfind(".") == std::string::npos)return false;
  return true;
}

std::string removePath(std::string const&n){
  auto const p = n.rfind("/");
  if(p == std::string::npos)return n;
  return n.substr(p+1);
}

std::string baseName(std::string const&n){
  return n.substr(0,n.rfind("."));
}

std::string fileExtension(std::string const&n){
  return n.substr(n.rfind("."));
}

std::string fixOutputFileName(std::string const&out,std::string const&in,std::string const&suffix){
  if(out != "")return out;

  auto const nameExt = removePath(in);
  auto const base    = baseName(nameExt);
  auto const ext     = fileExtension(nameExt);

  return base+suffix+ext;
}

struct Params{
  uint32_t columns    ;
  uint32_t rows       ;
  uint32_t width      ;
  uint32_t height     ;
  float    tilt       ;
  float    pitch      ;
  float    center     ;
  float    viewPortion;
  bool     progress   ;
};

void transform(fipImage &output,fipImage const&input,Params const&params){
  auto texelFetch = [&](int32_t x,int32_t y,uint32_t c){
    RGBQUAD v;
    if(x>=input.getWidth ())x = input.getWidth ()-1;
    if(y>=input.getHeight())y = input.getHeight()-1;
    if(x< 0            )x = 0                  ;
    if(y< 0            )y = 0                  ;
    input.getPixelColor(x,y,&v);
    if(c==0)return v.rgbRed  /255.f;
    if(c==1)return v.rgbGreen/255.f;
            return v.rgbBlue /255.f;
  };

  auto texture = [&](glm::vec2 const&cc,uint32_t c){
    auto xx = cc.x*input.getWidth();
    auto yy = cc.y*input.getHeight();
    auto xt = glm::fract(xx);
    auto yt = glm::fract(yy);
    auto xi = glm::floor(xx);
    auto yi = glm::floor(yy);
    auto m = texelFetch(xi,yi  ,c)*(1-xt) + texelFetch(xi+1,yi  ,c)*xt;
    auto n = texelFetch(xi,yi+1,c)*(1-xt) + texelFetch(xi+1,yi+1,c)*xt;
    return m*(1-yt) + n*yt;
  };

  auto const texArr = [&](glm::vec3 const&uvz){
    auto const nofImages     = params.rows*params.columns;
    auto const selectedImage = (uint32_t)glm::floor(uvz.z * nofImages);
    auto const col           = selectedImage%params.columns;
    auto const row           = selectedImage/params.columns;
    float x = (float)(col + uvz.x) / (float)params.columns;
    float y = (float)(row + uvz.y) / params.rows;
    return glm::vec2(x, y) * params.viewPortion;
  };

  float subp = 1.f/(3*params.width);

  for(uint32_t y=0;y<params.height;++y){
    if(params.progress)
      std::cerr << y << "/" << params.height << std::endl;
    for(uint32_t x=0;x<params.width;++x){
      float c[3];
      auto const xx = (float)x/params.width;
      auto const yy = (float)y/params.height;

      for(uint32_t i=0;i<3;++i){
        float z = (xx + i*subp + yy*params.tilt)*params.pitch - params.center;
        z = 1.f - glm::fract(z);
        z = glm::abs(z);
        c[i] = texture(texArr(glm::vec3(xx,yy,z)),i);
      }
      RGBQUAD col;
      col.rgbRed   = c[0]*255;
      col.rgbGreen = c[1]*255;
      col.rgbBlue  = c[2]*255;
      output.setPixelColor(x,y,&col);
    }
  }
}

int main(int argc,char*argv[]){
  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);
  Params params;

  auto suffix         = args->gets  ("--suffix"      ,"_h"      ,"suffix of output file if the output file is not specified"                                            );
  auto inputFileName  = args->gets  ("--input"       ,""        ,"input quilt image"                                                                                    );
  auto outputFileName = args->gets  ("--output"      ,""        ,"output quilt image. if --input is: \"/a/b/name.ext\" then the default output is \"name${SUFFIX}.ext\"");
  params.columns      = args->getu32("--columns"     ,5         ,"number of quilt columns"                                                                              );
  params.rows         = args->getu32("--rows"        ,9         ,"number of quilt rows"                                                                                 );
  params.width        = args->getu32("--width"       ,2560      ,"width of holographics display in pixels"                                                              );
  params.height       = args->getu32("--height"      ,1600      ,"height of holographic display in pixels"                                                              );
  params.tilt         = args->getf32("--tilt"        ,-0.1153f  ,"tilt of the holographics display"                                                                     );
  params.pitch        = args->getf32("--pitch"       ,354.42108f,"pitch of the holographics display"                                                                    );
  params.center       = args->getf32("--center"      ,0.04239f  ,"center of the holographics display"                                                                   );
  params.viewPortion  = args->getf32("--viewPortion" ,0.99976f  ,"viewPortion of the holographics display"                                                              );
  params.progress     = args->isPresent("--progress","print progress");
  auto printHelp      = args->isPresent("--help","print help");

  bool validated;
  try{
    validated = args->validate();
  }catch(std::exception&e){
    std::cerr << e.what() << std::endl;
  }


  if(!validated){
    std::cerr << "ERROR: argument validation failed!" << std::endl;
    printHelp |= true;
  }

  if(!checkInputName(inputFileName)){
    std::cerr << "ERROR: wrong input file!" << std::endl;
    printHelp |= true;
  }

  if(!checkOutputName(outputFileName)){
    std::cerr << "ERROR: wrong output file!" << std::endl;
    printHelp |= true;
  }

  if(printHelp){
    std::cerr << args->toStr() << std::endl;
    return 0;
  }
  
  outputFileName = fixOutputFileName(outputFileName,inputFileName,suffix);

  fipImage inputImg;
  inputImg.load(inputFileName.c_str());

  auto outputImg = fipImage(FIT_BITMAP,params.width,params.height,24);

  transform(outputImg,inputImg,params);

  outputImg.save(outputFileName.c_str());

  return 0;
}
