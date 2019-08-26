#pragma once

#include<Vars/Fwd.h>
#include<glm/glm.hpp>

void drawFace(vars::Vars&vars);
void drawFace(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj);
void updateFace(vars::Vars&vars);
void initFace(vars::Vars&vars);
