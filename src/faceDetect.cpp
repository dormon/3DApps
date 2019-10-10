#include <iostream>
#include "faceDetect.h" 

FaceDetector::FaceDetector(std::string classifierXML)
{
    cascade.load(classifierXML);    
}

glm::vec3 FaceDetector::getFaceCoords(cv::Mat& img, double scale) 
{ 
    std::vector<cv::Rect> faces; 
    cv::Mat gray;

    cvtColor( img, gray, cv::COLOR_BGR2GRAY ); 
    cascade.detectMultiScale( gray, faces, 1.1,2, 0|cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30) ); 

    if(!faces.empty())
    {
        cv::Rect r = faces[0];
        cv::Point center = (r.br()+r.tl())*0.5;
        if(glm::abs(static_cast<float>(center.y)/img.size().height-avg.y) > THRESHOLD)
            return avg;
        avg -= avg/WINDOW;
        avg += glm::vec3(static_cast<float>(center.x)/img.size().width, static_cast<float>(center.y)/img.size().height, static_cast<float>(r.width)/img.size().width)/WINDOW;
     }
    
    return avg;
}

FaceDetectorCapture::FaceDetectorCapture(std::string classifierXML) : FaceDetector(classifierXML)
{
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 5);
    capture.set(cv::CAP_PROP_BUFFERSIZE, 3);
    capture.open(0);
}

glm::vec3 FaceDetectorCapture::getFaceCoords(double scale)
{
    for(int i=0; i<FRAME_DROP; i++)
        capture.grab();
    capture.retrieve(frame);
    if(frame.empty())
        return glm::vec3(0.5);
    return FaceDetector::getFaceCoords(frame, scale);
}

