#include <iostream>
#include "faceDetect.h" 
 
FaceDetector::FaceDetector(std::string classifierXML)
{
    cascade.load(classifierXML);    
}
  
glm::vec2 FaceDetector::getFaceCoords( cv::Mat& img, double scale) 
{ 
    std::vector<cv::Rect> faces; 
    cv::Mat gray;
  
    cvtColor( img, gray, cv::COLOR_BGR2GRAY ); 
    cascade.detectMultiScale( gray, faces, 1.1,2, 0|cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30) ); 

    cv::Rect r = faces[0];
    cv::Point center = (r.br()+r.tl())*0.5;
    return glm::vec2(center.x, center.y); 
  
/* for ( size_t i = 0; i < faces.size(); i++ )
    {
    Rect r = faces[i];
    Scalar color = Scalar(255, 0, 0);
    cv::Point center = (r.br()+r.tl())*0.5;
    circle( img, center, 6, color);
    } 
    // Show Processed Image with detected faces 
    imshow( "Face Detection", img ); */ 
} 
