#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cmath>
#include <ArgumentViewer/ArgumentViewer.h>

std::pair<int, int> getGridSize(std::string path)
{
    std::vector<std::filesystem::path> files;
    for (const auto & file : std::filesystem::directory_iterator(path))
        if(file.path().extension() == ".ppm")
            files.push_back(file.path().filename());
    std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs)
             {return lhs.string() < rhs.string();});

    auto lastName = files.back().string();
    auto delimiter = lastName.find("_");
    return {std::stoi(lastName.substr(0,delimiter)), std::stoi(lastName.substr(delimiter+1))};
}

using HeaderInfo = struct{int maxValue; int width; int height;};
HeaderInfo readHeader(std::ifstream &ifs)
{
    HeaderInfo info;
    std::string line;
    std::getline(ifs, line);
    if (line != "P6") throw("Input ppm not in binary format!");
    do std::getline(ifs, line);
    while (line[0] == '#');
    std::stringstream imgSize(line);    
    try
    {
        imgSize >> info.width;
        imgSize >> info.height;
    }
    catch (...)
    {
        throw("Input ppm error: no resolution");
    }
        
    do std::getline(ifs, line);
    while (line[0] == '#');
    std::stringstream maxValSS(line); 
    try
    {
        maxValSS >> info.maxValue;
    }
    catch (...)
    {
        throw("Input ppm error: no max value");
    }
    return info;
}

void process(int argc, char **argv)
{
    auto args = argumentViewer::ArgumentViewer(argc,argv);
    auto inputFolder =  args.gets("-i","","input folder with images in format x_y.ppm starting with 0_0");  
    auto outputFolder =  args.gets("-o","","output folder");
    auto const interlace = args.getu32("-n",1,"interlace factor");
    if(inputFolder.empty() || outputFolder.empty())
        throw args.toStr();

    if (inputFolder.back() != '/')
        inputFolder.push_back('/');  
    if (outputFolder.back() != '/')
        outputFolder.push_back('/');  
  
    auto gridSize = getGridSize(inputFolder); 
    
    std::vector<std::vector<std::ifstream>> ifstreams;
    HeaderInfo imageInfo;
    for(int x=0; x<gridSize.first; x++)
    {
        ifstreams.push_back({});
        for(int y=0; y<gridSize.second; y++)
        { 
            ifstreams[x].push_back(std::ifstream(inputFolder+std::to_string(x)+"_"+std::to_string(y)+".ppm", std::ios::in | std::ios::binary));
            imageInfo = readHeader(ifstreams[x][y]);
        }}

    int bitdepth = std::log2(imageInfo.maxValue+1);
    std::cout << "Grid size: " << gridSize.first << " " << gridSize.second << std::endl;
    std::cout << "Resolution: " << imageInfo.width << " " << imageInfo.height << std::endl;
    std::cout << "Bitdepth: " << bitdepth << std::endl;
    if(bitdepth != 8)
        throw("8-bit only support");

    std::ofstream outputFile;
    int i=0;
    std::vector<char> buffer(3,0);
    for(int x=0; x<imageInfo.width; x++)
        for(int y=0; y<imageInfo.height; y++)
        {
            if(i % interlace == 0)
            {
                outputFile.close();
                outputFile.open(outputFolder+std::to_string(x)+"_"+std::to_string(y)+".ppm");
                outputFile << "P6" << std::endl;
                outputFile << gridSize.first*interlace << " " << gridSize.second*interlace << std::endl;
                outputFile << std::pow(2, bitdepth)-1 << std::endl;
            }
            for (auto &ifss : ifstreams)
                for(auto &ifs : ifss)
                {
                   //TODO interlace to read more from one stream to have same cam pixels grouped
                   ifs.read(buffer.data(), buffer.size()); 
                   outputFile.write(buffer.data(), buffer.size());;
                }
        }
}

void reconstruct(int x, int y, int interlace)
{

}

int main (int argc, char **argv)
{
    try{
        process(argc, argv);    
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
