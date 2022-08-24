#include <fake-jni/array.h>
#include "baron/impl/interface/native.h"

#ifdef BARON_DEBUG
#define LOG_BLACKLIST_MATCH \
fprintf(\
 vm.getLog(),\
 "BARON INFO: Ignored blacklisted method lookup '%s#%s%s'!\n",\
 className,\
 name,\
 sig\
);
#else
#define LOG_BLACKLIST_MATCH
#endif

#ifdef BARON_DEBUG
#define LOG_FABRICATED_METHOD \
fprintf(vm->getLog(), "BARON INFO: Fabricated method '%s%s' -> 0x%lx on class '%s'\n", name, sig, (intptr_t)mid, className);
#else
#define LOG_FABRICATED_METHOD
#endif

#define CHECK_BLACKLIST \
auto clazz = std::dynamic_pointer_cast<JClass>(env.resolveReference(clazzRef)); \
const auto className = clazz->getName();\
if (vm.isClassBlacklisted(className) || vm.isMethodBlacklisted(name, sig, className)) {\
 LOG_BLACKLIST_MATCH\
 return nullptr;\
}

#ifdef BARON_DEBUG
#define LOG_ARBITRARY_CALLBACK \
const auto& log = vm->getLog();\
fprintf(log, "BARON INFO: Invoked fabricated function: %s::%s%s\n", className, name, sig);
#else
#define LOG_ARBITRARY_CALLBACK
#endif

#define DEFINE_ARBITRARY_CALLBACK \
Jvm *vm = &this->vm;\
auto retClazz = resolveReturnClass(sig);\
const auto callback = [=](JNIEnv * jenv, jobject jobj, jvalue * values) -> jvalue {\
 LOG_ARBITRARY_CALLBACK\
 return vm->fabricateValue(*(FakeJni::JniEnv *) jenv, retClazz.get());\
};


//TODO once fake-jni supports user-defined core classes, append the backtrace to the JMethodID for later debugging
namespace Baron::Interface {
 //TODO create backtrace of invocation
 jmethodID NativeInterface::getMethodID(jclass const clazzRef, const char * name, const char * sig) const {
  using namespace FakeJni;
  CHECK_BLACKLIST
  DEFINE_ARBITRARY_CALLBACK
  auto mid = (JMethodID *)FakeJni::NativeInterface::getMethodID(clazzRef, name, sig);
  if (!mid) {
   mid = new JMethodID(callback, sig, name, JMethodID::PUBLIC);
   clazz->registerMethod(mid);
   LOG_FABRICATED_METHOD
  }
  //search for method again incase instrumentation occurred downstream (JClass::registerMethod)
  return FakeJni::NativeInterface::getMethodID(clazzRef, name, sig);
 }

 //TODO create backtrace of invocation
 jmethodID NativeInterface::getStaticMethodID(jclass clazzRef, const char * name, const char * sig) const {
  using namespace FakeJni;
  CHECK_BLACKLIST
  DEFINE_ARBITRARY_CALLBACK
  auto mid = (JMethodID *)FakeJni::NativeInterface::getStaticMethodID(clazzRef, name, sig);
  if (!mid) {
   mid = new JMethodID(callback, sig, name, JMethodID::PUBLIC | JMethodID::STATIC);
   clazz->registerMethod(mid);
   LOG_FABRICATED_METHOD
  }
  return FakeJni::NativeInterface::getStaticMethodID(clazzRef, name, sig);
 }

 std::shared_ptr<FakeJni::JClass const> NativeInterface::resolveReturnClass(const char *sig) const {
  const char *retSig = sig;
  while (*retSig && *retSig != ')') ++retSig;
  if (*retSig == ')') ++retSig;
  if (*retSig == 'L') {
   ++retSig;
   auto i = strchr(retSig, ';');
   if (i == nullptr)
       return std::shared_ptr<FakeJni::JClass const>();
   std::string qualifiedName(retSig, i - retSig);
   return env.vm.findClass(qualifiedName.c_str());
  }
  /*
  if (sig[0] != '\0' && sig[1] == '\0') {
   switch (sig[0]) {
    case 'Z': return FakeJni::booleanDescriptor;
    case 'B': return FakeJni::byteDescriptor;
    case 'C': return FakeJni::charDescriptor;
    case 'S': return FakeJni::shortDescriptor;
    case 'I': return FakeJni::intDescriptor;
    case 'J': return FakeJni::longDescriptor;
    case 'F': return FakeJni::floatDescriptor;
    case 'D': return FakeJni::doubleDescriptor;
    default: break;
   }
  }*/
  auto ret = env.vm.findClass(retSig);
  if (ret)
   return ret;
  if (retSig[0] == '[')
   return FakeJni::JArray<FakeJni::JObject>::getDescriptor();
  throw std::runtime_error("bad return class");
 }

}
