#include <utils/ls.hpp>

#include <filesystem>
namespace fs = std::filesystem;

std::vector<std::string> ls(std::string const&dir){
  std::vector<std::string>files;
  files.clear();
  for (const auto & entry : fs::directory_iterator(dir))
    files.push_back(entry.path());
  return files;
}
