#include <iostream>
#include <vector>
#include <ArgumentViewer/ArgumentViewer.h>
#include <opencv4/opencv2/opencv.hpp>

class Frame{
    public:
    int number;
    float angle;
    float magnitude;
};

class Buffer{
    private:
    static constexpr int BUFFER_SIZE{2};
    cv::Mat frames[BUFFER_SIZE];
    int active{0};
    public:
    cv::Mat& current() {return frames[active];}
    cv::Mat& previous() {return frames[(active+1)%BUFFER_SIZE];}

    friend bool operator>>(cv::VideoCapture& capture, Buffer& buffer)
    {
        buffer.active = (buffer.active+1)%buffer.BUFFER_SIZE;
        buffer.frames[buffer.active].release();
        capture >> buffer.frames[buffer.active];
        return !buffer.current().empty();
    }
};

class Analyzer{
    private:
    std::vector<std::vector<Frame>> sequences;
    Buffer buffer;
    int frameCounter{0};
    void processCurrent()
    {

    }    

    public:
    friend bool operator>>(cv::VideoCapture& capture, Analyzer& analyzer)
    {
        bool notEmpty = capture >> analyzer.buffer;
        if(notEmpty)
            analyzer.processCurrent();
        return notEmpty; 
    }
};

void process(int argc, char **argv)
{
    auto args = argumentViewer::ArgumentViewer(argc,argv);
    auto inputFile =  args.gets("-i","","input video file");  
    auto outputFolder =  args.gets("-o","","output folder");
    if(inputFile.empty() || outputFolder.empty())
        throw "Invalid output or input path \n"+args.toStr();

    if (outputFolder.back() != '/')
        outputFolder.push_back('/'); 

    cv::VideoCapture capture(inputFile);

    Analyzer analyzer;
    capture >> analyzer;
    int frameNumber{0};
    while(true)
        if(!(capture >> analyzer)) break;
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
