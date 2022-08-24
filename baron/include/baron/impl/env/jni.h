#pragma once

#include <fake-jni/jvm.h>

namespace Baron::Env {
 class JniEnv : public FakeJni::JniEnv {
 public:
  JniEnv(const FakeJni::Jvm& vm);
 };
}