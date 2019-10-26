#include <imguiVars.h>
#include <Vars/Vars.h>
#include <addVarsLimits.h>

#include <imguiSDL2OpenGL/imgui.h>

#include<vector>
#include<map>
#include<iostream>
#include<glm/glm.hpp>
#include<cstring>

std::string getHead(std::string const&n){
  return n.substr(0,n.find("."));
}

std::string getTail(std::string const&n){
  return n.substr(n.find(".")+1);
}

bool hasSplit(std::string const&n){
  return n.find(".") != std::string::npos;
}

class Group{
  public:
    Group(std::string const&n,std::string const&fn = ""):name(n),fullName(fn){}
    std::string name;
    std::string fullName;
    bool isVariable = false;
    std::map<std::string,std::unique_ptr<Group>>children;
};



class VarNamesHierarchy{
  public:
    std::map<std::string,std::unique_ptr<Group>>groups;
    VarNamesHierarchy(std::vector<std::string>const&names){
      for(auto const&name:names)
        insertIntoChildren(groups,name,name);
    }
    void insertIntoChildren(std::map<std::string,std::unique_ptr<Group>>&children,std::string const&name,std::string const&fullName){
      if(hasSplit(name)){
        auto const groupName  = getHead(name);
        auto const subVarName = getTail(name);
        createChildGroup(children,groupName);
        insertIntoChildren(children.at(groupName)->children,subVarName,fullName);
      }else{
        createChildGroup(children,name,fullName);
        children[name]->isVariable = true;
        children[name]->fullName   = fullName;
      }
    }
    void createChildGroup(std::map<std::string,std::unique_ptr<Group>>&children,std::string const&name,std::string const&fullName = ""){
      if(children.count(name)!=0)return;
      children[name] = std::make_unique<Group>(name,fullName);
    }
};


void drawVariable(std::unique_ptr<Group>const&group,vars::Vars &vars,ImguiLimits const*const lims){
  auto const n = group->name;
  auto const fn = group->fullName;
  bool change = false;
  auto const& type = vars.getType(fn);

  if(lims&&lims->isHidden(fn))
    return;

  if(vars.getKind(fn) == vars::ResourceKind::ENUM){
    if(lims){
      auto e = lims->enums.at(vars.getType(fn));
      auto const&names = e->names;
      auto&val = vars.getInt32(fn);
      auto it = e->valueToIndex.find(val);
      int32_t idx = 0;
      if(it != std::end(e->valueToIndex))
        idx = e->valueToIndex.at(val);

      change = ImGui::ListBox(n.c_str(), (int32_t*)&idx, names.data(), names.size(), 4);
      val = e->values.at(idx);
    }else{
      change = ImGui::DragScalar(n.c_str(),ImGuiDataType_S32,(int32_t*)vars.get(fn),1);
    }
  }
#define DRAG_SCALAR(TYPE,ENUM)\
    if(lim){\
      change = ImGui::DragScalar(n.c_str(),ENUM,(TYPE*)vars.get(fn),lim->step,&lim->minValue,&lim->maxValue);\
    }else\
      change = ImGui::DragScalar(n.c_str(),ENUM,(TYPE*)vars.get(fn),1)

  if(type == typeid(float)){
    auto lim = lims?lims->getLimit<float>(fn):nullptr;
    if(lim){
      change = ImGui::DragFloat(n.c_str(),(float*)vars.get(fn),lim->step,lim->minValue,lim->maxValue);
    }else
      change = ImGui::DragFloat(n.c_str(),(float*)vars.get(fn));
  }
  else if(type == typeid(uint64_t)){
    auto lim = lims?lims->getLimit<uint64_t>(fn):nullptr;
    DRAG_SCALAR(uint64_t,ImGuiDataType_U64);
  }
  else if(type == typeid(uint32_t)){
    auto lim = lims?lims->getLimit<uint32_t>(fn):nullptr;
    DRAG_SCALAR(uint32_t,ImGuiDataType_U32);
  }
  else if(type == typeid(int64_t)){
    auto lim = lims?lims->getLimit<int64_t>(fn):nullptr;
    DRAG_SCALAR(int64_t,ImGuiDataType_S64);
  }
  else if(type == typeid(int32_t)){
    auto lim = lims?lims->getLimit<int32_t>(fn):nullptr;
    DRAG_SCALAR(int32_t,ImGuiDataType_S32);
  }
  else if(type == typeid(bool)){
    change = ImGui::Checkbox(n.c_str(),(bool*)vars.get(fn));
  }
  else if(type == typeid(glm::vec4)){
    auto lim = lims?lims->getLimit<float>(fn):nullptr;
    if(lim)
      change = ImGui::DragFloat4(n.c_str(),(float*)vars.get(fn),lim->step,lim->minValue,lim->maxValue);
    else
      change = ImGui::DragFloat4(n.c_str(),(float*)vars.get(fn));
  }
  else if(type == typeid(glm::vec3)){
    auto lim = lims?lims->getLimit<float>(fn):nullptr;
    if(lim)
      change = ImGui::DragFloat3(n.c_str(),(float*)vars.get(fn),lim->step,lim->minValue,lim->maxValue);
    else
      change = ImGui::DragFloat3(n.c_str(),(float*)vars.get(fn));
  }
  else if(type == typeid(glm::vec2)){
    auto lim = lims?lims->getLimit<float>(fn):nullptr;
    if(lim)
      change = ImGui::DragFloat2(n.c_str(),(float*)vars.get(fn),lim->step,lim->minValue,lim->maxValue);
    else
      change = ImGui::DragFloat2(n.c_str(),(float*)vars.get(fn));
  }
  else if(type == typeid(glm::uvec2)){
    change = ImGui::DragInt2(n.c_str(),(int*)vars.get(fn));
  }
  else if(type == typeid(std::string)){
    auto&str = vars.getString(fn);
    auto const maxV = [](size_t i,size_t j){if(i>j)return i;return j;};
    auto const size = maxV(str.length()*2,256);
    auto buf = new char[size];
    std::strcpy(buf,str.c_str());
    change = ImGui::InputText(n.c_str(),buf,size);
    str = buf;
    delete[]buf;
  }
#undef DRAG_SCALAR
  if(change)
    vars.updateTicks(fn);
}


void drawGroup(std::unique_ptr<Group>const&group,vars::Vars &vars,ImguiLimits const*const lims){
  if(group->isVariable)
    drawVariable(group,vars,lims);

  if(group->children.empty())return;

  if(ImGui::TreeNode(group->name.c_str())){
    for(auto const&x:group->children)
      drawGroup(x.second,vars,lims);
    ImGui::TreePop();
  }
}


void drawImguiVars(vars::Vars &vars){
  std::vector<std::string>names;
  for(size_t i = 0;i<vars.getNofVars();++i)
    names.push_back(vars.getVarName(i));
  
  VarNamesHierarchy hierarchy(names);

  ImGui::Begin("vars");
  ImGui::PushItemWidth(-160);
  ImGui::LabelText("label", "Value");

  ImguiLimits const* lims = nullptr;
  if(vars.has(imguiLimitsVariable))
    lims = vars.get<ImguiLimits>(imguiLimitsVariable);

  for(auto const&x:hierarchy.groups)
    drawGroup(x.second,vars,lims);

  ImGui::End();
}
