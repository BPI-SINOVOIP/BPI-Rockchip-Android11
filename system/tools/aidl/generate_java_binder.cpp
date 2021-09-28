/*
 * Copyright (C) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aidl.h"
#include "aidl_to_java.h"
#include "generate_java.h"
#include "logging.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include <android-base/macros.h>
#include <android-base/stringprintf.h>

using android::base::Join;
using android::base::StringPrintf;

using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace aidl {
namespace java {

// =================================================
class VariableFactory {
 public:
  using Variable = ::android::aidl::java::Variable;

  explicit VariableFactory(const std::string& base) : base_(base), index_(0) {}
  std::shared_ptr<Variable> Get(const AidlTypeSpecifier& type, const AidlTypenames& typenames) {
    auto v = std::make_shared<Variable>(JavaSignatureOf(type, typenames),
                                        StringPrintf("%s%d", base_.c_str(), index_));
    vars_.push_back(v);
    index_++;
    return v;
  }

  std::shared_ptr<Variable> Get(int index) { return vars_[index]; }

 private:
  std::vector<std::shared_ptr<Variable>> vars_;
  std::string base_;
  int index_;

  DISALLOW_COPY_AND_ASSIGN(VariableFactory);
};

// =================================================
class StubClass : public Class {
 public:
  StubClass(const AidlInterface* interfaceType, const Options& options);
  ~StubClass() override = default;

  std::shared_ptr<Variable> transact_code;
  std::shared_ptr<Variable> transact_data;
  std::shared_ptr<Variable> transact_reply;
  std::shared_ptr<Variable> transact_flags;
  std::shared_ptr<SwitchStatement> transact_switch;
  std::shared_ptr<StatementBlock> transact_statements;
  std::shared_ptr<SwitchStatement> code_to_method_name_switch;

  // Where onTransact cases should be generated as separate methods.
  bool transact_outline;
  // Specific methods that should be outlined when transact_outline is true.
  std::unordered_set<const AidlMethod*> outline_methods;
  // Number of all methods.
  size_t all_method_count;

  // Finish generation. This will add a default case to the switch.
  void finish();

  std::shared_ptr<Expression> get_transact_descriptor(const AidlMethod* method);

 private:
  void make_as_interface(const AidlInterface* interfaceType);

  std::shared_ptr<Variable> transact_descriptor;
  const Options& options_;

  DISALLOW_COPY_AND_ASSIGN(StubClass);
};

StubClass::StubClass(const AidlInterface* interfaceType, const Options& options)
    : Class(), options_(options) {
  transact_descriptor = nullptr;
  transact_outline = false;
  all_method_count = 0;  // Will be set when outlining may be enabled.

  this->comment = "/** Local-side IPC implementation stub class. */";
  this->modifiers = PUBLIC | ABSTRACT | STATIC;
  this->what = Class::CLASS;
  this->type = interfaceType->GetCanonicalName() + ".Stub";
  this->extends = "android.os.Binder";
  this->interfaces.push_back(interfaceType->GetCanonicalName());

  // descriptor
  auto descriptor = std::make_shared<Field>(
      STATIC | FINAL | PRIVATE, std::make_shared<Variable>("java.lang.String", "DESCRIPTOR"));
  if (options.IsStructured()) {
    // mangle the interface name at build time and demangle it at runtime, to avoid
    // being renamed by jarjar. See b/153843174
    std::string name = interfaceType->GetCanonicalName();
    std::replace(name.begin(), name.end(), '.', '$');
    descriptor->value = "\"" + name + "\".replace('$', '.')";
  } else {
    descriptor->value = "\"" + interfaceType->GetCanonicalName() + "\"";
  }
  this->elements.push_back(descriptor);

  // ctor
  auto ctor = std::make_shared<Method>();
  ctor->modifiers = PUBLIC;
  ctor->comment =
      "/** Construct the stub at attach it to the "
      "interface. */";
  ctor->name = "Stub";
  ctor->statements = std::make_shared<StatementBlock>();
  if (interfaceType->IsVintfStability()) {
    auto stability = std::make_shared<LiteralStatement>("this.markVintfStability();\n");
    ctor->statements->Add(stability);
  }
  auto attach = std::make_shared<MethodCall>(
      THIS_VALUE, "attachInterface",
      std::vector<std::shared_ptr<Expression>>{THIS_VALUE,
                                               std::make_shared<LiteralExpression>("DESCRIPTOR")});
  ctor->statements->Add(attach);
  this->elements.push_back(ctor);

  // asInterface
  make_as_interface(interfaceType);

  // asBinder
  auto asBinder = std::make_shared<Method>();
  asBinder->modifiers = PUBLIC | OVERRIDE;
  asBinder->returnType = "android.os.IBinder";
  asBinder->name = "asBinder";
  asBinder->statements = std::make_shared<StatementBlock>();
  asBinder->statements->Add(std::make_shared<ReturnStatement>(THIS_VALUE));
  this->elements.push_back(asBinder);

  if (options_.GenTransactionNames()) {
    // getDefaultTransactionName
    auto getDefaultTransactionName = std::make_shared<Method>();
    getDefaultTransactionName->comment = "/** @hide */";
    getDefaultTransactionName->modifiers = PUBLIC | STATIC;
    getDefaultTransactionName->returnType = "java.lang.String";
    getDefaultTransactionName->name = "getDefaultTransactionName";
    auto code = std::make_shared<Variable>("int", "transactionCode");
    getDefaultTransactionName->parameters.push_back(code);
    getDefaultTransactionName->statements = std::make_shared<StatementBlock>();
    this->code_to_method_name_switch = std::make_shared<SwitchStatement>(code);
    getDefaultTransactionName->statements->Add(this->code_to_method_name_switch);
    this->elements.push_back(getDefaultTransactionName);

    // getTransactionName
    auto getTransactionName = std::make_shared<Method>();
    getTransactionName->comment = "/** @hide */";
    getTransactionName->modifiers = PUBLIC;
    getTransactionName->returnType = "java.lang.String";
    getTransactionName->name = "getTransactionName";
    auto code2 = std::make_shared<Variable>("int", "transactionCode");
    getTransactionName->parameters.push_back(code2);
    getTransactionName->statements = std::make_shared<StatementBlock>();
    getTransactionName->statements->Add(std::make_shared<ReturnStatement>(
        std::make_shared<MethodCall>(THIS_VALUE, "getDefaultTransactionName",
                                     std::vector<std::shared_ptr<Expression>>{code2})));
    this->elements.push_back(getTransactionName);
  }

  // onTransact
  this->transact_code = std::make_shared<Variable>("int", "code");
  this->transact_data = std::make_shared<Variable>("android.os.Parcel", "data");
  this->transact_reply = std::make_shared<Variable>("android.os.Parcel", "reply");
  this->transact_flags = std::make_shared<Variable>("int", "flags");
  auto onTransact = std::make_shared<Method>();
  onTransact->modifiers = PUBLIC | OVERRIDE;
  onTransact->returnType = "boolean";
  onTransact->name = "onTransact";
  onTransact->parameters.push_back(this->transact_code);
  onTransact->parameters.push_back(this->transact_data);
  onTransact->parameters.push_back(this->transact_reply);
  onTransact->parameters.push_back(this->transact_flags);
  onTransact->statements = std::make_shared<StatementBlock>();
  transact_statements = onTransact->statements;
  onTransact->exceptions.push_back("android.os.RemoteException");
  this->elements.push_back(onTransact);
  this->transact_switch = std::make_shared<SwitchStatement>(this->transact_code);
}

