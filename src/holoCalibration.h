class HoloCalibration
{
    public: 
        struct Calibration
        {
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
        } ;
        static Calibration getCalibration();

    private:
        static constexpr int BUFFER_SIZE {255};
        static constexpr int INTERRUPT_SIZE {67};
        static constexpr int PACKET_NUM {7};
        static constexpr int VENDOR_ID {0x04d8};
        static constexpr int INTERFACE_NUM {2};
};
