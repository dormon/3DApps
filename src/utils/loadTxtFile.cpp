#include <fstream>

#include <utils/loadTxtFile.hpp>

std::string loadTxtFile(std::string const&fileName){
  auto file = std::ifstream(fileName);
  if(!file.is_open())throw std::runtime_error("loadTxtFile - cannot open "+fileName);
  std::string str((std::istreambuf_iterator<char>(file)),
                 std::istreambuf_iterator<char>());
  return str;
}

