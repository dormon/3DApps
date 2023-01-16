#include<holoCalibration.h>

int main(int argc,char*argv[]){
  std::cerr << HoloCalibration::getCalibration() << std::endl;
  return 0;
}