void StubClass::finish() {
  auto default_case = std::make_shared<Case>();

  auto superCall = std::make_shared<MethodCall>(
      SUPER_VALUE, "onTransact",
      std::vector<std::shared_ptr<Expression>>{this->transact_code, this->transact_data,
                                               this->transact_reply, this->transact_flags});
  default_case->statements->Add(std::make_shared<ReturnStatement>(superCall));
  transact_switch->cases.push_back(default_case);

  transact_statements->Add(this->transact_switch);

  // getTransactionName
  if (options_.GenTransactionNames()) {
    // Some transaction codes are common, e.g. INTERFACE_TRANSACTION or DUMP_TRANSACTION.
    // Common transaction codes will not be resolved to a string by getTransactionName. The method
    // will return NULL in this case.
    auto code_switch_default_case = std::make_shared<Case>();
    code_switch_default_case->statements->Add(std::make_shared<ReturnStatement>(NULL_VALUE));
    this->code_to_method_name_switch->cases.push_back(code_switch_default_case);
  }
}

// The the expression for the interface's descriptor to be used when
// generating code for the given method. Null is acceptable for method
// and stands for synthetic cases.
std::shared_ptr<Expression> StubClass::get_transact_descriptor(const AidlMethod* method) {
  if (transact_outline) {
    if (method != nullptr) {
      // When outlining, each outlined method needs its own literal.
      if (outline_methods.count(method) != 0) {
        return std::make_shared<LiteralExpression>("DESCRIPTOR");
      }
    } else {
      // Synthetic case. A small number is assumed. Use its own descriptor
      // if there are only synthetic cases.
      if (outline_methods.size() == all_method_count) {
        return std::make_shared<LiteralExpression>("DESCRIPTOR");
      }
    }
  }

  // When not outlining, store the descriptor literal into a local variable, in
  // an effort to save const-string instructions in each switch case.
  if (transact_descriptor == nullptr) {
    transact_descriptor = std::make_shared<Variable>("java.lang.String", "descriptor");
    transact_statements->Add(std::make_shared<VariableDeclaration>(
        transact_descriptor, std::make_shared<LiteralExpression>("DESCRIPTOR")));
  }
  return transact_descriptor;
}

void StubClass::make_as_interface(const AidlInterface* interfaceType) {
  auto obj = std::make_shared<Variable>("android.os.IBinder", "obj");

  auto m = std::make_shared<Method>();
  m->comment = "/**\n * Cast an IBinder object into an ";
  m->comment += interfaceType->GetCanonicalName();
  m->comment += " interface,\n";
  m->comment += " * generating a proxy if needed.\n */";
  m->modifiers = PUBLIC | STATIC;
  m->returnType = interfaceType->GetCanonicalName();
  m->name = "asInterface";
  m->parameters.push_back(obj);
  m->statements = std::make_shared<StatementBlock>();

  auto ifstatement = std::make_shared<IfStatement>();
  ifstatement->expression = std::make_shared<Comparison>(obj, "==", NULL_VALUE);
  ifstatement->statements = std::make_shared<StatementBlock>();
  ifstatement->statements->Add(std::make_shared<ReturnStatement>(NULL_VALUE));
  m->statements->Add(ifstatement);

  // IInterface iin = obj.queryLocalInterface(DESCRIPTOR)
  auto queryLocalInterface = std::make_shared<MethodCall>(obj, "queryLocalInterface");
  queryLocalInterface->arguments.push_back(std::make_shared<LiteralExpression>("DESCRIPTOR"));
  auto iin = std::make_shared<Variable>("android.os.IInterface", "iin");
  auto iinVd = std::make_shared<VariableDeclaration>(iin, queryLocalInterface);
  m->statements->Add(iinVd);

  // Ensure the instance type of the local object is as expected.
  // One scenario where this is needed is if another package (with a
  // different class loader) runs in the same process as the service.

  // if (iin != null && iin instanceof <interfaceType>) return (<interfaceType>)
  // iin;
  auto iinNotNull = std::make_shared<Comparison>(iin, "!=", NULL_VALUE);
  auto instOfCheck = std::make_shared<Comparison>(
      iin, " instanceof ", std::make_shared<LiteralExpression>(interfaceType->GetCanonicalName()));
  auto instOfStatement = std::make_shared<IfStatement>();
  instOfStatement->expression = std::make_shared<Comparison>(iinNotNull, "&&", instOfCheck);
  instOfStatement->statements = std::make_shared<StatementBlock>();
  instOfStatement->statements->Add(std::make_shared<ReturnStatement>(
      std::make_shared<Cast>(interfaceType->GetCanonicalName(), iin)));
  m->statements->Add(instOfStatement);

  auto ne = std::make_shared<NewExpression>(interfaceType->GetCanonicalName() + ".Stub.Proxy");
  ne->arguments.push_back(obj);
  m->statements->Add(std::make_shared<ReturnStatement>(ne));

  this->elements.push_back(m);
}

