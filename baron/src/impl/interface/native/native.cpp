#include "baron/impl/interface/native.h"

namespace Baron::Interface {
 NativeInterface::NativeInterface(FakeJni::JniEnv& env) :
  FakeJni::NativeInterface(env),
  vm(dynamic_cast<Jvm&>(env.vm))
 {}
}