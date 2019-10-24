#pragma once

#include <Vars/Vars.h>
#include <limits>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <typeindex>

static constexpr char const* drawImguiVarsDefaultPostfix = "___limits";
static constexpr char const* limitsPostfixVariable = "___imguiVarsPostfix";
static constexpr char const* imguiLimitsVariable = "___varsLimits";

class VarLimit{
  public:
    virtual ~VarLimit(){}
};

template<typename T>
class VarsLimits : public VarLimit {
 public:
  VarsLimits(T mmin = 0, T mmax = 1e20, T s = 1)
      : minValue(mmin), maxValue(mmax), step(s)
  {
  }
  T minValue;
  T maxValue;
  T step;
};

class EnumLimits{
 public:
  EnumLimits(
      std::vector<int32_t>    const&v = {},
      std::vector<const char*>const&n = {}):values(v),names(n){
    for(size_t i=0;i<v.size();++i)
      valueToIndex[v.at(i)] = i;
  }
  std::map<int32_t,size_t>valueToIndex;
  std::vector<int32_t>values;
  std::vector<const char*>names;
};


class ImguiLimits{
  public:
    template<typename T>
    void setLimit(std::string const&n,T mmin,T mmax,T step){
      auto it = limits.find(n);
      if(it == std::end(limits)){
        limits[n] = std::make_shared<VarsLimits<T>>();
        it = limits.find(n);
      }else{
        auto l = std::dynamic_pointer_cast<VarsLimits<T>>(it->second);
        l->maxValue = mmax;
        l->minValue = mmin;
        l->step     = step;
      }
    }
    template<typename T>
    void setEnum(
      std::vector<int32_t>    const&v = {},
      std::vector<const char*>const&n = {}){
      auto it = enums.find(typeid(T));
      if(it == std::end(enums)){
        enums[typeid(T)] = std::make_shared<EnumLimits>(v,n);
      }else{
        auto e = it->second;
        e->names = n;
        e->values = v;
      }
    }
    void hide(std::string const&n){hidden.insert(n);}
    void show(std::string const&n){hidden.erase(n);}
    bool isHidden(std::string const&n){return hidden.count(n)!=0;}
    std::map<std::string,std::shared_ptr<VarLimit>>limits;
    std::set<std::string>hidden;
    std::map<std::type_index const,std::shared_ptr<EnumLimits>>enums;
};


void setVarsLimitsPostfix(vars::Vars&        vars,
                      std::string const& name = drawImguiVarsDefaultPostfix);
void addVarsLimitsF(vars::Vars&        vars,
               std::string const& name,
               float              mmin = -1e38,
               float              mmax = +1e38,
               float              step = 1.f);
void addVarsLimitsU(vars::Vars&        vars,
               std::string const& name,
               uint32_t           mmin = 0,
               uint32_t           mmax = 0xffffffff,
               uint32_t           step = 1);

template<typename ENUM>
void addEnumValues(
    vars::Vars                   &vars  ,
    std::vector<int>        const&values,
    std::vector<char const*>const&names ){
  auto lim = vars.addOrGet<ImguiLimits>(imguiLimitsVariable);
  lim->setEnum<ENUM>(values,names);
}
