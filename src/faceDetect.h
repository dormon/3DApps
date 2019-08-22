#include <opencv2/objdetect.hpp> 
#include <opencv2/imgproc.hpp> 
#include <opencv2/highgui.hpp>
#include <glm/glm.hpp>

class FaceDetector
{
    public:
        glm::vec3 getFaceCoords( cv::Mat& img, double scale ); 
        FaceDetector(std::string classifierXML);
    private:
        cv::CascadeClassifier cascade;
        constexpr static float WINDOW{20};
        glm::vec3 avg{0.5f};
};

class FaceDetectorCapture : FaceDetector
{
    public:
        FaceDetectorCapture(std::string classifierXML); 
        glm::vec3 getFaceCoords(double scale);
    private:
        constexpr static int FRAME_DROP = 1;
        cv::VideoCapture capture;
        cv::Mat frame;
}; 
