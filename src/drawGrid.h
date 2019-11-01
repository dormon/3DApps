#pragma once

#include<Vars/Vars.h>
#include<glm/glm.hpp>

void drawGrid(vars::Vars&vars);
void drawGrid(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj);
