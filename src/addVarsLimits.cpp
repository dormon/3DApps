#include <addVarsLimits.h>
#include <Vars/Vars.h>

#include <iostream>

void addVarsLimitsF(vars::Vars& vars,std::string const&name,float mmin,float mmax,float step){
  auto lim = vars.addOrGet<ImguiLimits>(imguiLimitsVariable);
  lim->setLimit<float>(name,mmin,mmax,step);
}

void addVarsLimitsU(vars::Vars& vars,std::string const&name,uint32_t mmin,uint32_t mmax,uint32_t step){
  auto lim = vars.addOrGet<ImguiLimits>(imguiLimitsVariable);
  lim->setLimit<uint32_t>(name,mmin,mmax,step);
}
 
void hide(vars::Vars&vars,std::string const&n){
  auto lim = vars.addOrGet<ImguiLimits>(imguiLimitsVariable);
  lim->hide(n);
}

void show(vars::Vars&vars,std::string const&n){
  auto lim = vars.addOrGet<ImguiLimits>(imguiLimitsVariable);
  lim->show(n);
}