// =================================================
class ProxyClass : public Class {
 public:
  ProxyClass(const AidlInterface* interfaceType, const Options& options);
  ~ProxyClass() override;

  std::shared_ptr<Variable> mRemote;
};

ProxyClass::ProxyClass(const AidlInterface* interfaceType, const Options& options) : Class() {
  this->modifiers = PRIVATE | STATIC;
  this->what = Class::CLASS;
  this->type = interfaceType->GetCanonicalName() + ".Stub.Proxy";
  this->interfaces.push_back(interfaceType->GetCanonicalName());

  // IBinder mRemote
  mRemote = std::make_shared<Variable>("android.os.IBinder", "mRemote");
  this->elements.push_back(std::make_shared<Field>(PRIVATE, mRemote));

  // Proxy()
  auto remote = std::make_shared<Variable>("android.os.IBinder", "remote");
  auto ctor = std::make_shared<Method>();
  ctor->name = "Proxy";
  ctor->statements = std::make_shared<StatementBlock>();
  ctor->parameters.push_back(remote);
  ctor->statements->Add(std::make_shared<Assignment>(mRemote, remote));
  this->elements.push_back(ctor);

  if (options.Version() > 0) {
    std::ostringstream code;
    code << "private int mCachedVersion = -1;\n";
    this->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
  }
  if (!options.Hash().empty()) {
    std::ostringstream code;
    code << "private String mCachedHash = \"-1\";\n";
    this->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
  }

  // IBinder asBinder()
  auto asBinder = std::make_shared<Method>();
  asBinder->modifiers = PUBLIC | OVERRIDE;
  asBinder->returnType = "android.os.IBinder";
  asBinder->name = "asBinder";
  asBinder->statements = std::make_shared<StatementBlock>();
  asBinder->statements->Add(std::make_shared<ReturnStatement>(mRemote));
  this->elements.push_back(asBinder);
}

ProxyClass::~ProxyClass() {}

// =================================================
static void generate_new_array(const AidlTypeSpecifier& type, const AidlTypenames& typenames,
                               std::shared_ptr<StatementBlock> addTo, std::shared_ptr<Variable> v,
                               std::shared_ptr<Variable> parcel) {
  auto len = std::make_shared<Variable>("int", v->name + "_length");
  addTo->Add(
      std::make_shared<VariableDeclaration>(len, std::make_shared<MethodCall>(parcel, "readInt")));
  auto lencheck = std::make_shared<IfStatement>();
  lencheck->expression =
      std::make_shared<Comparison>(len, "<", std::make_shared<LiteralExpression>("0"));
  lencheck->statements->Add(std::make_shared<Assignment>(v, NULL_VALUE));
  lencheck->elseif = std::make_shared<IfStatement>();
  lencheck->elseif->statements->Add(std::make_shared<Assignment>(
      v, std::make_shared<NewArrayExpression>(InstantiableJavaSignatureOf(type, typenames), len)));
  addTo->Add(lencheck);
}

static void generate_write_to_parcel(const AidlTypeSpecifier& type,
                                     std::shared_ptr<StatementBlock> addTo,
                                     std::shared_ptr<Variable> v, std::shared_ptr<Variable> parcel,
                                     bool is_return_value, const AidlTypenames& typenames) {
  string code;
  CodeWriterPtr writer = CodeWriter::ForString(&code);
  CodeGeneratorContext context{
      .writer = *(writer.get()),
      .typenames = typenames,
      .type = type,
      .parcel = parcel->name,
      .var = v->name,
      .is_return_value = is_return_value,
  };
  WriteToParcelFor(context);
  writer->Close();
  addTo->Add(std::make_shared<LiteralStatement>(code));
}

static void generate_int_constant(Class* interface, const std::string& name,
                                  const std::string& value) {
  auto code = StringPrintf("public static final int %s = %s;\n", name.c_str(), value.c_str());
  interface->elements.push_back(std::make_shared<LiteralClassElement>(code));
}

static void generate_string_constant(Class* interface, const std::string& name,
                                     const std::string& value) {
  auto code = StringPrintf("public static final String %s = %s;\n", name.c_str(), value.c_str());
  interface->elements.push_back(std::make_shared<LiteralClassElement>(code));
}

static std::shared_ptr<Method> generate_interface_method(const AidlMethod& method,
                                                         const AidlTypenames& typenames) {
  auto decl = std::make_shared<Method>();
  decl->comment = method.GetComments();
  decl->modifiers = PUBLIC;
  decl->returnType = JavaSignatureOf(method.GetType(), typenames);
  decl->name = method.GetName();
  decl->annotations = generate_java_annotations(method.GetType());

  for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
    decl->parameters.push_back(
        std::make_shared<Variable>(JavaSignatureOf(arg->GetType(), typenames), arg->GetName()));
  }

  decl->exceptions.push_back("android.os.RemoteException");

  return decl;
}

