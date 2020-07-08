#include <Barrier.h>
#include <Vars/Resource.h>
#include <stdexcept>

using namespace std;

Barrier::Barrier(vars::Vars &          vars,
                 string const &        m,
                 vector<string> const &inputs)
    : vars(vars), method(m)
{
  for (auto const &i : inputs) {
    if (!vars.has(i))
      throw runtime_error(string("cannot create Barrier for method: ") +
                          method + string(", missing input variable: ") + i);
    resources.push_back(tuple<shared_ptr<vars::Resource>, size_t, string>(
        vars.getResource(i), vars.getTicks(i), i));
  }
}

bool Barrier::notChange()
{
  bool changed = firstCall;
  firstCall    = false;
  for (auto const &r : resources)
    if (get<0>(r)->getTicks() > get<1>(r)) {
      changed |= true;
      break;
    }
  if (changed)
    for (auto &r : resources) get<1>(r) = get<0>(r)->getTicks();
  return !changed;
}

ObjectData::ObjectData(vars::Vars &vars) : vars(vars) {}

shared_ptr<Barrier> ObjectData::addMethod(string const &        method,
                                          vector<string> const &inputs)
{
  auto it = barriers.find(method);
  if (it != barriers.end()) return it->second;
  barriers[method] = make_shared<Barrier>(vars, method, inputs);
  return barriers.at(method);
}

bool notChanged(vars::Vars &          vars,
                string const &        objectName,
                string const &        method,
                vector<string> const &inputs)
{
  if (!vars.has(objectName)) vars.add<ObjectData>(objectName, vars);
  auto obj     = vars.get<ObjectData>(objectName);
  auto barrier = obj->addMethod(method, inputs);
  return barrier->notChange();
}
