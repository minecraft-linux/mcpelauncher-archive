#include "baron/impl/method.h"

#define VIRTUAL_INVOKE(arg) \
const FakeJni::JClass * clazz = nullptr;\
for (auto &[clz, instances] : jvm->fabricatedClassMappings) {\
 if (instances.find(clazzOrObj) != instances.end()) {\
  clazz = clz;\
  break;\
 }\
}\
if (!clazz) {\
 throw std::runtime_error("FATAL: Could not find class mapping for fabricated object!");\
}\
return findVirtualMatch(clazz)->directInvoke<jvalue>(env, clazzOrObj, arg);

namespace Baron::Internal {
 jvalue JMethodID::virtualInvoke(const FakeJni::JniEnv &env, FakeJni::JObject * clazzOrObj, CX::va_list_t &list) const {
  auto jvm = (Baron::Jvm *)&env.vm;
  if (jvm->isFabricated(clazzOrObj)) {
   VIRTUAL_INVOKE(list)
  }
  return FakeJni::JMethodID::virtualInvoke(env, clazzOrObj, list);
 }

 jvalue JMethodID::virtualInvoke(const FakeJni::JniEnv &env, FakeJni::JObject * clazzOrObj, const jvalue * args) const {
  auto jvm = (Baron::Jvm *)&env.vm;
  if (jvm->isFabricated(clazzOrObj)) {
   VIRTUAL_INVOKE(args)
  }
  return FakeJni::JMethodID::virtualInvoke(env, clazzOrObj, args);
 }
}