#include <math.h>  
#include <iostream>

class HoloCalibration
{
    public: 
        class Calibration
        {
            public:
            //default values for our small LKG, might work for other ones too
            float pitch {47.58427};
            float slope {-5.420652};
            float center {0.042};
            float viewCone {40.0};
            float invView {1.0};
            float verticalAngle {0.0};
            float dpi {338.0};
            float screenWidth {2560.0};
            float screenHeight {1600.0};
            float flipImageX {0.0};
            float flipImageY {0.0};
            float flipSubp {0.0};
            float recalculatedPitch() const {return pitch*(screenWidth/dpi)*cos(atan(1.0/slope));}
            float tilt() const {return (screenHeight/(screenWidth*slope))*((flipImageX == 1.0) ? -1 : 1);}
            float subp() const {return 1.0/(screenWidth*3);}
            friend std::ostream& operator<<(std::ostream& os, const Calibration& cal)
            {
                os  << "Pitch:\t" << cal.pitch << std::endl
                    << "Pitch_R:\t" << cal.recalculatedPitch() << std::endl
                    << "Slope:\t" << cal.slope << std::endl
                    << "Tilt:\t" << cal.tilt() << std::endl
                    << "Center:\t" << cal.center << std::endl
                    << "ViewCone:\t" << cal.viewCone << std::endl
                    << "InvView:\t" << cal.invView << std::endl
                    << "VerticalAngle:\t" << cal.verticalAngle << std::endl
                    << "DPI:\t" << cal.dpi << std::endl
                    << "ScreenWidth:\t" << cal.screenWidth << std::endl
                    << "ScreenHeight:\t" << cal.screenHeight << std::endl
                    << "FlipImageX:\t" << cal.flipImageX << std::endl
                    << "FlipImageY:\t" << cal.flipImageY << std::endl
                    << "FlipSubp:\t" << cal.flipSubp << std::endl
                    << "Subp:\t" << cal.subp() << std::endl
                    << std::endl;
                return os;
            }
        } ;
        static Calibration getCalibration();

    private:
        static constexpr int BUFFER_SIZE {255};
        static constexpr int INTERRUPT_SIZE {67};
        static constexpr int PACKET_NUM {7};
        static constexpr int VENDOR_ID {0x04d8};
        static constexpr int INTERFACE_NUM {2};
};
