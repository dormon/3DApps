#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include "System.h"

int main(int argc, char **argv)
{
    std::string inFileName = argv[3];
    std::string outFileName = argv[4];
    cv::VideoCapture capture(inFileName);
    if(!capture.isOpened())
    {
        std::cerr << "Cannot open file" + inFileName << std::endl;
        return -1;
    }
    //argv1 vocabulary, argv2 camera settings
    ORB_SLAM3::System SLAM(argv[1],argv[2],ORB_SLAM3::System::MONOCULAR,false);
    int i=0;
    cv::FileStorage fs(outFileName, cv::FileStorage::WRITE);
    while(1)
    {
        cv::Mat im, frame;
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
        capture >> frame;
        if(frame.empty()) break;
        cv::cvtColor(frame, im, cv::COLOR_BGR2GRAY);
        clahe->apply(im,im);

        std::vector<float> translation;
        std::vector<float> rotation; 

        fs << std::string("mat") + std::to_string(i) << SLAM.TrackMonocular(im,i);
        i++;
    }

    SLAM.Shutdown();
    return 0;
}
