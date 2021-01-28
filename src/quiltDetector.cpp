#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/features2d.hpp>
#include <string>
#include <vector>
#include <math.h>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <ArgumentViewer/ArgumentViewer.h>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc.hpp>

class Analyzer{
    private:
    class Accumulator
    {
        private:
        float value{0};
        int counter{0};
        public:
        float avg() {return (counter != 0) ? value/counter : 0;}
        Accumulator operator+=(float v)
        {
            value += v;
            counter++;
            return *this;
        }
    };
    class MetaFrame{
        public:
        int number{0};
        cv::Point2f direction{0,0};
        cv::Point2f magnitude{0};
        float bluriness{0};
        float distortion{0};
        
        friend std::ostream& operator<<(std::ostream& os, const MetaFrame& mf)
        {
            os << mf.number << " " << mf.direction.x << " " << mf.direction.y << " " << mf.magnitude.x << " " << mf.magnitude.y << " " << mf.bluriness << "  "<< mf.distortion;
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
   
    using SequencesVector = std::vector<std::vector<int>>;
    std::vector<MetaFrame> frames;
    SequencesVector sequences;
    SequencesVector quilts;
    SequencesVector trimmedQuilts;
    Buffer buffer;
    int frameCounter{0};
    float differencesExclude{0.3};
    float distWinSize{0.3};
    float yBounds{0.1};
    float xLimit{0.000001};
    float blurThreshold{100.0};
    cv::Point2f previousDirection{0,0};
    static constexpr int QUILT_SIZE{45};
    enum AnalysisMethod {SPARSE_FLOW, DENSE_FLOW, FEATURE_MATCHING}; 

    float analyzeBluriness(cv::Mat &frame)
    { 
        cv::Mat laplacian;
        cv::Laplacian(frame, laplacian, CV_64F);
        cv::Scalar mean, dev;
        cv::meanStdDev(laplacian, mean, dev);
        return dev[0];
    }
    float analyzeDistortion()
    {
        cv::Point2i size{buffer.current().gray.cols, buffer.current().gray.rows};
        cv::Point2i winSize{size*distWinSize};
        cv::Rect roi(0, 0, winSize.x, winSize.y);
        float yDirs[5];
        for(int i=-1; i<4; i++)
        {
            if(i<0)
            {
                roi.x = size.x/2 - winSize.x/2;
                roi.y = size.y/2 - winSize.y/2;
            }
            else
            { 
                roi.x = (i%2)*(size.x-winSize.x);
                roi.y = (i/2)*(size.y-winSize.y);
            }
            cv::Mat croppedPrevious = buffer.previous().gray(roi);
            cv::Mat croppedCurrent = buffer.current().gray(roi);
            cv::Mat flow(croppedCurrent.size(), CV_32FC2);
            cv::calcOpticalFlowFarneback(croppedPrevious, croppedCurrent, flow, 0.5, 3, 15, 3, 5, 1.2, 0);
            cv::medianBlur(flow,flow,3);
            yDirs[i+1] = cv::mean(flow).val[1];
        }
        Accumulator acc;
        for(int i=1; i<5; i++)
            acc += abs(yDirs[i]);
        return acc.avg()-abs(yDirs[0]);
    }

    MetaFrame analyzeMatches(std::vector<cv::Point2f> &p0, std::vector<cv::Point2f> &p1)
    {
        std::vector<float> xd, yd;
        if(p1.empty())
            for(const auto &p : p0)
            {
                xd.push_back(p.x);
                yd.push_back(p.y);
            }
        else 
            for(int i=0; i < p1.size(); i++)
            {
                cv::Point2f d = p1[i]-p0[i];
                xd.push_back(d.x);
                yd.push_back(d.y);
            }
        std::sort(xd.begin(), xd.end());
        std::sort(yd.begin(), yd.end());
        MetaFrame mf;
        int offset = xd.size()*differencesExclude;
        int halfOffset = offset/2;
        Accumulator corners[4];
        cv::Point2i size{buffer.current().gray.cols,buffer.current().gray.rows};
        for(int i=halfOffset; i<xd.size()-halfOffset; i++) 
        {
           mf.direction += {xd[i], yd[i]}; 
           mf.magnitude += {abs(xd[i]), abs(yd[i])};
        }

        mf.direction /= static_cast<float>(p0.size()-offset);
        mf.magnitude /= static_cast<float>(p0.size()-offset);
        return mf;
    }

   MetaFrame sparseFlowOffset()
    { 
        class SparseFlow{
            public: 
            std::vector<uchar> status;
            std::vector<float> err;
            std::vector<cv::Point2f> p0, p1;
        };

        SparseFlow flow;
        //might be used once per N frames, switchig the points at the end
        flow.p0.clear(); flow.p1.clear(); flow.err.clear(); flow.status.clear();
        cv::goodFeaturesToTrack(buffer.previous().gray, flow.p0, 100, 0.3, 7, cv::Mat(), 7, false, 0.04);
        auto criteria = cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, 10, 0.03);
        cv::calcOpticalFlowPyrLK(buffer.previous().gray, buffer.current().gray, flow.p0, flow.p1, flow.status, flow.err, cv::Size(15,15), 2, criteria);
        std::vector<cv::Point2f> pp0, pp1;
        for(int i=0; i<flow.p0.size(); i++)
            if(flow.status[i])
            {
                pp1.push_back(flow.p1[i]);
                pp0.push_back(flow.p0[i]);
            }
        return analyzeMatches(pp0, pp1);        
    }
    
