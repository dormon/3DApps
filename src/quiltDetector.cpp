#include <glm/fwd.hpp>
#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/features2d.hpp>
#include <string>
#include <vector>
#include <math.h>
#include <fstream>
#include <numeric>
#include <filesystem>
#include <algorithm>
#include <ArgumentViewer/ArgumentViewer.h>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudafeatures2d.hpp>
#include <glm/gtc/quaternion.hpp>


#include <glm/gtx/string_cast.hpp> 
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
        void clear() {value=0; counter=0;}
    };
    class MetaFrame{
        public:
        int number{0};
        cv::Point2f direction{0,0};
        cv::Point2f magnitude{0};
        float bluriness{0};
        float distortion{0};
        bool panPattern{0};       
 
        friend std::ostream& operator<<(std::ostream& os, const MetaFrame& mf)
        {
            os << mf.number << " " << mf.direction.x << " " << mf.direction.y << " " << mf.magnitude.x << " " << mf.magnitude.y << " " << mf.bluriness << "  " << mf.distortion << " " << mf.panPattern;
            return os;
        }
    };

    class SlamMetaFrame{
        public:
        int number{0};
        cv::Point3f positionDelta;
        glm::quat orientation;
        float quatDelta{0};
        float bluriness{0};

        friend SlamMetaFrame operator-(SlamMetaFrame& a, SlamMetaFrame& b)
        {
            SlamMetaFrame result;
            result.number = b.number;
            result.positionDelta = b.positionDelta - a.positionDelta;
            auto q1 = glm::normalize(a.orientation);
            auto q2 = glm::normalize(b.orientation);
            float d = glm::clamp(glm::dot(q1, q2),0.0f,1.0f);
            result.quatDelta = 1.0-glm::abs((glm::acos(d)/glm::half_pi<float>())-1.0);
            return result;
        }
    };

    class Frame{
        public:
        cv::Mat color;
        cv::Mat gray;
        cv::cuda::GpuMat gpuGray;
        ~Frame()
        {
            color.release();
            gray.release();
            gpuGray.release();
        }
    };

    class Buffer{
        private:
        static constexpr int BUFFER_SIZE{2};
        Frame frames[BUFFER_SIZE];
        int active{0};
        bool gpuFrames{false};
        public:
        Frame& current() {return frames[active];}
        Frame& previous() {return frames[(active+1)%BUFFER_SIZE];}
        Buffer(bool g) : gpuFrames{g} 
        {}
        friend bool operator>>(cv::VideoCapture& capture, Buffer& buffer)
        {    
            buffer.previous().~Frame();
            buffer.active = (buffer.active+1)%buffer.BUFFER_SIZE;
            capture >> buffer.current().color;
            if(buffer.current().color.empty())
                return false;
            cv::cvtColor(buffer.current().color, buffer.current().gray, cv::COLOR_BGR2GRAY);
            if(buffer.gpuFrames)
                buffer.current().gpuGray = cv::cuda::GpuMat(buffer.current().gray);
            return true;
        }
    };

    class DistortionStats{
        public:
        float amount{0};
        bool panPattern{false};
    };

    class ScoreCounter{
        private:
        Accumulator distortionAmount;
        Accumulator panPattern;
        Accumulator bluriness;
        Accumulator shaking;
        static constexpr float maxDistortion{2.5};
        static constexpr float maxBluriness{100};
        const float shakingLimit;
        public:
        ScoreCounter(float yb=1.5) : shakingLimit{(yb > 0) ? yb : 1.0f}
        {}
        friend void operator<<(ScoreCounter& counter, MetaFrame& frame)
        {   if(frame.number % DISTORTION_INTERVAL == 0)
            {
                //std::cerr << frame.distortion << " " << frame.panPattern << std::endl;
                counter.distortionAmount += frame.distortion;
                counter.panPattern += (frame.panPattern) ? 1.0 : 0.0;
            }
            counter.bluriness += frame.bluriness;
            counter.shaking += frame.magnitude.y;
        }
        friend void operator<<(ScoreCounter& counter, SlamMetaFrame& frame)
        {   
            counter.panPattern += frame.quatDelta;
            counter.bluriness += frame.bluriness;
            counter.shaking += frame.positionDelta.y+frame.positionDelta.z;
        }
        float getScore()
        {
            return (distortionAmount.avg()/maxDistortion)*0.2 + panPattern.avg()*0.5 + (bluriness.avg()/maxBluriness)*0.2 + (shaking.avg()/shakingLimit)*0.1;
        }
        float getSlamScore()
        {
            return (10*panPattern.avg()*0.7 + (bluriness.avg()/maxBluriness)*0.2 + (shaking.avg()/shakingLimit)*0.1);
        }
        void clear() {
            distortionAmount.clear();
            panPattern.clear();
            bluriness.clear();
            shaking.clear();
        }
    };
 
    using SequencesVector = std::vector<std::vector<int>>;
    std::vector<MetaFrame> frames;
    std::vector<SlamMetaFrame> slamFrames;
    SequencesVector sequences;
    SequencesVector quilts;
    SequencesVector trimmedQuilts;
    std::vector<float> sequencesScores;
    Buffer buffer;
    int frameCounter{0};
    float differencesExclude{0.3};
    float distWinSize{0.3};
    float xLimit{0.00001};
    float yBounds{1.5};
    bool gpuCompute{false};
    bool slamInput{false};
    cv::Point2f previousDirection{0,0};
    static constexpr int flowWinSize{15};
    static constexpr int QUILT_SIZE{10};
    enum AnalysisMethod {SPARSE_FLOW, DENSE_FLOW, FEATURE_MATCHING};
    AnalysisMethod method{FEATURE_MATCHING};
    cv::Mat denseFlowCache;

    float analyzeBluriness(cv::Mat &frame)
    { 
        cv::Mat laplacian;
        cv::Laplacian(frame, laplacian, CV_64F);
        cv::Scalar mean, dev;
        cv::meanStdDev(laplacian, mean, dev);
        return dev[0];
    }

    DistortionStats analyzeDistortion()
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

            cv::Mat flow;
            if(method == DENSE_FLOW)
                flow = denseFlowCache(roi);
            else
            {
                if(gpuCompute)
                { 
                    cv::cuda::GpuMat gpuCroppedPrevious = buffer.previous().gpuGray(roi);
                    cv::cuda::GpuMat gpuCroppedCurrent = buffer.current().gpuGray(roi);
                    cv::cuda::GpuMat gpuFlow;
                    //cv::cuda::GpuMat gpuFlow;//{cv::Size{static_cast<int>(1920*distWinSize),static_cast<int>(1080*distWinSize)}, CV_32FC2};
                    cv::Ptr<cv::cuda::FarnebackOpticalFlow> dense = cv::cuda::FarnebackOpticalFlow::create(3, 0.5, false, 15, 3, 5, 1.2, 0); 
                    dense->calc(gpuCroppedPrevious, gpuCroppedCurrent, gpuFlow);
                    gpuFlow.download(flow);
                }
                else
                {
                    cv::Mat croppedPrevious = buffer.previous().gray(roi);
                    cv::Mat croppedCurrent = buffer.current().gray(roi);
                    cv::calcOpticalFlowFarneback(croppedPrevious, croppedCurrent, flow, 0.5, 3, 15, 3, 5, 1.2, 0);
                }
            }
            cv::medianBlur(flow,flow,3);
            yDirs[i+1] = cv::mean(flow).val[1];
        }
        Accumulator acc;
        for(int i=1; i<5; i++)
            acc += abs(yDirs[i]);
        bool isPan{false};
        float pairs[2]{yDirs[1]*yDirs[4], yDirs[2]*yDirs[3]};
        float pairsAdd[2]{yDirs[1]+yDirs[4], yDirs[2]+yDirs[3]};
        constexpr float EPS{0.0001};
        if( ((pairs[0] > 0 || abs(pairs[0]) < EPS) && (pairs[1] > 0 || abs(pairs[1]) < EPS)) && (pairsAdd[0]*pairsAdd[1] < 0))
            isPan = true;
        return {acc.avg()-abs(yDirs[0]), isPan};
    }

    MetaFrame analyzeMatches(cv::Mat flow)
    { 
        MetaFrame mf;
        for(int y=0; y<flow.rows; y++)
            for(int x=0; x<flow.cols; x++)
            {
               mf.direction += {flow.at<cv::Vec2f>(y,x)[0], flow.at<cv::Vec2f>(y,x)[1]};
               mf.magnitude += {abs(flow.at<cv::Vec2f>(y,x)[0]), abs(flow.at<cv::Vec2f>(y,x)[1])};
            }
        float total = flow.rows*flow.cols;
        mf.direction /= total;
        mf.magnitude /= total;
        return mf;
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

        if(gpuCompute)
        {
            class SparseFlowGpu{
                public: 
                cv::cuda::GpuMat p0, p1;
                cv::cuda::GpuMat status;
            };
     
            SparseFlowGpu gpuFlow; 
            cv::Ptr<cv::cuda::CornersDetector> detector = cv::cuda::createGoodFeaturesToTrackDetector(buffer.previous().gpuGray.type(), 500, 0.04, 7);
            detector->detect(buffer.previous().gpuGray, gpuFlow.p0);
            cv::Ptr<cv::cuda::SparsePyrLKOpticalFlow> sparse = cv::cuda::SparsePyrLKOpticalFlow::create(cv::Size(flowWinSize, flowWinSize), 2, 10);
            sparse->calc(buffer.previous().gpuGray, buffer.current().gpuGray, gpuFlow.p0, gpuFlow.p1, gpuFlow.status);
            
            gpuFlow.p0.download(flow.p0);
            gpuFlow.p1.download(flow.p1);
            gpuFlow.status.download(flow.status);
        }
        else
        {
            //might be used once per N frames, switchig the points at the end
            flow.p0.clear(); flow.p1.clear(); flow.err.clear(); flow.status.clear();
            cv::goodFeaturesToTrack(buffer.previous().gray, flow.p0, 500, 0.3, 7, cv::Mat(), 7, false, 0.04);
            auto criteria = cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, 10, 0.03);
            cv::calcOpticalFlowPyrLK(buffer.previous().gray, buffer.current().gray, flow.p0, flow.p1, flow.status, flow.err, cv::Size(flowWinSize, flowWinSize), 2, criteria);
        }

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
        if(gpuCompute)
        {
            cv::cuda::GpuMat gpuFlow(buffer.current().gray.size(), CV_32FC2);
            cv::Ptr<cv::cuda::FarnebackOpticalFlow> dense = cv::cuda::FarnebackOpticalFlow::create(3, 0.5, false, 15, 3, 5, 1.2, 0); 
            dense->calc(buffer.previous().gpuGray, buffer.current().gpuGray, gpuFlow);
            gpuFlow.download(flow);
        }
        else
        {
            cv::calcOpticalFlowFarneback(buffer.previous().gray, buffer.current().gray ,flow, 0.5, 3, 15, 3, 5, 1.2, 0); 
        }
        flow.copyTo(denseFlowCache);
        return analyzeMatches(flow);
    }

    MetaFrame featureMatchingOffset()
    {
        std::vector<cv::KeyPoint> p0, p1; 
        std::vector<cv::Point2f> pp0, pp1;
        std::vector<std::vector<cv::DMatch>> matches;
        
        if(gpuCompute)
        {
            auto detector = cv::cuda::ORB::create();
            cv::cuda::GpuMat gd0, gd1;
            detector->detectAndCompute(buffer.previous().gpuGray, cv::noArray(), p0, gd0);
            detector->detectAndCompute(buffer.current().gpuGray, cv::noArray(), p1, gd1);
            if(p0.empty() || p1.empty())
                return {};
            auto matcher = cv::cuda::DescriptorMatcher::createBFMatcher(cv::NORM_HAMMING);
            matcher->knnMatch(gd0, gd1, matches, 2);
        }
        else
        { 
            auto detector = cv::ORB::create();
            cv::Mat d0, d1;
            detector->detectAndCompute(buffer.previous().gray, cv::noArray(),p0,d0);
            detector->detectAndCompute(buffer.current().gray, cv::noArray(),p1,d1);
            if(p0.empty() || p1.empty())
                return {};
            auto matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);
            matcher->knnMatch(d0,d1,matches,2);
        }

        for(auto const &m : matches)
            if(m[0].distance < 0.7*m[1].distance) 
            {    
                pp0.push_back(p0[m[0].queryIdx].pt);
                pp1.push_back(p1[m[0].trainIdx].pt);
            }
        return analyzeMatches(pp0,pp1);        
    }
    
    void processCurrent()
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
            if(frameCounter % DISTORTION_INTERVAL == 0)
            {
                auto stats = analyzeDistortion();
                mf.distortion = stats.amount;
                mf.panPattern = stats.panPattern;
            }
            mf.number = frameCounter;
            frames.push_back(mf);
            //TODO additional detection of scene change?
            frameCounter++; 
    }   

    public:
    constexpr static int DISTORTION_INTERVAL{5};

    Analyzer(unsigned int m, unsigned int gpu, float yb=1.5) : yBounds{yb}, gpuCompute{static_cast<bool>(gpu)}, method{static_cast<AnalysisMethod>(m)}, buffer{Buffer(gpu)}
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
    template <class T>
    void serializeFrameVector(std::vector<T> *frameVector, std::string outputFile)
    { 
        std::ofstream ofs(outputFile, std::ofstream::trunc);
        for(const auto& frame : *frameVector)
        {
            ofs << frame << " ";
            ofs << std::endl;
        } 
    }

    void loadFrames(std::string inputFile, bool slamFile=false)
    {
        std::ifstream ifs(inputFile);
        std::string line;
        slamInput = slamFile || slamInput;
        SlamMetaFrame previous;
        bool first{true};
        while(std::getline(ifs, line))
        {  
            std::stringstream linestream(line);
            if(slamInput)
            {
                SlamMetaFrame current;
                while(linestream >> current.number >> current.positionDelta.x >> current.positionDelta.y >> current.positionDelta.z >> current.orientation.x >> current.orientation.y >> current.orientation.z >> current.orientation.w); 
                if(!first)
                {
                    slamFrames.push_back(current-previous);
                    //std::cerr << slamFrames.back().orientationDelta.y << " " << current.orientationDelta.y << " " << previous.orientationDelta.y << std::endl;
                }
                first=false;
                previous = current;
            }
            else
            {
                frames.emplace_back();
                while(linestream >> frames.back().number >> frames.back().direction.x >> frames.back().direction.y >> frames.back().magnitude.x >> frames.back().magnitude.y >> frames.back().bluriness >> frames.back().distortion >> frames.back().panPattern);
            }
        }
 
        //TODO remove, just one 
        for(auto &frame : slamFrames)
            frame.bluriness = frames[frame.number].bluriness;
    }

    void createSequences(bool directionTest)
    {
        ScoreCounter score(yBounds);
        sequences.emplace_back();
        sequencesScores.emplace_back();
        int size = (slamInput) ? slamFrames.size() : frames.size(); 
        float prevDir = (slamInput) ? slamFrames[0].positionDelta.x : frames[0].direction.x;
        for(int i=1; i<frames.size(); i++)
        {

            //TODO refactor to one vector
            float horizDir{0};
            float shaking{0};
            if(slamInput)
            {
                horizDir = slamFrames[i].positionDelta.x;
                shaking = abs(slamFrames[i].positionDelta.y)+abs(slamFrames[i].positionDelta.z);
            }
            else
            {
                horizDir = frames[i].direction.x;
                shaking = abs(frames[i].direction.y); 
            }
    
            if((abs(horizDir) < xLimit) || (shaking > yBounds) || ( directionTest && ((horizDir < 0) != (prevDir < 0))))
            {
                if(sequences.back().size() >= QUILT_SIZE)
                {
                    sequences.emplace_back();
                    sequencesScores.emplace_back();
                }
                else
                {
                    sequences.back().clear();
                    score.clear();
                }
            }
            else
            {
                sequences.back().push_back(i);
                if(slamInput)
                {    
                    score << slamFrames[i];
                    sequencesScores.back() = score.getSlamScore();
                }
                else
                {
                    score << frames[i];
                    sequencesScores.back() = score.getScore();
                }
            }
            prevDir = horizDir;
        }
    } 

    const SequencesVector& createQuilts(std::string outputFolder, std::string prefix, bool directionTest=false)
    {
        createSequences(directionTest);
        serializeFrameVector(&frames, outputFolder+prefix+"_frames.txt");
        serializeFrameVector(&sequences, outputFolder+prefix+"_sequences.txt");
        serializeFrameVector(&sequencesScores, outputFolder+prefix+"_sequenceScores.txt");
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
        serializeFrameVector(&quilts, outputFolder+prefix+"_quilts.txt");
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
 
        serializeFrameVector(&trimmedQuilts, outputFolder+prefix+"_trimmedQuilts.txt");
        return trimmedQuilts;
    } 

    class Error{
        public: 
        float offset{0};
        float error{0};
        bool duplicates{false};
        std::vector<int> window;
    };

    void printWindow(Error error)
    {
        std::cerr << "Offset: " << error.offset << "  Error:" << error.error << " " << ((error.duplicates) ? "Has duplicates" : "Is unique") << std::endl;
        for(auto const &i : error.window)
            std::cerr << i << " ";
    }    

    void checkLinearWindow(float frameDistance, float startPosition, float range, int sequenceIndex)
    {
        //positions and smallest gap
        std::vector<float> positions;
        float acc{0};
        for(const auto &frame : sequences[sequenceIndex])
        {
            positions.push_back(acc);
            acc += frames[frame].magnitude.x;
        }                
       
        constexpr int STEP_RESOLUTION{1000};
        std::vector<Error> errors;
        errors.reserve(STEP_RESOLUTION+1);
        const float STEP_SIZE{(range*2)/STEP_RESOLUTION};
        for(float offset = startPosition-range; offset < startPosition+range; offset+=STEP_SIZE)
        {
            Error error;
            error.offset = offset;
            for(int i=0; i<QUILT_SIZE; i++)
            {  
                float sample = offset+i*frameDistance; 
                error.window.emplace_back();
                //nearest neigbour
                float lastDistance{99999999999999999999.0f};
                for(int j=0; j<positions.size(); j++)
                {
                    float distance = glm::abs(sample-positions[j]);
                    if(distance < lastDistance)
                    {
                        lastDistance = distance;
                        error.window.back() = j;
                    }
                }
                error.error += lastDistance*lastDistance;
            }
            error.duplicates = std::adjacent_find(error.window.begin(), error.window.end()) != error.window.end();
            errors.push_back(error);
        }
      
        std::cerr << std::endl << "Distance: " << frameDistance << std::endl;
        std::cerr << "Start: " << startPosition  << std::endl;
        std::cerr << "Range: " << range << std::endl;
        
        std::cerr << std::endl << "Positions:" << std::endl; 
        for(auto const &i : positions)
            std::cerr << i << " ";
        std::cerr << std::endl;

        std::cerr << std::endl << "Middle window:" << std::endl;
        Error bestError = errors[STEP_RESOLUTION/2];
        printWindow(bestError);
        
        std::cerr << std::endl << "Random window:" << std::endl;
        bestError = errors[rand()%STEP_RESOLUTION];
        printWindow(bestError);

        std::cerr << std::endl << "Best window:" << std::endl;
        for(auto const &error : errors)
            if(error.error < bestError.error)
                bestError = error;
        printWindow(bestError);
        std::cerr << std::endl;
        
    }

    void printResults()
    {
        Accumulator a;
        int size{QUILT_SIZE};
        float s = 0;
        std::cout << std::endl << "Sequence start - sequence end : score";
        for(int i=0; i<sequences.size(); i++)
            if(sequences[i].size() > size)
            {
             size = sequences[i].size();
             s = sequencesScores[i];
             //a+=sequencesScores[i];
             std::cout << std::endl << sequences[i].front() << "-" << sequences[i].back() <<  " : " << sequencesScores[i];
            }
        //if( s< 0){ s=0.001;}
        //std::cout << s << "," << size << std::endl;
       /*     if(!sequences[i].empty())
                std::cout << std::endl << sequences[i].front() << "-" << sequences[i].back() <<  " : " << sequencesScores[i];
        //std::cout << std::endl << "^^^^^^^^^^^^^" <<  std::endl << "start_frame-end_frame : score" << std::endl;*/
    }
};

