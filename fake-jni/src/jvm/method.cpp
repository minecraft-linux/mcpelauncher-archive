#include "fake-jni/jvm.h"
#include "fake-jni/internal/util.h"

#include <cx/tuple.h>
#include <cx/vararg.h>

#include <ffi.h>

#include <map>
#include <functional>

#define PARSER_CALLBACK(identifier, value) \
parser[#identifier[0]] = [&](char * token) { prototype.push_back(&value); };

#define RESOLVER_CASE(a_type, ffi_type) \
if (type == &ffi_type) {\
 return CX::Tuple<void (*)(), void (*)(), void (*)()> {\
  (void (*)())&NativeInvocationManager<a_type>::allocate<CX::va_list_t&>,\
  (void (*)())&NativeInvocationManager<a_type>::allocate<jvalue *>,\
  (void (*)())&NativeInvocationManager<a_type>::deallocate\
 };\
}

#define JMETHODID_INVOCATION_PREP \
const auto argc = descriptor->nargs - 2;\
for (unsigned int i = 0; i < argc; i++) {\
 const auto typeResolvers = typeResolverGenerator(descriptor->arg_types[i + 2]);\
 resolvers[i] = typeResolvers.get<0>();\
 resolvers[i + argc] = typeResolvers.get<1>();\
 deallocators[i] = typeResolvers.get<2>();\
}

//TODO find a better name for this glue template
template<typename Arg>
struct NativeInvocationManager {
 template<typename ListType>
 static void * allocate(ListType);

 template<>
 static void * allocate<CX::va_list_t&>(CX::va_list_t& list) {
  return new Arg;
 }

 template<>
 static void * allocate<jvalue *>(jvalue * const values) {
  /* TODO:
  const auto data = new Arg;
  *data = FakeJni::_CX::getAArg<Arg>(values);
  return (void *)data;*/
  return new Arg;
 }

 static void deallocate(void * const data) {
  delete (Arg *)data;
 }
};

//Generates the type resolvers, allocators and deallocators for the respective JNI argument list
static auto typeResolverGenerator(ffi_type * const type) {
 RESOLVER_CASE(FakeJni::JBoolean, ffi_type_uint8)
 RESOLVER_CASE(FakeJni::JByte, ffi_type_sint8)
 RESOLVER_CASE(FakeJni::JChar, ffi_type_uint16)
 RESOLVER_CASE(FakeJni::JShort, ffi_type_sint16)
 RESOLVER_CASE(FakeJni::JInt, ffi_type_sint32)
 RESOLVER_CASE(FakeJni::JFloat, ffi_type_float)
 RESOLVER_CASE(FakeJni::JLong, ffi_type_sint64)
 RESOLVER_CASE(FakeJni::JDouble, ffi_type_double)
 RESOLVER_CASE(FakeJni::JObject *, ffi_type_pointer)
 //This should never happen, if it does there's probably memory corruption
 throw std::runtime_error("FATAL: Could not resolve valid FFI type after parsing!");
};

//Non-template members of JMethodID
namespace FakeJni {
 std::map<size_t, std::pair<unsigned long, ffi_cif *>> JMethodID::descriptors;

 //TODO
 char * JMethodID::verifyName(const char * name) {
  return const_cast<char *>(name);
 }

 //TODO
 char * JMethodID::verifySignature(const char * sig) {
  return const_cast<char *>(sig);
 }

 std::vector<ffi_type *> JMethodID::getFfiPrototype(const char * signature, const char * name) {
  struct Parser {
   JniFunctionParser<> parser;
   std::vector<ffi_type *> prototype;

   Parser() {
    PARSER_CALLBACK(V, ffi_type_void)
    PARSER_CALLBACK(Z, ffi_type_uint8)
    PARSER_CALLBACK(B, ffi_type_sint8)
    PARSER_CALLBACK(C, ffi_type_uint16)
    PARSER_CALLBACK(S, ffi_type_sint16)
    PARSER_CALLBACK(I, ffi_type_sint32)
    PARSER_CALLBACK(F, ffi_type_float)
    PARSER_CALLBACK(J, ffi_type_sint64)
    PARSER_CALLBACK(D, ffi_type_double)
    PARSER_CALLBACK([, ffi_type_pointer)
    PARSER_CALLBACK(L, ffi_type_pointer)
   }

   std::vector<ffi_type *> parse(const char * signature) const {
    auto& ref = const_cast<Parser&>(*this);
    ref.prototype.clear();
    parser.parse(signature);
    return prototype;
   }
  };

  static const Parser parser;
  return parser.parse(signature);
 }

 JMethodID::JMethodID(const FakeJni::JMethodID * mid, bool dealloc) :
  JNINativeMethod {
   [&]() -> char *{
    if (!mid) {
     throw std::runtime_error("FATAL: Tried to create composed method descriptor with nullptr!");
    }
    return nullptr;
   }(),
   nullptr,
   (void *)mid
  },
  type(COMPOSED_FUNC),
  dealloc(dealloc),
  isArbitrary(false)
 {}

 JMethodID::JMethodID(const JNINativeMethod * method) :
  _jmethodID(),
  JNINativeMethod {
   verifyName(_INTERNAL_ARBITRARY_ALLOC_STR(method->name)),
   verifySignature(_INTERNAL_ARBITRARY_ALLOC_STR(method->signature)),
//   [&]() -> void * {
//#ifdef FAKE_JNI_DEBUG
//    auto const vm = Jvm::getCurrentVm();
//    const auto log = (vm ? vm->getLog() : stdout);
//#endif
//    Dl_info info;
//    //look up symbol information and dlopen it
//    try {
//     if (dladdr((void *)method->fnPtr, &info)) {
//      const auto handle = dlopen(info.dli_fname, RTLD_LAZY);
//      if (!handle) {
//#ifdef FAKE_JNI_DEBUG
//       fprintf(log, "WARNING: dlopen failed with message:\n%s\n", dlerror());
//#endif
//       throw -1;
//      }
//      const auto sym = dlsym(handle, info.dli_sname);
//      if (!sym) {
//#ifdef FAKE_JNI_DEBUG
//       fprintf(log, "WARNING: dlsym failed with message:\n%s\n", dlerror());
//#endif
//       throw -1;
//      }
//      return (void *)sym;
//     }
//    } catch (...) {
//#ifdef FAKE_JNI_DEBUG
//     fprintf(
//      log,
//      "WARNING: Could not resolve symbol information for register natives constructed JMethodID:"
//      "\n\t0x%lx -> %s\nUsing provided value instead!\n",
//      (uintptr_t)method->fnPtr,
//      signature
//     );
//#endif
//    }
//    return (void *)method->fnPtr;
//   }()
   (void *)method->fnPtr
  },
  //ReturnType (*fnPtr)(JNIEnv *env, jobject objectOrClass, ...)
  type(REGISTER_NATIVES_FUNC),
  descriptor([&]() -> ffi_cif * {
   auto& pair = descriptors[std::hash<std::string>{}(signature)];
   pair.first += 1;
   auto& descriptor = pair.second;
   if (!descriptor) {
    auto&& args = getFfiPrototype(signature, name);
    const auto argc = (unsigned int)args.size() - 1;
    descriptor = new ffi_cif;
    auto types = new ffi_type*[argc + 2];
    //env type
    types[0] = &ffi_type_pointer;
    //class or obj
    types[1] = &ffi_type_pointer;
    //the actual function arguments (skip env and obj)
    for (unsigned int i = 0; i < argc; i++) {
     types[i + 2] = args[i];
    }
    auto status = ffi_prep_cif(descriptor, FFI_DEFAULT_ABI, argc + 2, args[argc], types);
    if (status != FFI_OK) {
     throw std::runtime_error(
      "FATAL: ffi_prep_cif failed for function: '"
      + std::string(name)
      + signature
      + "' with error: "
      + std::to_string((uint32_t)status)
      + "!"
     );
    }
   }
   return descriptor;
  }()),
  resolvers(new void_func_t[2 * (descriptor->nargs - 2)]),
  deallocators(new void_func_t[descriptor->nargs - 2]),
  isArbitrary(true)
 {
  JMETHODID_INVOCATION_PREP
 }

 JMethodID::JMethodID(arbitrary_func_t func, const char * signature, const char * name, uint32_t modifiers) :
  _jmethodID(),
  JNINativeMethod {
   verifyName(name),
   verifySignature(signature),
   (void *)func
  },
  //ReturnType (*fnPtr)(JNIEnv *env, jobject objectOrClass, ...)
  type(REGISTER_NATIVES_FUNC),
  descriptor([&]() -> ffi_cif * {
   auto& pair = descriptors[std::hash<std::string>{}(signature)];
   pair.first += 1;
   auto& descriptor = pair.second;
   if (!descriptor) {
    auto&& args = getFfiPrototype(signature, name);
    const auto argc = (unsigned int)args.size() - 1;
    descriptor = new ffi_cif;
    auto types = new ffi_type*[2];
    types[0] = &ffi_type_pointer;
    types[1] = &ffi_type_pointer;
    auto status = ffi_prep_cif_var(descriptor, FFI_DEFAULT_ABI, 2, argc + 2, args[argc], types);
    if (status != FFI_OK) {
     throw std::runtime_error(
      "FATAL: ffi_prep_cif_var failed for function: '"
       + std::string(name)
       + signature
       + "' with error: "
       + std::to_string((uint32_t)status)
       + "!"
     );
    }
   }
   return descriptor;
  }()),
  resolvers(new void_func_t[2 * (descriptor->nargs - 2)]),
  deallocators(new void_func_t[descriptor->nargs - 2]),
  isArbitrary(false)
 {
  JMETHODID_INVOCATION_PREP
 }

 //TODO change return value of arbitrary callbakcs to jvalue
 JMethodID::JMethodID(CX::Lambda<jvalue (JNIEnv *, jobject, jvalue *)> func, const char * signature, const char * name, uint32_t modifiers) :
  _jmethodID(),
  JNINativeMethod {
   verifyName(_INTERNAL_ARBITRARY_ALLOC_STR(name)),
   verifySignature(_INTERNAL_ARBITRARY_ALLOC_STR(signature)),
   //segmented functor object
   CX::union_cast<decltype(fnPtr)>(func)
  },
  type(ARBITRARY_STL_FUNC),
  modifiers(modifiers),
  proxyFuncV((void (*)())&_CX::FunctionAccessor<3, CX::Lambda<jvalue (void *, void *, void *)>>::template invokeV<>),
  proxyFuncA((void (*)())&_CX::FunctionAccessor<3, CX::Lambda<jvalue (void *, void *, void *)>>::template invokeA<>),
  isArbitrary(true)
 {}

 bool JMethodID::operator ==(const JMethodID & mid) const noexcept {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->operator==(mid);
   default: return (name == mid.name)
    && (strcmp(getSignature(), mid.getSignature()) == 0)
    && (type == mid.type)
    && (modifiers == mid.modifiers)
    && ((type == MEMBER_FUNC) ? (fnPtr == mid.fnPtr && adj == mid.adj) : (fnPtr == mid.fnPtr))
    && (proxyFuncV == mid.proxyFuncV)
    && (proxyFuncA == mid.proxyFuncA);
  }
 }

 bool JMethodID::operator==(const JNINativeMethod *& mid) const {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->operator==(mid);
   default: return modifiers == 0
    && type == REGISTER_NATIVES_FUNC
    && strcmp(getName(), mid->name) == 0
    && strcmp(getSignature(), mid->signature) == 0
    && fnPtr == mid->fnPtr;
  }
 }

 JMethodID::~JMethodID() {
  //Clean up runtime-generated ffi descriptor and proxy functions
  switch(type) {
   case STL_FUNC: {
    ((CX::Lambda<void ()> *) &fnPtr)->~Lambda();
    break;
   }
   case REGISTER_NATIVES_FUNC: {
    delete[] resolvers;
    delete[] deallocators;
    auto& pair = descriptors[std::hash<std::string>{}(signature)];
    pair.first -= 1;
    if (pair.first == 0) {
     auto& cif = pair.second;
     delete[] cif->arg_types;
     delete cif;
    }
    break;
   }
   case COMPOSED_FUNC: {
    if (dealloc) {
     delete (JMethodID *)fnPtr;
    }
    break;
   }
   default: break;
  }
  if (isArbitrary) {
   delete[] name;
   delete[] signature;
  }
 }

 const char * JMethodID::getName() const noexcept {
  switch(type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->getName();
   default: return name;
  }
 }

 const char * JMethodID::getSignature() const noexcept {
  switch(type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->getSignature();
   default: return signature;
  }
 }

 uint32_t JMethodID::getModifiers() const noexcept {
  switch (type) {
   case REGISTER_NATIVES_FUNC: return 0;
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->getModifiers();
   default: return modifiers;
  }
 }

 const JMethodID * JMethodID::findVirtualMatch(const JClass * clazz) const {
  switch (type) {
   case COMPOSED_FUNC: {
    ((JMethodID *)fnPtr)->findVirtualMatch(clazz);
   }
   default: {
    const auto origClazz = clazz;
    const auto * jobjDescriptor = &*JObject::getDescriptor();
    const auto
     name = getName(),
     signature = getSignature();
    while (clazz != jobjDescriptor) {
     for (auto& method : clazz->getMethods()) {
      if (strcmp(name, method->getName()) == 0) {
       if (strcmp(signature, method->getSignature()) == 0) {
        return method;
       }
      }
     }
     clazz = clazz->parent.get();
    }
    throw std::runtime_error(
     "FATAL: Could not perform virtual function invocation for '"
     + std::string(name)
     + signature
     + "' since no classes in the inheritance hierarchy of '"
     + origClazz->getName()
     + "' register a matching overload!"
    );
   }
  }
 }

 jvalue JMethodID::invoke(const JniEnv& env, const JObject * clazzOrInst, ...) const {
  CX::va_list_t list;
  va_start(list, clazzOrInst);
  return directInvoke<jvalue>(env, (void *)clazzOrInst, list);
 }

 jvalue JMethodID::virtualInvoke(const JniEnv& env, JObject * clazzOrObj, CX::va_list_t& list) const {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->virtualInvoke(env, clazzOrObj, list);
   default: return vInvoke<jvalue>(env, clazzOrObj, list);
  }
 }

 jvalue JMethodID::virtualInvoke(const JniEnv& env, JObject * clazzOrObj, const jvalue * args) const {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->virtualInvoke(env, clazzOrObj, args);
   default: return vInvoke<jvalue>(env, clazzOrObj, args);
  }
 }

 jvalue JMethodID::nonVirtualInvoke(const JniEnv& env, JClass * clazz, JObject * inst, CX::va_list_t& list) const {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->nonVirtualInvoke(env, clazz, inst, list);
   default: return nvInvoke<jvalue>(env, clazz, inst, list);
  }
 }

 jvalue JMethodID::nonVirtualInvoke(const JniEnv& env, JClass * clazz, JObject * inst, const jvalue * args) const {
  switch (type) {
   case COMPOSED_FUNC: return ((JMethodID *)fnPtr)->nonVirtualInvoke(env, clazz, inst, args);
   default: return nvInvoke<jvalue>(env, clazz, inst, args);
  }
 }

 jvalue JMethodID::virtualInvoke(const JniEnv& env, jobject clazzOrObjRef, CX::va_list_t& list) const {
  return virtualInvoke(env, env.resolveReference(clazzOrObjRef).get(), list);
 }

 jvalue JMethodID::virtualInvoke(const JniEnv& env, jobject clazzOrObjRef, const jvalue * args) const {
  return virtualInvoke(env, env.resolveReference(clazzOrObjRef).get(), args);
 }

 jvalue JMethodID::nonVirtualInvoke(const JniEnv& env, jclass clazz, jobject inst, CX::va_list_t& list) const {
  return nonVirtualInvoke(env, (JClass *) env.resolveReference((jobject) clazz).get(), env.resolveReference(inst).get(), list);
 }

 jvalue JMethodID::nonVirtualInvoke(const JniEnv& env, jclass clazz, jobject inst, const jvalue * args) const {
  return nonVirtualInvoke(env, (JClass *) env.resolveReference((jobject) clazz).get(), env.resolveReference(inst).get(), args);
 }

}
