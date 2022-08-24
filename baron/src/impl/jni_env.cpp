#include "baron/impl/interface/native.h"
#include "baron/impl/env/jni.h"

namespace Baron::Env {
 JniEnv::JniEnv(const FakeJni::Jvm& vm) : FakeJni::JniEnv(vm) {
  setNativeInterface<Baron::Interface::NativeInterface>();
 }
}