#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include "System.h"
#include "Converter.h"

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
    //cv::FileStorage fs(outFileName, cv::FileStorage::WRITE);
    std::ofstream ofs(outFileName, std::ofstream::trunc);
    while(1)
    {
        cv::Mat im, frame;
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
        capture >> frame;
        if(frame.empty()) break;
        cv::cvtColor(frame, im, cv::COLOR_BGR2GRAY);
        clahe->apply(im,im);

        //fs << std::string("mat") + std::to_string(i) << SLAM.TrackMonocular(im,i);
        cv::Mat mat = SLAM.TrackMonocular(im,i);
        if(mat.cols > 0)
        {
            cv::Rect rect(0,0,3,3);
            cv::Mat rot = mat(rect);
            auto rotation = ORB_SLAM3::Converter::toQuaternion(rot);
            ofs << i << " " << mat.at<float>(0,3) << " " << mat.at<float>(1,3) << " " << mat.at<float>(2,3) << " " << rotation[0] << " " << rotation[1] << " " << rotation[2] << " " << rotation[3] << std::endl;
        }
        i++;
    }

    SLAM.Shutdown();
    return 0;
}
