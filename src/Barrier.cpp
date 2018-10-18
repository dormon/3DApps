#include <Barrier.h>
#include <Vars/Resource.h>


Barrier::Barrier(vars::Vars&vars,std::vector<std::string>const&inputs):vars(vars){
  for(auto const&i:inputs){
    if(!vars.has(i))
      throw std::runtime_error(std::string("cannot create Barrier, missing input variable: ")+i);
    resources.push_back(std::tuple<std::shared_ptr<vars::Resource>,size_t,std::string>(vars.getResource(i),vars.getTicks(i),i));
  }
}

bool Barrier::notChange(){
  bool changed = firstCall;
  firstCall = false;
  for(auto const&r:resources)
    if(std::get<0>(r)->getTicks() > std::get<1>(r)){
      changed |= true;
      break;
    }
  if(changed)
    for(auto &r:resources)
      std::get<1>(r) = std::get<0>(r)->getTicks();
  return !changed;
}


ObjectData::ObjectData(vars::Vars&vars):vars(vars){}

std::shared_ptr<Barrier> ObjectData::addMethod(std::string const&method,std::vector<std::string>const&inputs){
  auto it = barriers.find(method);
  if(it != barriers.end())return it->second;
  barriers[method] = std::make_shared<Barrier>(vars,inputs);
  return barriers.at(method);
}

bool notChanged(
    vars::Vars                   &vars      ,
    std::string             const&objectName,
    std::string             const&method    ,
    std::vector<std::string>const&inputs    )
{
  if(!vars.has(objectName))
    vars.add<ObjectData>(objectName,vars);
  auto obj = vars.get<ObjectData>(objectName);
  auto barrier = obj->addMethod(method,inputs);
  return barrier->notChange();
}

