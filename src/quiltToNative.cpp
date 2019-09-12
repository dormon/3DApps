#include <ArgumentViewer/ArgumentViewer.h>
#include <FreeImagePlus.h>

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

int main(int argc,char*argv[]){
  auto args = std::make_shared<argumentViewer::ArgumentViewer>(argc,argv);

  auto suffix         = args->gets  ("--suffix"      ,"_h"      ,"suffix of output file if the output file is not specified"                                            );
  auto inputFileName  = args->gets  ("--input"       ,""        ,"input quilt image"                                                                                    );
  auto outputFileName = args->gets  ("--output"      ,""        ,"output quilt image. if --input is: \"/a/b/name.ext\" then the default output is \"name${SUFFIX}.ext\"");
  auto columns        = args->getu32("--columns"     ,5         ,"number of quilt columns"                                                                              );
  auto rows           = args->getu32("--rows"        ,9         ,"number of quilt rows"                                                                                 );
  auto width          = args->getu32("--width"       ,2560      ,"width of holographics display in pixels"                                                              );
  auto height         = args->getu32("--height"      ,1600      ,"height of holographic display in pixels"                                                              );
  auto tilt           = args->getf32("--tilt"        ,-0.1153f  ,"tilt of the holographics display"                                                                     );
  auto pitch          = args->getf32("--pitch"       ,354.42108f,"pitch of the holographics display"                                                                    );
  auto center         = args->getf32("--center"      ,0.04239f  ,"center of the holographics display"                                                                   );
  auto viewPortion    = args->getf32("--viewPortion" ,0.99976f  ,"viewPortion of the holographics display"                                                              );
  auto printHelp      = args->isPresent("--help","print help");

  auto const validated = args->validate();


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
  std::cerr << outputFileName << std::endl;

  fipImage inputImg;
  inputImg.load(inputFileName.c_str());


  auto outputImg = fipImage(FIT_BITMAP,width,height,24);
  outputImg.save(outputFileName.c_str());

  return 0;
}