void process(int argc, char **argv)
{
    auto args = argumentViewer::ArgumentViewer(argc,argv);
    auto inputFile =  args.gets("-i","","input video file");  
    auto outputFolder =  args.gets("-o","","output folder");
    auto yBounds = args.getf32("-y", 1.5,"allowed amount of vertical movement");
    auto inputFrames =  args.gets("-f","","input pre-analyzed frames file");
    auto inputSlamFrames =  args.gets("-s","","input pre-analyzed ORB_SLAM frames file");
    bool exportFrames = args.isPresent("-e","export quilts as image files");
    auto method = args.getu32("-m", 0, "method in the first phase of detection (0-Lucas-Kanade, 1-Farneback, 2-ORB)");
    auto directionTest = args.isPresent("-d","strict test for the flow direction");
    auto gpu = args.getu32("-g", 0, "0-cpu flow, 1-gpu flow computations");
    auto const showHelp = args.isPresent("-h","shows help");
    if (showHelp || !args.validate()) {
        std::cerr << args.toStr();
        exit(0);
    }
    if( (inputFrames.empty() && inputFile.empty()) || outputFolder.empty())
        throw "Invalid output or input path \n"+args.toStr();

    if (outputFolder.back() != '/')
        outputFolder.push_back('/');
    if(!std::filesystem::exists(outputFolder))
        std::filesystem::create_directory(outputFolder);
    Analyzer analyzer(method, gpu, yBounds);
    if(inputFrames.empty() && inputSlamFrames.empty())
    {
       cv::VideoCapture capture(inputFile);
       while(capture >> analyzer);
    }
    else if(inputSlamFrames.empty())
        analyzer.loadFrames(inputFrames);
    else
    {
        //TODO remove
        analyzer.loadFrames(inputFrames);
        analyzer.loadFrames(inputSlamFrames, true);
    }
    auto quilts = analyzer.createQuilts(outputFolder, std::filesystem::path(inputFile).stem(), directionTest);
    analyzer.printResults();

    //testing the window check
    //analyzer.checkLinearWindow(20.0, 55.5, 5.0, 0);

    if(exportFrames && !inputFile.empty())
    {
        cv::VideoCapture capture(inputFile);
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
