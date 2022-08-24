#pragma once

#include <cstring>

#include "baron/impl/jvm.h"

namespace Baron::Internal {
 class JFieldID : public FakeJni::JFieldID {
 public:
  using FakeJni::JFieldID::JFieldID;

  jvalue get(const FakeJni::JniEnv &env, FakeJni::JObject * obj) const override;
  void set(const FakeJni::JniEnv &env, FakeJni::JObject * obj, void * value) const override;

 private:
  const FakeJni::JClass * resolveType(const Jvm * vm) const {
   const auto name = getName();
   return vm->findClass(name).get();
  }
 };
}