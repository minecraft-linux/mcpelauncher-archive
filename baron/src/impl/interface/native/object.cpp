#include "baron/impl/interface/native.h"

#include <algorithm>

namespace Baron::Interface {
 jobject NativeInterface::allocObject(jclass jclazz) const {
  auto clazz = env.resolveReference(jclazz).get();
  return env.createLocalReference(vm.fabricateInstance((FakeJni::JClass *) clazz));
 }

 jclass NativeInterface::getObjectClass(jobject jobj) const {
  auto obj = env.resolveReference(jobj).get();
  auto & fabInstances = vm.fabricatedInstances;
  auto end = fabInstances.end();
  if (std::find(fabInstances.begin(), end, obj) != end) {
   for (auto &[clazz, instances] : vm.fabricatedClassMappings) {
    end = instances.end();
    if (std::find(instances.begin(), end, obj) != end) {
     return (jclass) env.createLocalReference(const_cast<FakeJni::JClass *>(clazz)->shared_from_this());
    }
   }
   //TODO this should never happen
//   throw std::runtime_error("FATAL: ");
   return nullptr;
  }
  return FakeJni::NativeInterface::getObjectClass(jobj);
 }

 jboolean NativeInterface::isInstanceOf(jobject jobj, jclass jclazz) const {
  using namespace FakeJni;
  auto clazz = std::dynamic_pointer_cast<FakeJni::JClass>(env.resolveReference(jclazz));
  if (clazz->isArbitrary) {
   auto obj = env.resolveReference(jobj).get();
   auto clazz = (JClass *) env.resolveReference(jclazz).get();
   auto& instances = vm.fabricatedClassMappings[clazz];
   auto end = instances.end();
   return (jboolean)(std::find(instances.begin(), end, obj) != end);
  }
  return FakeJni::NativeInterface::isSameObject(jobj, jclazz);
 }
}