static void generate_stub_code(const AidlInterface& iface, const AidlMethod& method, bool oneway,
                               std::shared_ptr<Variable> transact_data,
                               std::shared_ptr<Variable> transact_reply,
                               const AidlTypenames& typenames,
                               std::shared_ptr<StatementBlock> statements,
                               std::shared_ptr<StubClass> stubClass, const Options& options) {
  std::shared_ptr<TryStatement> tryStatement;
  std::shared_ptr<FinallyStatement> finallyStatement;
  auto realCall = std::make_shared<MethodCall>(THIS_VALUE, method.GetName());

  // interface token validation is the very first thing we do
  statements->Add(std::make_shared<MethodCall>(
      transact_data, "enforceInterface",
      std::vector<std::shared_ptr<Expression>>{stubClass->get_transact_descriptor(&method)}));

  // args
  VariableFactory stubArgs("_arg");
  {
    // keep this across different args in order to create the classloader
    // at most once.
    bool is_classloader_created = false;
    for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
      std::shared_ptr<Variable> v = stubArgs.Get(arg->GetType(), typenames);

      statements->Add(std::make_shared<VariableDeclaration>(v));

      if (arg->GetDirection() & AidlArgument::IN_DIR) {
        string code;
        CodeWriterPtr writer = CodeWriter::ForString(&code);
        CodeGeneratorContext context{.writer = *(writer.get()),
                                     .typenames = typenames,
                                     .type = arg->GetType(),
                                     .parcel = transact_data->name,
                                     .var = v->name,
                                     .is_classloader_created = &is_classloader_created};
        CreateFromParcelFor(context);
        writer->Close();
        statements->Add(std::make_shared<LiteralStatement>(code));
      } else {
        if (!arg->GetType().IsArray()) {
          statements->Add(std::make_shared<Assignment>(
              v, std::make_shared<NewExpression>(
                     InstantiableJavaSignatureOf(arg->GetType(), typenames))));
        } else {
          generate_new_array(arg->GetType(), typenames, statements, v, transact_data);
        }
      }

      realCall->arguments.push_back(v);
    }
  }

  if (options.GenTraces()) {
    // try and finally, but only when generating trace code
    tryStatement = std::make_shared<TryStatement>();
    finallyStatement = std::make_shared<FinallyStatement>();

    tryStatement->statements->Add(std::make_shared<MethodCall>(
        std::make_shared<LiteralExpression>("android.os.Trace"), "traceBegin",
        std::vector<std::shared_ptr<Expression>>{
            std::make_shared<LiteralExpression>("android.os.Trace.TRACE_TAG_AIDL"),
            std::make_shared<StringLiteralExpression>(iface.GetName() + "::" + method.GetName() +
                                                      "::server")}));

    finallyStatement->statements->Add(std::make_shared<MethodCall>(
        std::make_shared<LiteralExpression>("android.os.Trace"), "traceEnd",
        std::vector<std::shared_ptr<Expression>>{
            std::make_shared<LiteralExpression>("android.os.Trace.TRACE_TAG_AIDL")}));
  }

  // the real call
  if (method.GetType().GetName() == "void") {
    if (options.GenTraces()) {
      statements->Add(tryStatement);
      tryStatement->statements->Add(realCall);
      statements->Add(finallyStatement);
    } else {
      statements->Add(realCall);
    }

    if (!oneway) {
      // report that there were no exceptions
      auto ex = std::make_shared<MethodCall>(transact_reply, "writeNoException");
      statements->Add(ex);
    }
  } else {
    auto _result =
        std::make_shared<Variable>(JavaSignatureOf(method.GetType(), typenames), "_result");
    if (options.GenTraces()) {
      statements->Add(std::make_shared<VariableDeclaration>(_result));
      statements->Add(tryStatement);
      tryStatement->statements->Add(std::make_shared<Assignment>(_result, realCall));
      statements->Add(finallyStatement);
    } else {
      statements->Add(std::make_shared<VariableDeclaration>(_result, realCall));
    }

    if (!oneway) {
      // report that there were no exceptions
      auto ex = std::make_shared<MethodCall>(transact_reply, "writeNoException");
      statements->Add(ex);
    }

    // marshall the return value
    generate_write_to_parcel(method.GetType(), statements, _result, transact_reply, true,
                             typenames);
  }

  // out parameters
  int i = 0;
  for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
    std::shared_ptr<Variable> v = stubArgs.Get(i++);

    if (arg->GetDirection() & AidlArgument::OUT_DIR) {
      generate_write_to_parcel(arg->GetType(), statements, v, transact_reply, true, typenames);
    }
  }

  // return true
  statements->Add(std::make_shared<ReturnStatement>(TRUE_VALUE));
}

static void generate_stub_case(const AidlInterface& iface, const AidlMethod& method,
                               const std::string& transactCodeName, bool oneway,
                               std::shared_ptr<StubClass> stubClass, const AidlTypenames& typenames,
                               const Options& options) {
  auto c = std::make_shared<Case>(transactCodeName);

  generate_stub_code(iface, method, oneway, stubClass->transact_data, stubClass->transact_reply,
                     typenames, c->statements, stubClass, options);

  stubClass->transact_switch->cases.push_back(c);
}

static void generate_stub_case_outline(const AidlInterface& iface, const AidlMethod& method,
                                       const std::string& transactCodeName, bool oneway,
                                       std::shared_ptr<StubClass> stubClass,
                                       const AidlTypenames& typenames, const Options& options) {
  std::string outline_name = "onTransact$" + method.GetName() + "$";
  // Generate an "outlined" method with the actual code.
  {
    auto transact_data = std::make_shared<Variable>("android.os.Parcel", "data");
    auto transact_reply = std::make_shared<Variable>("android.os.Parcel", "reply");
    auto onTransact_case = std::make_shared<Method>();
    onTransact_case->modifiers = PRIVATE;
    onTransact_case->returnType = "boolean";
    onTransact_case->name = outline_name;
    onTransact_case->parameters.push_back(transact_data);
    onTransact_case->parameters.push_back(transact_reply);
    onTransact_case->statements = std::make_shared<StatementBlock>();
    onTransact_case->exceptions.push_back("android.os.RemoteException");
    stubClass->elements.push_back(onTransact_case);

    generate_stub_code(iface, method, oneway, transact_data, transact_reply, typenames,
                       onTransact_case->statements, stubClass, options);
  }

  // Generate the case dispatch.
  {
    auto c = std::make_shared<Case>(transactCodeName);

    auto helper_call =
        std::make_shared<MethodCall>(THIS_VALUE, outline_name,
                                     std::vector<std::shared_ptr<Expression>>{
                                         stubClass->transact_data, stubClass->transact_reply});
    c->statements->Add(std::make_shared<ReturnStatement>(helper_call));

    stubClass->transact_switch->cases.push_back(c);
  }
}

