#include <iostream>
#include <vector>
#include <math.h>
#include <ArgumentViewer/ArgumentViewer.h>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc.hpp>

class Analyzer{
    private:
    class MetaFrame{
        public:
        int number;
        cv::Point2f direction;
    };

    class Frame{
        public:
        cv::Mat color;
        cv::Mat gray;
        ~Frame()
        {
            color.release();
            gray.release();
        }
    };

    class Buffer{
        private:
        static constexpr int BUFFER_SIZE{2};
        Frame frames[BUFFER_SIZE];
        int active{0};
        public:
        Frame& current() {return frames[active];}
        Frame& previous() {return frames[(active+1)%BUFFER_SIZE];}

        friend bool operator>>(cv::VideoCapture& capture, Buffer& buffer)
        {    
            buffer.previous().~Frame();
            buffer.active = (buffer.active+1)%buffer.BUFFER_SIZE;
            capture >> buffer.current().color;
            if(buffer.current().color.empty())
                return false;
            cv::cvtColor(buffer.current().color, buffer.current().gray, cv::COLOR_BGR2GRAY);
            return true;
        }
    };
    
    class SparseFlow{
        public: 
        std::vector<uchar> status;
        std::vector<float> err;
        std::vector<cv::Point2f> p0, p1;
    };

    std::vector<std::vector<MetaFrame>> sequences;
    Buffer buffer;
    int frameCounter{0};
    float yBounds{1.1};
    float xLimit{1.5};
    cv::Point2f previousDirection{0,0};
    static constexpr int QUILT_SIZE{45};

    void processCurrent()
    {
        SparseFlow flow;
        //might be used once per N frames, switchig the points at the end
        flow.p0.clear(); flow.p1.clear(); flow.err.clear(); flow.status.clear();
        cv::goodFeaturesToTrack(buffer.previous().gray, flow.p0, 100, 0.3, 7, cv::Mat(), 7, false, 0.04);
        auto criteria = cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, 10, 0.03);
        cv::calcOpticalFlowPyrLK(buffer.previous().gray, buffer.current().gray, flow.p0, flow.p1, flow.status, flow.err, cv::Size(15,15), 2, criteria);
        cv::Point2f avgDirection;
        int goodPoints{0};
        for(int i=0; i<flow.p0.size(); i++)
            if(flow.status[i])
            {
                avgDirection += flow.p1[i]-flow.p0[i];
                goodPoints++;
            }
        avgDirection /= goodPoints;
        if((abs(avgDirection.x) < xLimit) || (abs(avgDirection.y) > yBounds) || ((avgDirection.x < 0) != (previousDirection.x < 0)))
        {
            if(sequences.front().size() >= QUILT_SIZE)
                sequences.emplace_back();
            else
                sequences.front().clear();
        }
        else
            sequences.front().push_back({frameCounter, avgDirection});
        frameCounter++;
    }   

    public:
    Analyzer()
    {
        sequences.emplace_back();        
    }

    friend bool operator>>(cv::VideoCapture& capture, Analyzer& analyzer)
    {
        bool notEmpty = capture >> analyzer.buffer;
        if(notEmpty && !analyzer.buffer.previous().color.empty())
            analyzer.processCurrent();
        return notEmpty; 
    }
    
    void exportSequences()
    { 
        for(const auto& sequence : sequences)
            if(!sequence.empty())
            {
                std::cerr << sequence.front().number << " " << sequence.back().number << std::endl;
            }
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
    while(capture >> analyzer);
    analyzer.exportSequences();
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