    MetaFrame denseFlowOffset()
    {
        cv::Mat flow(buffer.current().gray.size(), CV_32FC2);
        cv::calcOpticalFlowFarneback(buffer.previous().gray, buffer.current().gray ,flow, 0.5, 3, 15, 3, 5, 1.2, 0); 
        std::vector<cv::Point2f> d, dummy;
        for(int y=0; y<flow.rows; y++)
            for(int x=0; x<flow.cols; x++)
                d.push_back({flow.at<cv::Vec2f>(y,x)[0], flow.at<cv::Vec2f>(y,x)[1]});
        return analyzeMatches(d, dummy);
    }

    MetaFrame featureMatchingOffset()
    {
        auto detector = cv::ORB::create();
        std::vector<cv::KeyPoint> p0, p1; 
        std::vector<cv::Point2f> pp0, pp1; 
        cv::Mat d0, d1;
        detector->detectAndCompute(buffer.previous().gray, cv::noArray(),p0,d0);
        detector->detectAndCompute(buffer.current().gray, cv::noArray(),p1,d1);
        if(p0.empty() || p1.empty())
            return {};
        auto matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);
        std::vector<std::vector<cv::DMatch>> matches;
        matcher->knnMatch(d0,d1,matches,2);

        for(auto const &m : matches)
            if(m[0].distance < 0.7*m[1].distance) 
            {    
                pp0.push_back(p0[m[0].queryIdx].pt);
                pp1.push_back(p1[m[0].trainIdx].pt);
            }
        return analyzeMatches(pp0,pp1);        
    }
    
    void processCurrent(AnalysisMethod method=FEATURE_MATCHING)
    {
            MetaFrame mf;;
            switch (method)
            {
                case SPARSE_FLOW:
                mf = sparseFlowOffset();
                break;
                
                case DENSE_FLOW:
                mf = denseFlowOffset();
                break;
                
                case FEATURE_MATCHING:
                mf = featureMatchingOffset();
                break;
            } 
            mf.bluriness = analyzeBluriness(buffer.current().gray);
            if(frameCounter % 4 == 0)
                mf.distortion = analyzeDistortion();
            mf.number = frameCounter;
            frames.push_back(mf);
            //TODO additional detection of scene change?
            frameCounter++; 
    }   

    public:
    Analyzer()
    {
    }

    friend bool operator>>(cv::VideoCapture& capture, Analyzer& analyzer)
    {
        bool notEmpty = capture >> analyzer.buffer;
        if(notEmpty && !analyzer.buffer.previous().color.empty())
            analyzer.processCurrent();
        return notEmpty; 
    }   

    void serializeFrameVector(SequencesVector *frameVector, std::string outputFile)
    { 
        std::ofstream ofs(outputFile, std::ofstream::trunc);
        for(const auto& frameIndices : *frameVector)
        {
            for(const auto& index : frameIndices)
            {
                ofs << index << " ";
            }
            ofs << std::endl;
        } 
    }
    void serializeFrameVector(std::vector<MetaFrame> *frameVector, std::string outputFile)
    { 
        std::ofstream ofs(outputFile, std::ofstream::trunc);
        for(const auto& frame : *frameVector)
        {
            ofs << frame << " ";
            ofs << std::endl;
        } 
    }