static std::shared_ptr<Method> generate_proxy_method(
    const AidlInterface& iface, const AidlMethod& method, const std::string& transactCodeName,
    bool oneway, std::shared_ptr<ProxyClass> proxyClass, const AidlTypenames& typenames,
    const Options& options) {
  auto proxy = std::make_shared<Method>();
  proxy->comment = method.GetComments();
  proxy->modifiers = PUBLIC | OVERRIDE;
  proxy->returnType = JavaSignatureOf(method.GetType(), typenames);
  proxy->name = method.GetName();
  proxy->statements = std::make_shared<StatementBlock>();
  for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
    proxy->parameters.push_back(
        std::make_shared<Variable>(JavaSignatureOf(arg->GetType(), typenames), arg->GetName()));
  }
  proxy->exceptions.push_back("android.os.RemoteException");

  // the parcels
  auto _data = std::make_shared<Variable>("android.os.Parcel", "_data");
  proxy->statements->Add(std::make_shared<VariableDeclaration>(
      _data, std::make_shared<MethodCall>("android.os.Parcel", "obtain")));
  std::shared_ptr<Variable> _reply = nullptr;
  if (!oneway) {
    _reply = std::make_shared<Variable>("android.os.Parcel", "_reply");
    proxy->statements->Add(std::make_shared<VariableDeclaration>(
        _reply, std::make_shared<MethodCall>("android.os.Parcel", "obtain")));
  }

  // the return value
  std::shared_ptr<Variable> _result = nullptr;
  if (method.GetType().GetName() != "void") {
    _result = std::make_shared<Variable>(*proxy->returnType, "_result");
    proxy->statements->Add(std::make_shared<VariableDeclaration>(_result));
  }

  // try and finally
  auto tryStatement = std::make_shared<TryStatement>();
  proxy->statements->Add(tryStatement);
  auto finallyStatement = std::make_shared<FinallyStatement>();
  proxy->statements->Add(finallyStatement);

  if (options.GenTraces()) {
    tryStatement->statements->Add(std::make_shared<MethodCall>(
        std::make_shared<LiteralExpression>("android.os.Trace"), "traceBegin",
        std::vector<std::shared_ptr<Expression>>{
            std::make_shared<LiteralExpression>("android.os.Trace.TRACE_TAG_AIDL"),
            std::make_shared<StringLiteralExpression>(iface.GetName() + "::" + method.GetName() +
                                                      "::client")}));
  }

  // the interface identifier token: the DESCRIPTOR constant, marshalled as a
  // string
  tryStatement->statements->Add(std::make_shared<MethodCall>(
      _data, "writeInterfaceToken",
      std::vector<std::shared_ptr<Expression>>{std::make_shared<LiteralExpression>("DESCRIPTOR")}));

  // the parameters
  for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
    auto v = std::make_shared<Variable>(JavaSignatureOf(arg->GetType(), typenames), arg->GetName());
    AidlArgument::Direction dir = arg->GetDirection();
    if (dir == AidlArgument::OUT_DIR && arg->GetType().IsArray()) {
      auto checklen = std::make_shared<IfStatement>();
      checklen->expression = std::make_shared<Comparison>(v, "==", NULL_VALUE);
      checklen->statements->Add(std::make_shared<MethodCall>(
          _data, "writeInt",
          std::vector<std::shared_ptr<Expression>>{std::make_shared<LiteralExpression>("-1")}));
      checklen->elseif = std::make_shared<IfStatement>();
      checklen->elseif->statements->Add(std::make_shared<MethodCall>(
          _data, "writeInt",
          std::vector<std::shared_ptr<Expression>>{std::make_shared<FieldVariable>(v, "length")}));
      tryStatement->statements->Add(checklen);
    } else if (dir & AidlArgument::IN_DIR) {
      generate_write_to_parcel(arg->GetType(), tryStatement->statements, v, _data, false,
                               typenames);
    }
  }

  // the transact call
  auto call = std::make_shared<MethodCall>(
      proxyClass->mRemote, "transact",
      std::vector<std::shared_ptr<Expression>>{
          std::make_shared<LiteralExpression>("Stub." + transactCodeName), _data,
          _reply ? _reply : NULL_VALUE,
          std::make_shared<LiteralExpression>(oneway ? "android.os.IBinder.FLAG_ONEWAY" : "0")});
  auto _status = std::make_shared<Variable>("boolean", "_status");
  tryStatement->statements->Add(std::make_shared<VariableDeclaration>(_status, call));

  // If the transaction returns false, which means UNKNOWN_TRANSACTION, fall
  // back to the local method in the default impl, if set before.
  vector<string> arg_names;
  for (const auto& arg : method.GetArguments()) {
    arg_names.emplace_back(arg->GetName());
  }
  bool has_return_type = method.GetType().GetName() != "void";
  tryStatement->statements->Add(std::make_shared<LiteralStatement>(
      android::base::StringPrintf(has_return_type ? "if (!_status && getDefaultImpl() != null) {\n"
                                                    "  return getDefaultImpl().%s(%s);\n"
                                                    "}\n"
                                                  : "if (!_status && getDefaultImpl() != null) {\n"
                                                    "  getDefaultImpl().%s(%s);\n"
                                                    "  return;\n"
                                                    "}\n",
                                  method.GetName().c_str(), Join(arg_names, ", ").c_str())));

  // throw back exceptions.
  if (_reply) {
    auto ex = std::make_shared<MethodCall>(_reply, "readException");
    tryStatement->statements->Add(ex);
  }

  // returning and cleanup
  if (_reply != nullptr) {
    // keep this across return value and arguments in order to create the
    // classloader at most once.
    bool is_classloader_created = false;
    if (_result != nullptr) {
      string code;
      CodeWriterPtr writer = CodeWriter::ForString(&code);
      CodeGeneratorContext context{.writer = *(writer.get()),
                                   .typenames = typenames,
                                   .type = method.GetType(),
                                   .parcel = _reply->name,
                                   .var = _result->name,
                                   .is_classloader_created = &is_classloader_created};
      CreateFromParcelFor(context);
      writer->Close();
      tryStatement->statements->Add(std::make_shared<LiteralStatement>(code));
    }

    // the out/inout parameters
    for (const std::unique_ptr<AidlArgument>& arg : method.GetArguments()) {
      if (arg->GetDirection() & AidlArgument::OUT_DIR) {
        string code;
        CodeWriterPtr writer = CodeWriter::ForString(&code);
        CodeGeneratorContext context{.writer = *(writer.get()),
                                     .typenames = typenames,
                                     .type = arg->GetType(),
                                     .parcel = _reply->name,
                                     .var = arg->GetName(),
                                     .is_classloader_created = &is_classloader_created};
        ReadFromParcelFor(context);
        writer->Close();
        tryStatement->statements->Add(std::make_shared<LiteralStatement>(code));
      }
    }

    finallyStatement->statements->Add(std::make_shared<MethodCall>(_reply, "recycle"));
  }
  finallyStatement->statements->Add(std::make_shared<MethodCall>(_data, "recycle"));

  if (options.GenTraces()) {
    finallyStatement->statements->Add(std::make_shared<MethodCall>(
        std::make_shared<LiteralExpression>("android.os.Trace"), "traceEnd",
        std::vector<std::shared_ptr<Expression>>{
            std::make_shared<LiteralExpression>("android.os.Trace.TRACE_TAG_AIDL")}));
  }

  if (_result != nullptr) {
    proxy->statements->Add(std::make_shared<ReturnStatement>(_result));
  }

  return proxy;
}

