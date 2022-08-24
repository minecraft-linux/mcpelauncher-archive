#pragma once

#include <fake-jni/jvm.h>

#include <map>
#include <set>
#include <utility>

namespace Baron {
 class Jvm : public FakeJni::Jvm {
 public:
  std::set<std::string> blacklistedClasses;
  //globally blacklisted class properties will be associated with an empty string
  std::map<std::string, std::set<std::string>> blacklistedFields;
  std::map<std::string, std::set<std::string>> blacklistedMethods;

  std::set<std::shared_ptr<FakeJni::JObject>> fabricatedInstancesHolder;
  std::set<const FakeJni::JObject *> fabricatedInstances;
  //TODO provide backtrace of accesses and values at destruction
  std::map<const std::pair<const FakeJni::JObject *, const FakeJni::JFieldID *>, jvalue> fabricatedValues;
  std::map<const FakeJni::JClass *, std::set<const FakeJni::JObject *>> fabricatedClassMappings;

  explicit Jvm(FILE * log = stdout);

  //FakeJni::Jvm overrides
  virtual std::shared_ptr<const FakeJni::JClass> findClass(const char * name) const override;
  virtual jint destroy() override;

  //baron specific
  virtual bool isClassBlacklisted(const char * name) const;
  virtual bool isMethodBlacklisted(const char * name, const char * sig, const char * clazz = "") const;
  virtual bool isFieldBlacklisted(const char * name, const char * sig, const char * clazz = "") const;
  //Blacklist class from fabrication
  virtual void blacklistClass(const char * name);
  //Blacklist field in 'clazz' from fabrication
  virtual void blacklistField(const char * name, const char * sig, const char * clazz = "");
  //Blacklist method in 'clazz' from fabrication
  virtual void blacklistMethod(const char * name, const char * sig, const char * clazz = "");
  virtual std::shared_ptr<FakeJni::JObject> fabricateInstance(const FakeJni::JClass * jclazz) const;
  virtual jvalue fabricateValue(FakeJni::JniEnv &env, const FakeJni::JClass * clazz) const;
  virtual bool isFabricated(const FakeJni::JObject * jobj) const noexcept;
 };
}