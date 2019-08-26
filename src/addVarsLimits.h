#pragma once

#include <Vars/Fwd.h>
#include <limits>

static constexpr char const* drawImguiVarsDefaultPostfix = "___limits";
static constexpr char const* limitsPostfixVariable = "___imguiVarsPostfix";

template<typename T>
class VarsLimits {
 public:
  VarsLimits(T mmin = 0, T mmax = 1e20, T s = 1)
      : minValue(mmin), maxValue(mmax), step(s)
  {
  }
  T minValue;
  T maxValue;
  T step;
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