static void generate_methods(const AidlInterface& iface, const AidlMethod& method, Class* interface,
                             std::shared_ptr<StubClass> stubClass,
                             std::shared_ptr<ProxyClass> proxyClass, int index,
                             const AidlTypenames& typenames, const Options& options) {
  const bool oneway = method.IsOneway();

  // == the TRANSACT_ constant =============================================
  string transactCodeName = "TRANSACTION_";
  transactCodeName += method.GetName();

  auto transactCode =
      std::make_shared<Field>(STATIC | FINAL, std::make_shared<Variable>("int", transactCodeName));
  transactCode->value =
      StringPrintf("(android.os.IBinder.FIRST_CALL_TRANSACTION + %d)", index);
  stubClass->elements.push_back(transactCode);

  // getTransactionName
  if (options.GenTransactionNames()) {
    auto c = std::make_shared<Case>(transactCodeName);
    c->statements->Add(std::make_shared<ReturnStatement>(
        std::make_shared<StringLiteralExpression>(method.GetName())));
    stubClass->code_to_method_name_switch->cases.push_back(c);
  }

  // == the declaration in the interface ===================================
  std::shared_ptr<ClassElement> decl;
  if (method.IsUserDefined()) {
    decl = generate_interface_method(method, typenames);
  } else {
    if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
      std::ostringstream code;
      code << "public int " << kGetInterfaceVersion << "() "
           << "throws android.os.RemoteException;\n";
      decl = std::make_shared<LiteralClassElement>(code.str());
    }
    if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      std::ostringstream code;
      code << "public String " << kGetInterfaceHash << "() "
           << "throws android.os.RemoteException;\n";
      decl = std::make_shared<LiteralClassElement>(code.str());
    }
  }
  interface->elements.push_back(decl);

  // == the stub method ====================================================
  if (method.IsUserDefined()) {
    bool outline_stub =
        stubClass->transact_outline && stubClass->outline_methods.count(&method) != 0;
    if (outline_stub) {
      generate_stub_case_outline(iface, method, transactCodeName, oneway, stubClass, typenames,
                                 options);
    } else {
      generate_stub_case(iface, method, transactCodeName, oneway, stubClass, typenames, options);
    }
  } else {
    if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
      auto c = std::make_shared<Case>(transactCodeName);
      std::ostringstream code;
      code << "data.enforceInterface(descriptor);\n"
           << "reply.writeNoException();\n"
           << "reply.writeInt(" << kGetInterfaceVersion << "());\n"
           << "return true;\n";
      c->statements->Add(std::make_shared<LiteralStatement>(code.str()));
      stubClass->transact_switch->cases.push_back(c);
    }
    if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      auto c = std::make_shared<Case>(transactCodeName);
      std::ostringstream code;
      code << "data.enforceInterface(descriptor);\n"
           << "reply.writeNoException();\n"
           << "reply.writeString(" << kGetInterfaceHash << "());\n"
           << "return true;\n";
      c->statements->Add(std::make_shared<LiteralStatement>(code.str()));
      stubClass->transact_switch->cases.push_back(c);
    }
  }

  // == the proxy method ===================================================
  std::shared_ptr<ClassElement> proxy = nullptr;
  if (method.IsUserDefined()) {
    proxy = generate_proxy_method(iface, method, transactCodeName, oneway, proxyClass, typenames,
                                  options);

  } else {
    if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
      std::ostringstream code;
      code << "@Override\n"
           << "public int " << kGetInterfaceVersion << "()"
           << " throws "
           << "android.os.RemoteException {\n"
           << "  if (mCachedVersion == -1) {\n"
           << "    android.os.Parcel data = android.os.Parcel.obtain();\n"
           << "    android.os.Parcel reply = android.os.Parcel.obtain();\n"
           << "    try {\n"
           << "      data.writeInterfaceToken(DESCRIPTOR);\n"
           << "      boolean _status = mRemote.transact(Stub." << transactCodeName << ", "
           << "data, reply, 0);\n"
           << "      if (!_status) {\n"
           << "        if (getDefaultImpl() != null) {\n"
           << "          return getDefaultImpl().getInterfaceVersion();\n"
           << "        }\n"
           << "      }\n"
           << "      reply.readException();\n"
           << "      mCachedVersion = reply.readInt();\n"
           << "    } finally {\n"
           << "      reply.recycle();\n"
           << "      data.recycle();\n"
           << "    }\n"
           << "  }\n"
           << "  return mCachedVersion;\n"
           << "}\n";
      proxy = std::make_shared<LiteralClassElement>(code.str());
    }
    if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      std::ostringstream code;
      code << "@Override\n"
           << "public synchronized String " << kGetInterfaceHash << "()"
           << " throws "
           << "android.os.RemoteException {\n"
           << "  if (\"-1\".equals(mCachedHash)) {\n"
           << "    android.os.Parcel data = android.os.Parcel.obtain();\n"
           << "    android.os.Parcel reply = android.os.Parcel.obtain();\n"
           << "    try {\n"
           << "      data.writeInterfaceToken(DESCRIPTOR);\n"
           << "      boolean _status = mRemote.transact(Stub." << transactCodeName << ", "
           << "data, reply, 0);\n"
           << "      if (!_status) {\n"
           << "        if (getDefaultImpl() != null) {\n"
           << "          return getDefaultImpl().getInterfaceHash();\n"
           << "        }\n"
           << "      }\n"
           << "      reply.readException();\n"
           << "      mCachedHash = reply.readString();\n"
           << "    } finally {\n"
           << "      reply.recycle();\n"
           << "      data.recycle();\n"
           << "    }\n"
           << "  }\n"
           << "  return mCachedHash;\n"
           << "}\n";
      proxy = std::make_shared<LiteralClassElement>(code.str());
    }
  }
  if (proxy != nullptr) {
    proxyClass->elements.push_back(proxy);
  }
}

