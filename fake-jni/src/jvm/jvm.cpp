#include "fake-jni/internal/util.h"
#include "fake-jni/fake-jni.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <csignal>
#include <cxxabi.h>

#define UNW_LOCAL_ONLY
#include <fake-jni/jvm.h>
#ifdef BUILD_FAKE_JNI_WITH_LIBUNWIND
#include <libunwind.h>
#endif

#define DEFAULT_MANGLED_SYMBOL_NAME_CACHE 4096

#define _UNW_SUCCEED_OR_THROW(name, ...)\
if ((unw_status = name(__VA_ARGS__))) {\
 throw UnwindException("FATAL: " + std::string(#name) + "() failed with error code: " + std::to_string(unw_status));\
}

#define _UNW_SUCCEED_OR_EXIT(name, ...)\
if ((unw_status = name(__VA_ARGS__))) {\
 fprintf(log, "FATAL: %s() failed with error code: %d\n", #name, unw_status);\
 exit(-1);\
}

//Non-template members of Jvm
namespace FakeJni {
 //Class descriptors for all primitives
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JVoid, voidDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JBoolean, booleanDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JByte, byteDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JChar, charDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JShort, shortDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JInt, intDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JFloat, floatDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JLong, longDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR
 BEGIN_NATIVE_PRIMITIVE_DESCRIPTOR(JDouble, doubleDescriptor) END_NATIVE_PRIMITIVE_DESCRIPTOR

 PointerList<Jvm *> Jvm::vms;
 thread_local const Jvm *Jvm::currentVm;

 Jvm::VmThreadContext::VmThreadContext(const FakeJni::Jvm &vm) {
  if (Jvm::currentVm) {
   throw std::runtime_error("FATAL: Tried to overwrite context of a running JVM instance!");
  }
  Jvm::currentVm = &vm;
 }

 Jvm::VmThreadContext::~VmThreadContext() {
  for (auto& hook : destructorHooks) {
   hook();
  }
  Jvm::currentVm = nullptr;
 }

 Jvm::VmThreadContext Jvm::setVmContext() const {
  return {*this};
 }

 Jvm::Jvm(FILE *log) :
  JavaVM(),
  uuid(generateJvmUuid()),
  log(log),
  invoke(new InvokeInterface(*this)),
  jvmti(new JvmtiInterface(*this)),
  jvmtiEnv(new JvmtiEnv(*this)),
  jniEnvFactory([this]() { return std::make_unique<JniEnv>(*this); }),
  globalRefs(128),
  libraries{true}
 {
  functions = invoke;
  vms.insert(this);
  registerDefaultClasses();
 }

 FILE * Jvm::getLog() const {
  return log;
 }

 void Jvm::registerDefaultClasses() {
  registerClass<JObject>();
  registerClass<JClass>();
  registerClass<JString>();
  registerClass<JThrowable>();
  registerClass<JArray<JThrowable>>();
//  registerClass<JWeak>();
  registerClass<JBooleanArray>();
  registerClass<JByteArray>();
  registerClass<JCharArray>();
  registerClass<JShortArray>();
  registerClass<JIntArray>();
  registerClass<JFloatArray>();
  registerClass<JLongArray>();
  registerClass<JDoubleArray>();
  registerClass<JArray<JObject>>();
  registerClass(voidDescriptor);
  registerClass(booleanDescriptor);
  registerClass(byteDescriptor);
  registerClass(shortDescriptor);
  registerClass(intDescriptor);
  registerClass(floatDescriptor);
  registerClass(longDescriptor);
  registerClass(doubleDescriptor);
 }

#define _SIG_SET(_signal, action) \
if (sigaction(_signal, &new_sa, &action) == -1) {\
 throw std::runtime_error("FATAL: Could not set the default Jvm _signal signal handler!");\
}

#define _SIG_CASE(_signal, action) \
case _signal: {\
 if (action.sa_handler) {\
  (*action.sa_handler)(sig);\
 } else if (action.sa_sigaction) {\
  (*action.sa_sigaction)(sig, info, uc);\
 }\
}

 void Jvm::registerDefaultSignalHandler() {
  static struct sigaction
   new_sa,
   old_abrt_sa,
   old_segv_sa,
   old_ill_sa;
  new_sa.sa_flags |= SA_SIGINFO;
  new_sa.sa_sigaction = [](int sig, siginfo_t * info, void * uc) -> void {
   static const auto error = "Received fatal signal: %s\n";
   //vm is a thread_local variable that is only set when Jvm::start() is invoked, and is unset when it returns
   //if vm is not set, then no vm is currently running and the previous signal handler should be used
   const auto vm = Jvm::getCurrentVm();
   if (vm) {
    auto ssig = strsignal(sig);
    char message[strlen(ssig) + (strlen(error) - 2) + 1];
    snprintf(message, sizeof(message), error, ssig);
#ifdef BUILD_FAKE_JNI_WITH_LIBUNWIND
    vm->fatalError(message, (ucontext_t *)uc);
#else
    vm->fatalError(message);
#endif
   } else {
    switch(sig) {
     _SIG_CASE(SIGABRT, old_abrt_sa)
     _SIG_CASE(SIGSEGV, old_segv_sa)
     _SIG_CASE(SIGILL, old_ill_sa)
     default: { /*do nothing*/ }
    }
   }
  };
  sigemptyset(&new_sa.sa_mask);
  _SIG_SET(SIGABRT, old_abrt_sa)
  _SIG_SET(SIGSEGV, old_segv_sa)
  _SIG_SET(SIGILL, old_ill_sa)
 }

 const char * Jvm::generateJvmUuid() noexcept {
  static auto randCharFunc = []() -> char {
   return (char)((rand() % 2 ? 65 : 97) + (rand() % 25));
  };
  auto str = new char[33];
  for (uint32_t i = 0; i < 32; i++) {
   str[i] = randCharFunc();
  }
  //Ensure that the uuid is unique
  bool changed = true;
  auto size = vms.size();
  decltype(size) i = 0;
  while (changed && i < size) {
   changed = false;
   while (vms[i]->uuid == std::string(str)) {
    str[rand() % 31] = randCharFunc();
    changed = true;
   }
   i = (changed ? 0 : i + 1);
  }
  str[32] = '\0';
  return str;
 }

 const Jvm * Jvm::getCurrentVm() noexcept {
  return currentVm;
 }

 inline void Jvm::setInvokeInterface(InvokeInterface * const ii) {
  delete invoke;
  invoke = ii;
  functions = ii;
 }

 inline InvokeInterface& Jvm::getInvokeInterface() const {
  return *invoke;
 }

 inline void Jvm::setJvmtiInterface(JvmtiInterface * const ji) {
  delete jvmti;
  jvmti = ji;
  jvmtiEnv->functions = ji;
 }

 inline JvmtiInterface& Jvm::getJvmtiInterface() const {
  return *jvmti;
 }

 inline std::unique_ptr<JniEnv> Jvm::createJniEnv() const {
  return jniEnvFactory();
 }

 inline void Jvm::setJvmtiEnv(JvmtiEnv * const env) {
  auto interface = (JvmtiInterface *)env->functions;
  delete jvmtiEnv;
  jvmtiEnv = env;
  jvmtiEnv->setJvmtiInterface(*interface);
 }

 inline JvmtiEnv& Jvm::getJvmtiEnv() const {
  return *jvmtiEnv;
 }

 bool Jvm::isRunning() const {
  return running;
 }

 jobject Jvm::createGlobalReference(std::shared_ptr<FakeJni::JObject> object) {
  auto index = globalRefs.createReference(std::move(object));
  return JniReferenceDescription(index, true).ptr;
 }

 void Jvm::deleteGlobalReference(jobject reference) {
  auto desc = JniReferenceDescription(reference).desc;
  if (!desc.isGlobal)
   throw std::runtime_error("FATAL: Reference is a local reference");
  globalRefs.deleteReference(desc.index);
 }

 JniReferenceTable& Jvm::getGlobalReferenceTable() {
  return globalRefs;
 }

 Jvm::~Jvm() {
  vms.erase(this);
  for (auto library : libraries) {
   removeLibrary(library, "");
  }
  delete jvmtiEnv;
  delete jvmti;
  delete invoke;
  delete[] uuid;
 }

 bool Jvm::registerClass(std::shared_ptr<const JClass> clazz) {
  std::lock_guard<std::shared_mutex> lock (classes_mutex);
  auto found = classes.find(clazz->getName());
  if (found != classes.end()) {
#ifdef FAKE_JNI_DEBUG
   fprintf(
    log,
    "WARNING: Class '%s' is already registered on the JVM instance '%s'!\n",
    clazz->getName(),
    uuid
   );
#endif
   return false;
  } else {
   classes.insert({std::string(clazz->getName()), clazz});
  }
  return true;
 }

 bool Jvm::unregisterClass(const JClass * clazz) {
  std::lock_guard<std::shared_mutex> lock (classes_mutex);
  auto found = classes.find(clazz->getName());
  if (found != classes.end() && found->second.get() == clazz) {
   classes.erase(found);
   return true;
  } else {
#ifdef FAKE_JNI_DEBUG
   fprintf(
    log,
    "WARNING: Class '%s' is not registered on the JVM instance '%s'!\n",
    clazz->getName(),
    uuid
   );
#endif
   return false;
  }
 }

 std::shared_ptr<const JClass> Jvm::findClass(const char * name) const {
  std::shared_lock<std::shared_mutex> lock (classes_mutex);
  auto found = classes.find(name);
  if (found != classes.end())
    return found->second;
  return nullptr;
 }

 void Jvm::attachLibrary(
  const std::string & rpath,
  const std::string & options,
  LibraryOptions loptions
 ) {
  std::scoped_lock libraryLock(library_mutex);
  [[maybe_unused]]
  const auto&& context = setVmContext();
  std::string path = rpath.empty() ? "(embedded)" : rpath;
  bool libraryExists = false;
  for (auto lib : libraries) {
   if (lib->path == path) {
    libraryExists = true;
    break;
   }
  }
  if (!libraryExists) {
   auto library = new Library(const_cast<Jvm&>(*this), path, loptions);
#ifdef FAKE_JNI_DEBUG
   fprintf(log, "DEBUG: Created library: '%s'\n", path.c_str());
#endif
   if (library->jniBound()) {
    if (!library->jniLoad()) {
     //set Jvm::currentVm to nullptr before throwing an exception, so the Jvm signal handler does not  process this
     currentVm = nullptr;
     throw std::runtime_error("FATAL: Error initializing JNI library: '" + path + "'");
    }
   }
   //Only one startup agent function is called per library instance
   if (library->agentBound()) {
    const auto agentInitializer = (running ? &Library::agentAttach : &Library::agentLoad);
    if ((library->*agentInitializer)(const_cast<char *>(options.c_str()))) {
     currentVm = nullptr;
     //set Jvm::currentVm to nullptr before throwing an exception, so the Jvm signal handler does not  process this
     throw std::runtime_error("FATAL: Error initializing agent library: '" + path + "'");
    }
   }
   libraries.insert(library);
#ifdef FAKE_JNI_DEBUG
   fprintf(log, "DEBUG: Registered library: '%s'\n", path.c_str());
#endif
  }
#ifdef FAKE_JNI_DEBUG
  else {
   fprintf(log, "WARNING: Library '%s' is already registered on this DefaultJvm instance!\n", path.c_str());
  }
#endif
 }

 //Removes a library from the DefaultJvm instance
 //Implicitly unloads the agent within the library, if there was one
 //Implicitly unloads the jni component within the library, if there was one
 bool Jvm::removeLibrary(const std::string & path, const std::string & options) {
  for (auto library : libraries) {
   if (library->path == path) {
    return removeLibrary(library, options);
   }
  }
  fprintf(log, "WARNING: Library '%s' was not registered on this Jvm instance!\n", path.c_str());
  return false;
 }

 bool Jvm::removeLibrary(const Library * library, const std::string & options) {
  std::scoped_lock libraryLock(library_mutex);
  [[maybe_unused]]
  const auto&& context = setVmContext();
#ifdef FAKE_JNI_DEBUG
  fprintf(log, "DEBUG: Removing library: '%s'\n", library->path.c_str());
#endif
  if (library->jniBound()) {
   library->jniUnload();
  }
  if (library->agentBound()) {
   library->agentUnload(const_cast<char *>(options.c_str()));
  }
  libraries.erase(library);
  return true;
 }

 const PointerList<const Library *>& Jvm::getLibraries() const {
  return libraries;
 }

 void Jvm::start() {
  JArray<JString> args{};
  start(&args);
 }

 void Jvm::start(const JObject * oArgs) {
  if (oArgs) {
   if (strcmp(oArgs->getClass().getName(), JArray<JString>::getDescriptor()->getName())) {
    throw std::runtime_error("FATAL: JVM entry point only accepts an array of stings!");
   }
  } else {
   throw std::runtime_error("FATAL: You must provide a non-null array of arguments for the JVM entry point!");
  }
  auto args = (JArray<JString> *)oArgs;
  [[maybe_unused]]
  const auto&& context = setVmContext();
  if (running) {
   throw std::runtime_error("FATAL: Tried to start JVM instance twice!");
  }

  context.destructorHooks.emplace_back([&]() { running = false; });
//  currentVm = this;
  running = true;
  try {
   std::shared_lock<std::shared_mutex> lock (classes_mutex);
   const JClass * encapsulatingClass = nullptr;
   const JMethodID * main = nullptr;
   for (auto& clazz : classes) {
    for (auto& mid : clazz.second->getMethods()) {
     if (strcmp(mid->getName(), "main") == 0) {
      if (strcmp(mid->getSignature(), "([java/lang/String;)V") == 0) {
       encapsulatingClass = clazz.second.get();
       main = mid;
       break;
      }
     }
    }
    if (main) {
     break;
    }
   }
   if (!main) {
    currentVm = nullptr;
    throw std::runtime_error("No classes define the default Java entry point: 'main([Ljava/lang/String;)V'!");
   }
   LocalFrame frame (*this);
   main->invoke(frame.getJniEnv(), encapsulatingClass, args);
  } catch(const std::exception &ex) {
   //TODO put the Jvm into an errored state so the user can handle the error
   fprintf(log, "FATAL: VM encountered an uncaught exception with message:\n%s\n", ex.what());
   exit(-1);
  } catch(...) {
   //TODO put the Jvm into an errored state so the user can handle the error
   fprintf(log, "FATAL: VM encountered an unknown fatal error!\n");
   exit(-1);
  }
 }

 //TODO
 JInt Jvm::destroy() {
//  running = false;
  return JNI_OK;
 }

 void Jvm::throwException(std::shared_ptr<JThrowable> throwable) {
  clearException();
  exception = std::move(throwable);
 }

 std::shared_ptr<JThrowable> Jvm::getException() const {
  return exception;
 }

 void Jvm::clearException() {
  exception.reset();
 }

 void Jvm::fatalError(const char * message) const {
#ifdef BUILD_FAKE_JNI_WITH_LIBUNWIND
  fatalError(message, nullptr);
#else
  printf("FATAL: Fatal error thrown on Jvm instance '%s' with message: \n%s\n\n", uuid, message);
#endif
 }

#ifdef BUILD_FAKE_JNI_WITH_LIBUNWIND
 void Jvm::fatalError(const char * message, ucontext_t * context) const {
  printf("FATAL: Fatal error thrown on Jvm instance '%s' with message: \n%s\n\n", uuid, message);
  try {
   if (context) {
    printBacktrace(context);
   } else {
    printBacktrace();
   }
  } catch (UnwindException &e) {
   fprintf(log, "FATAL: Encountered exception unwinding stack:\n%s\n", e.what());
   exit(-1);
  } catch (...) {
   fprintf(log, "FATAL: Encountered unexpected exception unwinding stack!\n");
   exit(-1);
  }
  //If vm execution has not entered through Jvm::start(), Jvm::attachLibrary or Jvm::removeLibrary
  if (!Jvm::currentVm) {
   fprintf(log, "FATAL: JVM execution started through unsupported entry point!\n");
   abort();
  }
  unw_context_t uc;
  unw_cursor_t cursor;
  unw_word_t off;
  int
   unw_status,
   demangle_status = -1,
   symbol_size = DEFAULT_MANGLED_SYMBOL_NAME_CACHE - 1024;
  bool returnFrameFound = false;

  //TODO put the Jvm into an errored state so the user can handle the error
  //find FakeJni::Jvm::start() and continue execution at that frame
  //initialize frame to the current frame for local unwinding
  if (context) {
   _UNW_SUCCEED_OR_EXIT(unw_init_local, &cursor, context)
  } else {
   _UNW_SUCCEED_OR_EXIT(unw_getcontext, &uc)
   _UNW_SUCCEED_OR_EXIT(unw_init_local, &cursor, &uc)
  }
  //unwind each frame up the stack
  while ((unw_status = unw_step(&cursor)) > 0) {
   //resolve mangled symbol name
   //will continue until the buffer is large enough to contain it
   do {
    char mangled[(symbol_size += 1024)];
    unw_status = unw_get_proc_name(&cursor, mangled, sizeof(mangled), &off);
   } while (unw_status == UNW_ENOMEM);
   if (unw_status) {
    fprintf(log, "FATAL: unw_get_proc_name() failed with error code: %d\n", unw_status);
    exit(-1);
   }
   char mangled[symbol_size];
   _UNW_SUCCEED_OR_EXIT(unw_get_proc_name, &cursor, mangled, sizeof(mangled), &off)
   char *sym = abi::__cxa_demangle(mangled, nullptr, nullptr, &demangle_status);
   if (demangle_status == -2) {
    free(sym);
    sym = mangled;
   } else if (demangle_status) {
    free(sym);
#ifdef FAKE_JNI_DEBUG
    fprintf(log, "WARNING: Could not demangle symbol: %s\n", mangled);
#endif
    continue;
   }
   if (running) {
    if (strcmp(sym, "FakeJni::Jvm::start()") == 0) {
     returnFrameFound = true;
    }
   } else {
    if (((uintptr_t)strstr(sym, "FakeJni::Jvm::attachLibrary") | (uintptr_t)strstr(sym, "FakeJni::Jvm::removeLibrary")) != 0) {
     returnFrameFound = true;
    }
   }
   if (!demangle_status) {
    free(sym);
   }
   if (returnFrameFound) {
    break;
   }
  }
  if (unw_status < 0) {
   fprintf(log, "FATAL: unw_step() failed with error code: %d\n", unw_status);
   exit(-1);
  }
  if (!returnFrameFound) {
   if (running) {
    fprintf(log, "FATAL: FakeJni::Jvm::start() entry point was not found on the stack!\n");
   } else {
    fprintf(
     log,
     "FATAL: Neither FakeJni::Jvm::attachLibrary() nor FakeJni::Jvm::removeLibrary() entry points were not "
     "found on the stack!\n"
    );
   }
   exit(-1);
  }
  //if resume is successful this code will never be reached
  unw_status = unw_resume(&cursor);
  fprintf(log, "FATAL: unw_resume() failed with error code: %d\n", unw_status);
  exit(-1);
 }

 void Jvm::printBacktrace(ucontext_t * context) const {
  printf("Backtrace: #STACK_FRAME STACK_POINTER: (SYMBOL_NAME+OFFSET) [INSTRUCTION_POINTER] in SYMBOL_SOURCE\n");
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp, off;
  unw_proc_info_t pip;
  int
   unw_status,
   demangle_status = -1,
   frame_number = 0,
   symbol_size = DEFAULT_MANGLED_SYMBOL_NAME_CACHE - 1024;
  bool demangleAttempted = false;
  Dl_info dlinfo;
  const char
   *sym,
   *obj_file,
   *demangled;
  if (context) {
   _UNW_SUCCEED_OR_THROW(unw_init_local, &cursor, context)
  } else {
   _UNW_SUCCEED_OR_THROW(unw_getcontext, &uc)
   _UNW_SUCCEED_OR_THROW(unw_init_local, &cursor, &uc)
  }
  while ((unw_status = unw_step(&cursor)) > 0) {
   pip.unwind_info = nullptr;
   //get process info
   _UNW_SUCCEED_OR_THROW(unw_get_proc_info, &cursor, &pip)
   //TODO maybe we should log this?
//  printf();
   //get instruction and stack pointers
   _UNW_SUCCEED_OR_THROW(unw_get_reg, &cursor, UNW_REG_IP, &ip)
   _UNW_SUCCEED_OR_THROW(unw_get_reg, &cursor, UNW_REG_SP, &sp)
   //resolve mangled symbol name
   //will continue until the buffer is large enough to contain it
   do {
    char mangled[(symbol_size += 1024)];
    unw_status = unw_get_proc_name(&cursor, mangled, sizeof(mangled), &off);
   } while (unw_status == UNW_ENOMEM);
   if (unw_status) {
    throw UnwindException("FATAL: unw_get_proc_name() failed with error code: " + std::to_string(unw_status));
   }
   //get symbol name and demangle if possible
   char mangled[symbol_size];
   sym = mangled;
   if (unw_get_proc_name(&cursor, mangled, sizeof(mangled), &off)) {
    sym = "[stripped]";
   } else {
    demangleAttempted = true;
    demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &demangle_status);
    if (!demangle_status) {
     sym = demangled;
    }
   }
   //resolve object file name
   if (dladdr((void *)(pip.start_ip + off), &dlinfo) && dlinfo.dli_fname && *dlinfo.dli_fname) {
    obj_file = dlinfo.dli_fname;
   } else {
    obj_file = "[stripped]";
   }
   //#frame_num stack_ptr, (symbol_name+offset) [instruction_ptr] in object_file
   fprintf(
    log,
    "#%2d 0x%lx: (%s+0x%lx) [0x%lx] in %s\n",
    frame_number,
    (intptr_t)sp,
    sym,
    (intptr_t)off,
    (intptr_t)ip,
    obj_file
   );
   //if the name was demangled, free the allocated string
   if (demangleAttempted) {
    free((void *)demangled);
   }
   frame_number += 1;
  }
  if (unw_status != 0) {
   throw UnwindException("unw_step() failed with error code: " + std::to_string(unw_status));
  }
 }
#endif

 jobject _CX::createLocalReturnReference(const JniEnv& env, std::shared_ptr<JObject> ptr) {
  return const_cast<JniEnv &>(env).createLocalReference(ptr);
 }

 std::shared_ptr<JObject> _CX::resolveArgumentReference(const JniEnv& env, jobject object) {
  return env.resolveReference(object);
 }

} 
