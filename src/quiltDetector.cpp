#include <iostream>
#include <vector>
#include <math.h>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <ArgumentViewer/ArgumentViewer.h>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc.hpp>

class Analyzer{
    private:
    class MetaFrame{
        public:
        int number{0};
        cv::Point2f direction{0,0};
        float bluriness{0};
        
        friend std::ostream& operator<<(std::ostream& os, const MetaFrame& mf)
        {
            os << mf.number << " " << mf.direction.x << " " << mf.direction.y << " " << mf.bluriness << "  ";
            return os;
        }
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

    using sequencesVector = std::vector<std::vector<MetaFrame>>;
    using quiltsVector = std::vector<std::vector<int>>;

    sequencesVector sequences;
    quiltsVector quilts;
    quiltsVector trimmedQuilts;
    Buffer buffer;
    int frameCounter{0};
    float yBounds{0.001};
    float xLimit{0.001};
    float blurThreshold{100.0};
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
        avgDirection.x /= buffer.current().gray.cols;
        avgDirection.y /= buffer.current().gray.rows;
        //TODO additional detection of scene change?
        if((abs(avgDirection.x) < xLimit) || (abs(avgDirection.y) > yBounds) || ((avgDirection.x < 0) != (previousDirection.x < 0)))
        {
            if(sequences.back().size() >= QUILT_SIZE)
                sequences.emplace_back();
            else
                sequences.back().clear();
        }
        else
        {
            cv::Mat laplacian;
            cv::Laplacian(buffer.current().gray, laplacian, CV_64F);
            cv::Scalar mean, dev;
            cv::meanStdDev(laplacian, mean, dev);
            sequences.back().push_back({frameCounter, avgDirection, static_cast<float>(dev[0])});
        }
    
    frameCounter++;
    }   

    public:
    Analyzer()
    {
        sequences.emplace_back();
        quilts.emplace_back();
    }

    friend bool operator>>(cv::VideoCapture& capture, Analyzer& analyzer)
    {
        bool notEmpty = capture >> analyzer.buffer;
        if(notEmpty && !analyzer.buffer.previous().color.empty())
            analyzer.processCurrent();
        return notEmpty; 
    }   

    template <class T>
    void serializeFrameVector(T *frameVector, std::string outputFile)
    { 
        std::ofstream ofs(outputFile, std::ofstream::trunc);
        for(const auto& frames : *frameVector)
        {
            for(const auto& frame : frames)
            {
                ofs << frame << " ";
            }
            ofs << std::endl;
        } 
    }

    void loadSequences(std::string inputFile)
    {
        sequences.clear();
        std::ifstream ifs(inputFile);
        std::string line;
        std::stringstream linestream(line);
        while(std::getline(ifs, line))
        {
            sequences.emplace_back();
            MetaFrame val;
            while(linestream >> val.number >> val.direction.x >> val.direction.y >> val.bluriness)
                sequences.back().push_back(val);
        }
    }

    const quiltsVector& createQuilts(std::string outputFolder)
    {
        //TODO blur error
        serializeFrameVector(&sequences, outputFolder+"sequences.txt");
        for(const auto& sequence : sequences)
        {
            float maxOffset{0};
            if(!quilts.back().empty())
                quilts.emplace_back();
            for(const auto& frame : sequence)
            {
                float absDir{abs(frame.direction.x)}; 
                if(absDir > maxOffset)
                    maxOffset = absDir;
            }
            float offsetAcc{0};
            for(int i=0; i<sequence.size(); i++)
            {
                offsetAcc += abs(sequence[i].direction.x);
                if(offsetAcc >= maxOffset)
                {
                    if(i == 0)
                        quilts.back().push_back(sequence[i].number);
                    //else if(fmod(offsetAcc,maxOffset) < fmod(offsetAcc+abs(sequence[i+1].direction.x),maxOffset))
                    else if(offsetAcc-maxOffset < abs(offsetAcc-maxOffset-abs(sequence[i-1].direction.x)))
                        quilts.back().push_back(sequence[i].number);
                    else
                        quilts.back().push_back(sequence[i-1].number);
                    offsetAcc -= maxOffset;
                }
            }
        }
        serializeFrameVector(&quilts, outputFolder+"quilts.txt");

        //TODO limit for extra long sequences
        for(const auto& quilt : quilts)
            if(quilt.size() >= QUILT_SIZE)
            {
                int stride = quilt.size()/QUILT_SIZE;
                int offset = ((quilt.size()%QUILT_SIZE))/2;
                trimmedQuilts.emplace_back();
                for(int i=offset; i<quilt.size()-offset; i+=stride)
                    trimmedQuilts.back().push_back(quilt[i]);
            
                while(trimmedQuilts.back().size() > QUILT_SIZE)
                    trimmedQuilts.back().pop_back();
            }
 
        serializeFrameVector(&trimmedQuilts, outputFolder+"trimmedQuilts.txt");
        return trimmedQuilts;
    } 
};

void process(int argc, char **argv)
{
    auto args = argumentViewer::ArgumentViewer(argc,argv);
    auto inputFile =  args.gets("-i","","input video file");  
    auto outputFolder =  args.gets("-o","","output folder");
    auto inputSequences =  args.gets("-s","","input sequences file");
    bool exportFrames = args.isPresent("-e","export quilts as image files");
    if(inputFile.empty() || outputFolder.empty())
        throw "Invalid output or input path \n"+args.toStr();

    if (outputFolder.back() != '/')
        outputFolder.push_back('/');
    std::filesystem::remove_all(outputFolder); 
    std::filesystem::create_directory(outputFolder);

    cv::VideoCapture capture(inputFile);
    Analyzer analyzer;
    if(inputSequences.empty())
        while(capture >> analyzer);
    else
        analyzer.loadSequences(inputSequences);
    auto quilts = analyzer.createQuilts(outputFolder);

    if(exportFrames)
    {
        for(int i=0; i<quilts.size(); i++)
            if(!quilts[i].empty())
                std::filesystem::create_directory(outputFolder+std::to_string(i));
        int frameNum{0};
        capture.set(cv::CAP_PROP_POS_AVI_RATIO, 0);
        while(true)
        {
            cv::Mat frame;
            capture >> frame;
            if(frame.empty()) break;
            for(int i=0; i<quilts.size(); i++)
                if(std::binary_search(quilts[i].begin(), quilts[i].end(), frameNum))
                    cv::imwrite(outputFolder+std::to_string(i)+"/"+std::to_string(frameNum)+".png", frame); 
            frameNum++;
        }
    }
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