static void generate_interface_descriptors(std::shared_ptr<StubClass> stub,
                                           std::shared_ptr<ProxyClass> proxy) {
  // the interface descriptor transaction handler
  auto c = std::make_shared<Case>("INTERFACE_TRANSACTION");
  c->statements->Add(std::make_shared<MethodCall>(
      stub->transact_reply, "writeString",
      std::vector<std::shared_ptr<Expression>>{stub->get_transact_descriptor(nullptr)}));
  c->statements->Add(std::make_shared<ReturnStatement>(TRUE_VALUE));
  stub->transact_switch->cases.push_back(c);

  // and the proxy-side method returning the descriptor directly
  auto getDesc = std::make_shared<Method>();
  getDesc->modifiers = PUBLIC;
  getDesc->returnType = "java.lang.String";
  getDesc->name = "getInterfaceDescriptor";
  getDesc->statements = std::make_shared<StatementBlock>();
  getDesc->statements->Add(
      std::make_shared<ReturnStatement>(std::make_shared<LiteralExpression>("DESCRIPTOR")));
  proxy->elements.push_back(getDesc);
}

// Check whether (some) methods in this interface should be "outlined," that
// is, have specific onTransact methods for certain cases. Set up StubClass
// metadata accordingly.
//
// Outlining will be enabled if the interface has more than outline_threshold
// methods. In that case, the methods are sorted by number of arguments
// (so that more "complex" methods come later), and the first non_outline_count
// number of methods not outlined (are kept in the onTransact() method).
//
// Requirements: non_outline_count <= outline_threshold.
static void compute_outline_methods(const AidlInterface* iface,
                                    const std::shared_ptr<StubClass> stub, size_t outline_threshold,
                                    size_t non_outline_count) {
  CHECK_LE(non_outline_count, outline_threshold);
  // We'll outline (create sub methods) if there are more than min_methods
  // cases.
  stub->transact_outline = iface->GetMethods().size() > outline_threshold;
  if (stub->transact_outline) {
    stub->all_method_count = iface->GetMethods().size();
    std::vector<const AidlMethod*> methods;
    methods.reserve(iface->GetMethods().size());
    for (const std::unique_ptr<AidlMethod>& ptr : iface->GetMethods()) {
      methods.push_back(ptr.get());
    }

    std::stable_sort(
        methods.begin(),
        methods.end(),
        [](const AidlMethod* m1, const AidlMethod* m2) {
          return m1->GetArguments().size() < m2->GetArguments().size();
        });

    stub->outline_methods.insert(methods.begin() + non_outline_count,
                                 methods.end());
  }
}

static shared_ptr<ClassElement> generate_default_impl_method(const AidlMethod& method,
                                                             const AidlTypenames& typenames) {
  auto default_method = std::make_shared<Method>();
  default_method->comment = method.GetComments();
  default_method->modifiers = PUBLIC | OVERRIDE;
  default_method->returnType = JavaSignatureOf(method.GetType(), typenames);
  default_method->name = method.GetName();
  default_method->statements = std::make_shared<StatementBlock>();
  for (const auto& arg : method.GetArguments()) {
    default_method->parameters.push_back(
        std::make_shared<Variable>(JavaSignatureOf(arg->GetType(), typenames), arg->GetName()));
  }
  default_method->exceptions.push_back("android.os.RemoteException");

  if (method.GetType().GetName() != "void") {
    const string& defaultValue = DefaultJavaValueOf(method.GetType(), typenames);
    default_method->statements->Add(
        std::make_shared<LiteralStatement>(StringPrintf("return %s;\n", defaultValue.c_str())));
  }
  return default_method;
}

