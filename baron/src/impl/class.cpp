#include "baron/impl/class.h"
#include "baron/impl/method.h"
#include "baron/impl/field.h"

#include <cstdio>
#include <cstdint>

#ifdef BARON_DEBUG
#define LOG_BLACKLIST_MATCH \
fprintf(\
 jvm->getLog(),\
 "BARON INFO: Ignored JClass::newInstance for blacklisted constructor: <init>(%s)!\n",\
 signature\
);
#else
#define LOG_BLACKLIST_MATCH
#endif

#define NEW_INSTANCE(arg) \
auto jvm = (Baron::Jvm *)vm;\
if (jvm->isMethodBlacklisted("<init>", signature)) {\
 LOG_BLACKLIST_MATCH\
 return nullptr;\
}\
auto mid = getMethod(signature, "<init>");\
if (!mid) {\
mid = new JMethodID(\
 [&](JNIEnv * env, jobject clazzOrInst, jvalue * args) -> jvalue {\
  JavaVM * vm;\
  env->GetJavaVM(&vm);\
  auto jvm = (Baron::Jvm *)vm;\
  jvalue value;\
  value.l = ((FakeJni::JniEnv *)env)->createLocalReference(jvm->fabricateInstance(this));\
  return value;\
 },\
 signature,\
 "<init>",\
 FakeJni::JMethodID::PUBLIC\
);\
}\
FakeJni::LocalFrame frame (*(FakeJni::Jvm const *) vm);\
return frame.getJniEnv().resolveReference(mid->directInvoke<jvalue>(frame.getJniEnv(), (void *)this, arg));

namespace Baron::Internal {
 bool JClass::registerMethod(const FakeJni::JMethodID * mid, bool deallocate) const {
  return FakeJni::JClass::registerMethod(new JMethodID(mid, deallocate), true);
 }

// bool JClass::unregisterMethod(const FakeJni::JMethodID * mid) const noexcept {
//
// }

 bool JClass::registerField(FakeJni::JFieldID * fid, bool deallocate) const noexcept {
  return FakeJni::JClass::registerField(new JFieldID(fid, deallocate), true);
 }

// bool JClass::unregisterField(FakeJni::JFieldID * fid) const noexcept {
//
// }

 std::shared_ptr<FakeJni::JObject> JClass::newInstance(const JavaVM * vm, const char * signature, CX::va_list_t& list) const {
  NEW_INSTANCE(list)
 }

 std::shared_ptr<FakeJni::JObject> JClass::newInstance(const JavaVM * vm, const char * signature, const jvalue * values) const {
  NEW_INSTANCE(values)
 }
}