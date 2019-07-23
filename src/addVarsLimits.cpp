#include <addVarsLimits.h>
#include <Vars/Vars.h>

#include <iostream>

void setVarsLimitsPostfix(vars::Vars& vars,std::string const&name){
  vars.addString(limitsPostfixVariable,name);
}

void addVarsLimitsF(vars::Vars& vars,std::string const&name,float mmin,float mmax,float step){
  auto const postfix = vars.addOrGetString(limitsPostfixVariable,drawImguiVarsDefaultPostfix);
  vars.reCreate<VarsLimits<float>>(name+postfix,mmin,mmax,step);
}

void addVarsLimitsU(vars::Vars& vars,std::string const&name,uint32_t mmin,uint32_t mmax,uint32_t step){
  auto const postfix = vars.addOrGetString(limitsPostfixVariable,drawImguiVarsDefaultPostfix);
  vars.reCreate<VarsLimits<uint32_t>>(name+postfix,mmin,mmax,step);
}
 
