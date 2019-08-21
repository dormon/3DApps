#include <opencv2/objdetect.hpp> 
#include <opencv2/imgproc.hpp> 
#include <glm/glm.hpp>
#include <iostream> 
  
class FaceDetector
{
    public:
        glm::vec2 getFaceCoords( cv::Mat& img, double scale ); 
        FaceDetector(std::string classifierXML);
    private:
       cv::CascadeClassifier cascade; 
}; 
