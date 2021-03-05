#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include "System.h"

cv::Mat rot2euler(const cv::Mat & rotationMatrix)
{
    cv::Mat euler(3,1,CV_64F);

    double m00 = rotationMatrix.at<double>(0,0);
    double m02 = rotationMatrix.at<double>(0,2);
    double m10 = rotationMatrix.at<double>(1,0);
    double m11 = rotationMatrix.at<double>(1,1);
    double m12 = rotationMatrix.at<double>(1,2);
    double m20 = rotationMatrix.at<double>(2,0);
    double m22 = rotationMatrix.at<double>(2,2);

    double bank, attitude, heading;

    // Assuming the angles are in radians.
    if (m10 > 0.998) { // singularity at north pole
        bank = 0;
        attitude = CV_PI/2;
        heading = atan2(m02,m22);
    }
    else if (m10 < -0.998) { // singularity at south pole
        bank = 0;
        attitude = -CV_PI/2;
        heading = atan2(m02,m22);
    }
    else
    {
        bank = atan2(-m12,m11);
        attitude = asin(m10);
        heading = atan2(-m20,m00);
    }

    euler.at<double>(0) = bank;
    euler.at<double>(1) = attitude;
    euler.at<double>(2) = heading;

    return euler;
}

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
            auto rotation = rot2euler(rot);
            ofs << i << " " << mat.at<float>(0,3) << " " << mat.at<float>(1,3) << " " << mat.at<float>(2,3) << " " << rotation.at<float>(0,0) << " " << rotation.at<float>(0,1) << " " << rotation.at<float>(0,2) << std::endl;
        }
        i++;
    }

    SLAM.Shutdown();
    return 0;
}