static shared_ptr<Class> generate_default_impl_class(const AidlInterface& iface,
                                                     const AidlTypenames& typenames,
                                                     const Options& options) {
  auto default_class = std::make_shared<Class>();
  default_class->comment = "/** Default implementation for " + iface.GetName() + ". */";
  default_class->modifiers = PUBLIC | STATIC;
  default_class->what = Class::CLASS;
  default_class->type = iface.GetCanonicalName() + ".Default";
  default_class->interfaces.emplace_back(iface.GetCanonicalName());

  for (const auto& m : iface.GetMethods()) {
    if (m->IsUserDefined()) {
      default_class->elements.emplace_back(generate_default_impl_method(*m.get(), typenames));
    } else {
      // These are called only when the remote side does not implement these
      // methods, which is normally impossible, because these methods are
      // automatically declared in the interface class and not implementing
      // them on the remote side causes a compilation error. But if the remote
      // side somehow managed to not implement it, that's an error and we
      // report the case by returning an invalid value here.
      if (m->GetName() == kGetInterfaceVersion && options.Version() > 0) {
        std::ostringstream code;
        code << "@Override\n"
             << "public int " << kGetInterfaceVersion << "() {\n"
             << "  return 0;\n"
             << "}\n";
        default_class->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
      }
      if (m->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
        std::ostringstream code;
        code << "@Override\n"
             << "public String " << kGetInterfaceHash << "() {\n"
             << "  return \"\";\n"
             << "}\n";
        default_class->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
      }
    }
  }

  default_class->elements.emplace_back(
      std::make_shared<LiteralClassElement>("@Override\n"
                                            "public android.os.IBinder asBinder() {\n"
                                            "  return null;\n"
                                            "}\n"));

  return default_class;
}

std::unique_ptr<Class> generate_binder_interface_class(const AidlInterface* iface,
                                                       const AidlTypenames& typenames,
                                                       const Options& options) {
  // the interface class
  auto interface = std::make_unique<Class>();
  interface->comment = iface->GetComments();
  interface->modifiers = PUBLIC;
  interface->what = Class::INTERFACE;
  interface->type = iface->GetCanonicalName();
  interface->interfaces.push_back("android.os.IInterface");
  interface->annotations = generate_java_annotations(*iface);

  if (options.Version()) {
    std::ostringstream code;
    code << "/**\n"
         << " * The version of this interface that the caller is built against.\n"
         << " * This might be different from what {@link #getInterfaceVersion()\n"
         << " * getInterfaceVersion} returns as that is the version of the interface\n"
         << " * that the remote object is implementing.\n"
         << " */\n"
         << "public static final int VERSION = " << options.Version() << ";\n";
    interface->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
  }
  if (!options.Hash().empty()) {
    std::ostringstream code;
    code << "public static final String HASH = \"" << options.Hash() << "\";\n";
    interface->elements.emplace_back(std::make_shared<LiteralClassElement>(code.str()));
  }

  // the default impl class
  auto default_impl = generate_default_impl_class(*iface, typenames, options);
  interface->elements.emplace_back(default_impl);

  // the stub inner class
  auto stub = std::make_shared<StubClass>(iface, options);
  interface->elements.push_back(stub);

  compute_outline_methods(iface,
                          stub,
                          options.onTransact_outline_threshold_,
                          options.onTransact_non_outline_count_);

  // the proxy inner class
  auto proxy = std::make_shared<ProxyClass>(iface, options);
  stub->elements.push_back(proxy);

  // stub and proxy support for getInterfaceDescriptor()
  generate_interface_descriptors(stub, proxy);

  // all the declared constants of the interface
  for (const auto& constant : iface->GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();
    auto comment = constant->GetType().GetComments();
    if (comment.length() != 0) {
      auto code = StringPrintf("%s\n", comment.c_str());
      interface->elements.push_back(std::make_shared<LiteralClassElement>(code));
    }
    switch (value.GetType()) {
      case AidlConstantValue::Type::STRING: {
        generate_string_constant(interface.get(), constant->GetName(),
                                 constant->ValueString(ConstantValueDecorator));
        break;
      }
      case AidlConstantValue::Type::BOOLEAN:  // fall-through
      case AidlConstantValue::Type::INT8:     // fall-through
      case AidlConstantValue::Type::INT32: {
        generate_int_constant(interface.get(), constant->GetName(),
                              constant->ValueString(ConstantValueDecorator));
        break;
      }
      default: {
        LOG(FATAL) << "Unrecognized constant type: " << static_cast<int>(value.GetType());
      }
    }
  }

  // all the declared methods of the interface

  for (const auto& item : iface->GetMethods()) {
    generate_methods(*iface, *item, interface.get(), stub, proxy, item->GetId(), typenames,
                     options);
  }

  // additional static methods for the default impl set/get to the
  // stub class. Can't add them to the interface as the generated java files
  // may be compiled with Java < 1.7 where static interface method isn't
  // supported.
  // TODO(b/111417145) make this conditional depending on the Java language
  // version requested
  const string i_name = iface->GetCanonicalName();
  stub->elements.emplace_back(std::make_shared<LiteralClassElement>(
      StringPrintf("public static boolean setDefaultImpl(%s impl) {\n"
                   "  // Only one user of this interface can use this function\n"
                   "  // at a time. This is a heuristic to detect if two different\n"
                   "  // users in the same process use this function.\n"
                   "  if (Stub.Proxy.sDefaultImpl != null) {\n"
                   "    throw new IllegalStateException(\"setDefaultImpl() called twice\");\n"
                   "  }\n"
                   "  if (impl != null) {\n"
                   "    Stub.Proxy.sDefaultImpl = impl;\n"
                   "    return true;\n"
                   "  }\n"
                   "  return false;\n"
                   "}\n",
                   i_name.c_str())));
  stub->elements.emplace_back(
      std::make_shared<LiteralClassElement>(StringPrintf("public static %s getDefaultImpl() {\n"
                                                         "  return Stub.Proxy.sDefaultImpl;\n"
                                                         "}\n",
                                                         i_name.c_str())));

  // the static field is defined in the proxy class, not in the interface class
  // because all fields in an interface class are by default final.
  proxy->elements.emplace_back(std::make_shared<LiteralClassElement>(
      StringPrintf("public static %s sDefaultImpl;\n", i_name.c_str())));

  stub->finish();

  return interface;
}

}  // namespace java
}  // namespace aidl
}  // namespace android
