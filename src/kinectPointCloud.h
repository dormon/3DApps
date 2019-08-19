#pragma once

#include<Vars/Fwd.h>
#include<glm/glm.hpp>

void drawKPC(vars::Vars&vars);
void drawKPC(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj);
void initKPC(vars::Vars&vars);
void updateKPC(vars::Vars&vars);

