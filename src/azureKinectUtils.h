#pragma once

#include <k4a/k4a.hpp>
#include <glm/glm.hpp>

glm::ivec2 getColorResolution(const k4a_color_resolution_t resolution);
glm::ivec2 getDepthResolution(const k4a_depth_mode_t depthMode);
glm::ivec2 getDepthRange(const k4a_depth_mode_t depthMode);
