#include <azureKinectUtils.h>

glm::ivec2 getColorResolution(const k4a_color_resolution_t resolution)
{
    switch (resolution)
    {
        case K4A_COLOR_RESOLUTION_720P:
            return { 1280, 720 };
        case K4A_COLOR_RESOLUTION_2160P:
            return { 3840, 2160 };
        case K4A_COLOR_RESOLUTION_1440P:
            return { 2560, 1440 };
        case K4A_COLOR_RESOLUTION_1080P:
            return { 1920, 1080 };
        case K4A_COLOR_RESOLUTION_3072P:
            return { 4096, 3072 };
        case K4A_COLOR_RESOLUTION_1536P:
            return { 2048, 1536 };

        default:
            throw std::logic_error("Invalid color dimensions value!");
    }
}

glm::ivec2 getDepthResolution(const k4a_depth_mode_t depthMode)
{
    switch (depthMode)
    {
        case K4A_DEPTH_MODE_NFOV_2X2BINNED:
            return { 320, 288 };
        case K4A_DEPTH_MODE_NFOV_UNBINNED:
            return { 640, 576 };
        case K4A_DEPTH_MODE_WFOV_2X2BINNED:
            return { 512, 512 };
        case K4A_DEPTH_MODE_WFOV_UNBINNED:
            return { 1024, 1024 };
        case K4A_DEPTH_MODE_PASSIVE_IR:
            return { 1024, 1024 };

        default:
            throw std::logic_error("Invalid depth dimensions value!");
    }
}

glm::ivec2 getDepthRange(const k4a_depth_mode_t depthMode)
{
    switch (depthMode)
    {
        case K4A_DEPTH_MODE_NFOV_2X2BINNED:
            return { 500, 5800 };
        case K4A_DEPTH_MODE_NFOV_UNBINNED:
            return { 500, 4000 };
        case K4A_DEPTH_MODE_WFOV_2X2BINNED:
            return { 250, 3000 };
        case K4A_DEPTH_MODE_WFOV_UNBINNED:
            return { 250, 2500 };

        case K4A_DEPTH_MODE_PASSIVE_IR:
        default:
            throw std::logic_error("Invalid depth mode!");
    }
}

