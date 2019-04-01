#include <imguiVars.h>

#include <imguiSDL2OpenGL/imgui.h>

#include<vector>
#include<map>
#include<iostream>
#include<glm/glm.hpp>

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


void drawGroup(std::unique_ptr<Group>const&group,vars::Vars &vars){
  if(group->isVariable){
    auto const n = group->name;
    auto const fn = group->fullName;
    bool change = false;
    if(vars.getType(fn) == typeid(float)){
      change = ImGui::DragFloat(n.c_str(),(float*)vars.get(fn),0.1f);
    }
    if(vars.getType(fn) == typeid(uint32_t)){
      change = ImGui::InputScalar(n.c_str(),ImGuiDataType_U32,(uint32_t*)vars.get(fn));
      //ImGui::DragInt(n.c_str(),(int32_t*)vars.get(fn),1,0);
    }
    if(vars.getType(fn) == typeid(uint64_t)){
      change = ImGui::InputScalar(n.c_str(),ImGuiDataType_U64,(uint64_t*)vars.get(fn));
    }
    if(vars.getType(fn) == typeid(bool)){
      change = ImGui::Checkbox(n.c_str(),(bool*)vars.get(fn));
    }
    if(vars.getType(fn) == typeid(glm::vec4)){
      change = ImGui::DragFloat4(n.c_str(),(float*)vars.get(fn));
    }
    if(vars.getType(fn) == typeid(glm::vec3)){
      change = ImGui::DragFloat3(n.c_str(),(float*)vars.get(fn));
    }
    if(vars.getType(fn) == typeid(glm::vec2)){
      change = ImGui::DragFloat2(n.c_str(),(float*)vars.get(fn));
    }
    //if(vars.getType(fn) == typeid(std::string)){
    //  ImGui::TextV
    //}
    if(change)
      vars.updateTicks(fn);
  }

  if(group->children.empty())return;

  if(ImGui::TreeNode(group->name.c_str())){
    for(auto const&x:group->children)
      drawGroup(x.second,vars);
    ImGui::TreePop();
  }
}


void drawImguiVars(vars::Vars &vars,vars::Vars &&limits){
  std::vector<std::string>names;
  for(size_t i = 0;i<vars.getNofVars();++i)
    names.push_back(vars.getVarName(i));
  
  VarNamesHierarchy hierarchy(names);
  

  ImGui::Begin("vars");
  ImGui::PushItemWidth(-120);
  ImGui::LabelText("label", "Value");

  for(auto const&x:hierarchy.groups)
    drawGroup(x.second,vars);

  ImGui::End();

  
}