    void loadFrames(std::string inputFile)
    {
        std::ifstream ifs(inputFile);
        std::string line;
        std::stringstream linestream(line);
        while(std::getline(ifs, line))
        {
            MetaFrame val;
            while(linestream >> val.number >> val.direction.x >> val.direction.y >> val.magnitude.x >> val.magnitude.y >> val.bluriness >> val.distortion)
                frames.push_back(val);
        }
    }

    void createSequences()
    {
        sequences.emplace_back();
        for(int i=1; i<frames.size(); i++)
            if((abs(frames[i].direction.x) < xLimit) || (abs(frames[i].direction.y) > yBounds))
            //if((abs(frames[i].direction.x) < xLimit) || (abs(frames[i].direction.y) > yBounds) || ((frames[i].direction.x < 0) != (frames[i-1].direction.x < 0)))
            {
                if(sequences.back().size() >= QUILT_SIZE)
                    sequences.emplace_back();
                else
                    sequences.back().clear();
            }
            else
            {
                sequences.back().push_back(i);
            }
    } 

    const SequencesVector& createQuilts(std::string outputFolder)
    {
        //TODO blur error
        createSequences();
        serializeFrameVector(&frames, outputFolder+"frames.txt");
        serializeFrameVector(&sequences, outputFolder+"sequences.txt");
        quilts.emplace_back();

        for(const auto& sequence : sequences)
        {
            float maxOffset{0};
            if(!quilts.back().empty())
                quilts.emplace_back();
            for(const auto& frame : sequence)
            {
                float absDir{abs(frames[frame].magnitude.x)}; 
                if(absDir > maxOffset)
                    maxOffset = absDir;
            }
            float offsetAcc{0};
            for(int i=0; i<sequence.size(); i++)
            {
                offsetAcc += abs(frames[sequence[i]].magnitude.x);
                if(offsetAcc >= maxOffset)
                {
                    if(i == 0)
                        quilts.back().push_back(frames[sequence[i]].number);
                    //else if(fmod(offsetAcc,maxOffset) < fmod(offsetAcc+abs(sequence[i+1].direction.x),maxOffset))
                    else if(offsetAcc-maxOffset < abs(offsetAcc-maxOffset-abs(frames[sequence[i-1]].direction.x)))
                        quilts.back().push_back(frames[sequence[i]].number);
                    else
                        quilts.back().push_back(frames[sequence[i-1]].number);
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

    void printResults()
    {
        for(const auto& sequence : sequences)
            if(!sequence.empty())
            {
                float bluriness{0};
                for(const auto& frame : sequence)
                    bluriness += frames[frame].bluriness;
                bluriness /= sequence.size();

                std::cerr << std::endl << sequence.front() << "-" << sequence.back() << " sharp: " << bluriness; 
            }
    }
};

void process(int argc, char **argv)
{
    auto args = argumentViewer::ArgumentViewer(argc,argv);
    auto inputFile =  args.gets("-i","","input video file");  
    auto outputFolder =  args.gets("-o","","output folder");
    auto inputFrames =  args.gets("-s","","input pre-analyzed file");
    bool exportFrames = args.isPresent("-e","export quilts as image files");
    auto const showHelp = args.isPresent("-h","shows help");
    if (showHelp || !args.validate()) {
        std::cerr << args.toStr();
        exit(0);
    }
    if(inputFile.empty() || outputFolder.empty())
        throw "Invalid output or input path \n"+args.toStr();

    if (outputFolder.back() != '/')
        outputFolder.push_back('/');
    std::filesystem::remove_all(outputFolder); 
    std::filesystem::create_directory(outputFolder);

    cv::VideoCapture capture(inputFile);
    Analyzer analyzer;
    if(inputFrames.empty())
        while(capture >> analyzer);
    else
        analyzer.loadFrames(inputFrames);
    auto quilts = analyzer.createQuilts(outputFolder);
    analyzer.printResults();

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
