/*
 * Copyright (C) 2015, The Android Open Source Project
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

#include "generate_cpp.h"
#include "aidl.h"

#include <cctype>
#include <cstring>
#include <memory>
#include <random>
#include <set>
#include <string>

#include <android-base/stringprintf.h>

#include "aidl_language.h"
#include "aidl_to_cpp.h"
#include "ast_cpp.h"
#include "code_writer.h"
#include "logging.h"
#include "os.h"

using android::base::Join;
using android::base::StringPrintf;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace aidl {
namespace cpp {
namespace internals {
namespace {

const char kAndroidStatusVarName[] = "_aidl_ret_status";
const char kCodeVarName[] = "_aidl_code";
const char kFlagsVarName[] = "_aidl_flags";
const char kDataVarName[] = "_aidl_data";
const char kErrorLabel[] = "_aidl_error";
const char kImplVarName[] = "_aidl_impl";
const char kReplyVarName[] = "_aidl_reply";
const char kReturnVarName[] = "_aidl_return";
const char kStatusVarName[] = "_aidl_status";
const char kTraceVarName[] = "_aidl_trace";
const char kAndroidParcelLiteral[] = "::android::Parcel";
const char kAndroidStatusLiteral[] = "::android::status_t";
const char kAndroidStatusOk[] = "::android::OK";
const char kBinderStatusLiteral[] = "::android::binder::Status";
const char kIBinderHeader[] = "binder/IBinder.h";
const char kIInterfaceHeader[] = "binder/IInterface.h";
const char kParcelHeader[] = "binder/Parcel.h";
const char kStabilityHeader[] = "binder/Stability.h";
const char kStatusHeader[] = "binder/Status.h";
const char kString16Header[] = "utils/String16.h";
const char kTraceHeader[] = "utils/Trace.h";
const char kStrongPointerHeader[] = "utils/StrongPointer.h";
const char kAndroidBaseMacrosHeader[] = "android-base/macros.h";

unique_ptr<AstNode> BreakOnStatusNotOk() {
  IfStatement* ret = new IfStatement(new Comparison(
      new LiteralExpression(kAndroidStatusVarName), "!=",
      new LiteralExpression(kAndroidStatusOk)));
  ret->OnTrue()->AddLiteral("break");
  return unique_ptr<AstNode>(ret);
}

unique_ptr<AstNode> GotoErrorOnBadStatus() {
  IfStatement* ret = new IfStatement(new Comparison(
      new LiteralExpression(kAndroidStatusVarName), "!=",
      new LiteralExpression(kAndroidStatusOk)));
  ret->OnTrue()->AddLiteral(StringPrintf("goto %s", kErrorLabel));
  return unique_ptr<AstNode>(ret);
}

unique_ptr<AstNode> ReturnOnStatusNotOk() {
  IfStatement* ret = new IfStatement(new Comparison(new LiteralExpression(kAndroidStatusVarName),
                                                    "!=", new LiteralExpression(kAndroidStatusOk)));
  ret->OnTrue()->AddLiteral(StringPrintf("return %s", kAndroidStatusVarName));
  return unique_ptr<AstNode>(ret);
}

ArgList BuildArgList(const AidlTypenames& typenames, const AidlMethod& method, bool for_declaration,
                     bool type_name_only = false) {
  // Build up the argument list for the server method call.
  vector<string> method_arguments;
  for (const unique_ptr<AidlArgument>& a : method.GetArguments()) {
    string literal;
    // b/144943748: CppNameOf FileDescriptor is unique_fd. Don't pass it by
    // const reference but by value to make it easier for the user to keep
    // it beyond the scope of the call. unique_fd is a thin wrapper for an
    // int (fd) so passing by value is not expensive.
    const bool nonCopyable = IsNonCopyableType(a->GetType(), typenames);
    if (for_declaration) {
      // Method declarations need typenames, pointers to out params, and variable
      // names that match the .aidl specification.
      literal = CppNameOf(a->GetType(), typenames);

      if (a->IsOut()) {
        literal = literal + "*";
      } else {
        const auto definedType = typenames.TryGetDefinedType(a->GetType().GetName());

        const bool isEnum = definedType && definedType->AsEnumDeclaration() != nullptr;
        const bool isPrimitive = AidlTypenames::IsPrimitiveTypename(a->GetType().GetName());

        // We pass in parameters that are not primitives by const reference.
        // Arrays of primitives are not primitives.
        if (!(isPrimitive || isEnum || nonCopyable) || a->GetType().IsArray()) {
          literal = "const " + literal + "&";
        }
      }
      if (!type_name_only) {
        literal += " " + a->GetName();
      }
    } else {
      std::string varName = BuildVarName(*a);
      if (a->IsOut()) {
        literal = "&" + varName;
      } else if (nonCopyable) {
        literal = "std::move(" + varName + ")";
      } else {
        literal = varName;
      }
    }
    method_arguments.push_back(literal);
  }

  if (method.GetType().GetName() != "void") {
    string literal;
    if (for_declaration) {
      literal = CppNameOf(method.GetType(), typenames) + "*";
      if (!type_name_only) {
        literal += " " + string(kReturnVarName);
      }
    } else {
      literal = string{"&"} + kReturnVarName;
    }
    method_arguments.push_back(literal);
  }

  return ArgList(method_arguments);
}

unique_ptr<Declaration> BuildMethodDecl(const AidlMethod& method, const AidlTypenames& typenames,
                                        bool for_interface) {
  uint32_t modifiers = 0;
  if (for_interface) {
    modifiers |= MethodDecl::IS_VIRTUAL;
    modifiers |= MethodDecl::IS_PURE_VIRTUAL;
  } else {
    modifiers |= MethodDecl::IS_OVERRIDE;
  }

  return unique_ptr<Declaration>{
      new MethodDecl{kBinderStatusLiteral, method.GetName(),
                     BuildArgList(typenames, method, true /* for method decl */), modifiers}};
}

unique_ptr<Declaration> BuildMetaMethodDecl(const AidlMethod& method, const AidlTypenames&,
                                            const Options& options, bool for_interface) {
  CHECK(!method.IsUserDefined());
  if (method.GetName() == kGetInterfaceVersion && options.Version()) {
    std::ostringstream code;
    if (for_interface) {
      code << "virtual ";
    }
    code << "int32_t " << kGetInterfaceVersion << "()";
    if (for_interface) {
      code << " = 0;\n";
    } else {
      code << " override;\n";
    }
    return unique_ptr<Declaration>(new LiteralDecl(code.str()));
  }
  if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
    std::ostringstream code;
    if (for_interface) {
      code << "virtual ";
    }
    code << "std::string " << kGetInterfaceHash << "()";
    if (for_interface) {
      code << " = 0;\n";
    } else {
      code << " override;\n";
    }
    return unique_ptr<Declaration>(new LiteralDecl(code.str()));
  }
  return nullptr;
}

std::vector<unique_ptr<Declaration>> NestInNamespaces(vector<unique_ptr<Declaration>> decls,
                                                      const vector<string>& package) {
  auto it = package.crbegin();  // Iterate over the namespaces inner to outer
  for (; it != package.crend(); ++it) {
    vector<unique_ptr<Declaration>> inner;
    inner.emplace_back(unique_ptr<Declaration>{new CppNamespace{*it, std::move(decls)}});

    decls = std::move(inner);
  }
  return decls;
}

std::vector<unique_ptr<Declaration>> NestInNamespaces(unique_ptr<Declaration> decl,
                                                      const vector<string>& package) {
  vector<unique_ptr<Declaration>> decls;
  decls.push_back(std::move(decl));
  return NestInNamespaces(std::move(decls), package);
}

bool DeclareLocalVariable(const AidlArgument& a, StatementBlock* b,
                          const AidlTypenames& typenamespaces) {
  string type = CppNameOf(a.GetType(), typenamespaces);

  b->AddLiteral(type + " " + BuildVarName(a));
  return true;
}

string BuildHeaderGuard(const AidlDefinedType& defined_type, ClassNames header_type) {
  string class_name = ClassName(defined_type, header_type);
  for (size_t i = 1; i < class_name.size(); ++i) {
    if (isupper(class_name[i])) {
      class_name.insert(i, "_");
      ++i;
    }
  }
  string ret = StringPrintf("AIDL_GENERATED_%s_%s_H_", defined_type.GetPackage().c_str(),
                            class_name.c_str());
  for (char& c : ret) {
    if (c == '.') {
      c = '_';
    }
    c = toupper(c);
  }
  return ret;
}

unique_ptr<Declaration> DefineClientTransaction(const AidlTypenames& typenames,
                                                const AidlInterface& interface,
                                                const AidlMethod& method, const Options& options) {
  const string i_name = ClassName(interface, ClassNames::INTERFACE);
  const string bp_name = ClassName(interface, ClassNames::CLIENT);
  unique_ptr<MethodImpl> ret{
      new MethodImpl{kBinderStatusLiteral, bp_name, method.GetName(),
                     ArgList{BuildArgList(typenames, method, true /* for method decl */)}}};
  StatementBlock* b = ret->GetStatementBlock();

  // Declare parcels to hold our query and the response.
  b->AddLiteral(StringPrintf("%s %s", kAndroidParcelLiteral, kDataVarName));
  // Even if we're oneway, the transact method still takes a parcel.
  b->AddLiteral(StringPrintf("%s %s", kAndroidParcelLiteral, kReplyVarName));

  // Declare the status_t variable we need for error handling.
  b->AddLiteral(StringPrintf("%s %s = %s", kAndroidStatusLiteral,
                             kAndroidStatusVarName,
                             kAndroidStatusOk));
  // We unconditionally return a Status object.
  b->AddLiteral(StringPrintf("%s %s", kBinderStatusLiteral, kStatusVarName));

  if (options.GenTraces()) {
    b->AddLiteral(StringPrintf("::android::ScopedTrace %s(ATRACE_TAG_AIDL, \"%s::%s::cppClient\")",
                               kTraceVarName, interface.GetName().c_str(),
                               method.GetName().c_str()));
  }

  if (options.GenLog()) {
    b->AddLiteral(GenLogBeforeExecute(bp_name, method, false /* isServer */, false /* isNdk */),
                  false /* no semicolon */);
  }

  // Add the name of the interface we're hoping to call.
  b->AddStatement(new Assignment(
      kAndroidStatusVarName,
      new MethodCall(StringPrintf("%s.writeInterfaceToken",
                                  kDataVarName),
                     "getInterfaceDescriptor()")));
  b->AddStatement(GotoErrorOnBadStatus());

  for (const auto& a: method.GetArguments()) {
    const string var_name = ((a->IsOut()) ? "*" : "") + a->GetName();

    if (a->IsIn()) {
      // Serialization looks roughly like:
      //     _aidl_ret_status = _aidl_data.WriteInt32(in_param_name);
      //     if (_aidl_ret_status != ::android::OK) { goto error; }
      const string& method = ParcelWriteMethodOf(a->GetType(), typenames);
      b->AddStatement(
          new Assignment(kAndroidStatusVarName,
                         new MethodCall(StringPrintf("%s.%s", kDataVarName, method.c_str()),
                                        ParcelWriteCastOf(a->GetType(), typenames, var_name))));
      b->AddStatement(GotoErrorOnBadStatus());
    } else if (a->IsOut() && a->GetType().IsArray()) {
      // Special case, the length of the out array is written into the parcel.
      //     _aidl_ret_status = _aidl_data.writeVectorSize(&out_param_name);
      //     if (_aidl_ret_status != ::android::OK) { goto error; }
      b->AddStatement(new Assignment(
          kAndroidStatusVarName,
          new MethodCall(StringPrintf("%s.writeVectorSize", kDataVarName), var_name)));
      b->AddStatement(GotoErrorOnBadStatus());
    }
  }

  // Invoke the transaction on the remote binder and confirm status.
  string transaction_code = GetTransactionIdFor(method);

  vector<string> args = {transaction_code, kDataVarName,
                         StringPrintf("&%s", kReplyVarName)};

  if (method.IsOneway()) {
    args.push_back("::android::IBinder::FLAG_ONEWAY");
  }

  b->AddStatement(new Assignment(
      kAndroidStatusVarName,
      new MethodCall("remote()->transact",
                     ArgList(args))));

  // If the method is not implemented in the remote side, try to call the
  // default implementation, if provided.
  vector<string> arg_names;
  for (const auto& a : method.GetArguments()) {
    if (IsNonCopyableType(a->GetType(), typenames)) {
      arg_names.emplace_back(StringPrintf("std::move(%s)", a->GetName().c_str()));
    } else {
      arg_names.emplace_back(a->GetName());
    }
  }
  if (method.GetType().GetName() != "void") {
    arg_names.emplace_back(kReturnVarName);
  }
  b->AddLiteral(StringPrintf("if (UNLIKELY(_aidl_ret_status == ::android::UNKNOWN_TRANSACTION && "
                             "%s::getDefaultImpl())) {\n"
                             "   return %s::getDefaultImpl()->%s(%s);\n"
                             "}\n",
                             i_name.c_str(), i_name.c_str(), method.GetName().c_str(),
                             Join(arg_names, ", ").c_str()),
                false /* no semicolon */);

  b->AddStatement(GotoErrorOnBadStatus());

  if (!method.IsOneway()) {
    // Strip off the exception header and fail if we see a remote exception.
    // _aidl_ret_status = _aidl_status.readFromParcel(_aidl_reply);
    // if (_aidl_ret_status != ::android::OK) { goto error; }
    // if (!_aidl_status.isOk()) { return _aidl_ret_status; }
    b->AddStatement(new Assignment(
        kAndroidStatusVarName,
        StringPrintf("%s.readFromParcel(%s)", kStatusVarName, kReplyVarName)));
    b->AddStatement(GotoErrorOnBadStatus());
    IfStatement* exception_check = new IfStatement(
        new LiteralExpression(StringPrintf("!%s.isOk()", kStatusVarName)));
    b->AddStatement(exception_check);
    exception_check->OnTrue()->AddLiteral(
        StringPrintf("return %s", kStatusVarName));
  }

  // Type checking should guarantee that nothing below emits code until "return
  // status" if we are a oneway method, so no more fear of accessing reply.

  // If the method is expected to return something, read it first by convention.
  if (method.GetType().GetName() != "void") {
    const string& method_call = ParcelReadMethodOf(method.GetType(), typenames);
    b->AddStatement(new Assignment(
        kAndroidStatusVarName,
        new MethodCall(StringPrintf("%s.%s", kReplyVarName, method_call.c_str()),
                       ParcelReadCastOf(method.GetType(), typenames, kReturnVarName))));
    b->AddStatement(GotoErrorOnBadStatus());
  }

  for (const AidlArgument* a : method.GetOutArguments()) {
    // Deserialization looks roughly like:
    //     _aidl_ret_status = _aidl_reply.ReadInt32(out_param_name);
    //     if (_aidl_status != ::android::OK) { goto _aidl_error; }
    string method = ParcelReadMethodOf(a->GetType(), typenames);

    b->AddStatement(
        new Assignment(kAndroidStatusVarName,
                       new MethodCall(StringPrintf("%s.%s", kReplyVarName, method.c_str()),
                                      ParcelReadCastOf(a->GetType(), typenames, a->GetName()))));
    b->AddStatement(GotoErrorOnBadStatus());
  }

  // If we've gotten to here, one of two things is true:
  //   1) We've read some bad status_t
  //   2) We've only read status_t == OK and there was no exception in the
  //      response.
  // In both cases, we're free to set Status from the status_t and return.
  b->AddLiteral(StringPrintf("%s:\n", kErrorLabel), false /* no semicolon */);
  b->AddLiteral(
      StringPrintf("%s.setFromStatusT(%s)", kStatusVarName,
                   kAndroidStatusVarName));

  if (options.GenLog()) {
    b->AddLiteral(GenLogAfterExecute(bp_name, interface, method, kStatusVarName, kReturnVarName,
                                     false /* isServer */, false /* isNdk */),
                  false /* no semicolon */);
  }

  b->AddLiteral(StringPrintf("return %s", kStatusVarName));

  return unique_ptr<Declaration>(ret.release());
}

unique_ptr<Declaration> DefineClientMetaTransaction(const AidlTypenames& /* typenames */,
                                                    const AidlInterface& interface,
                                                    const AidlMethod& method,
                                                    const Options& options) {
  CHECK(!method.IsUserDefined());
  if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
    const string iface = ClassName(interface, ClassNames::INTERFACE);
    const string proxy = ClassName(interface, ClassNames::CLIENT);
    // Note: race condition can happen here, but no locking is required
    // because 1) writing an interger is atomic and 2) this transaction
    // will always return the same value, i.e., competing threads will
    // give write the same value to cached_version_.
    std::ostringstream code;
    code << "int32_t " << proxy << "::" << kGetInterfaceVersion << "() {\n"
         << "  if (cached_version_ == -1) {\n"
         << "    ::android::Parcel data;\n"
         << "    ::android::Parcel reply;\n"
         << "    data.writeInterfaceToken(getInterfaceDescriptor());\n"
         << "    ::android::status_t err = remote()->transact(" << GetTransactionIdFor(method)
         << ", data, &reply);\n"
         << "    if (err == ::android::OK) {\n"
         << "      ::android::binder::Status _aidl_status;\n"
         << "      err = _aidl_status.readFromParcel(reply);\n"
         << "      if (err == ::android::OK && _aidl_status.isOk()) {\n"
         << "        cached_version_ = reply.readInt32();\n"
         << "      }\n"
         << "    }\n"
         << "  }\n"
         << "  return cached_version_;\n"
         << "}\n";
    return unique_ptr<Declaration>(new LiteralDecl(code.str()));
  }
  if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
    const string iface = ClassName(interface, ClassNames::INTERFACE);
    const string proxy = ClassName(interface, ClassNames::CLIENT);
    std::ostringstream code;
    code << "std::string " << proxy << "::" << kGetInterfaceHash << "() {\n"
         << "  std::lock_guard<std::mutex> lockGuard(cached_hash_mutex_);\n"
         << "  if (cached_hash_ == \"-1\") {\n"
         << "    ::android::Parcel data;\n"
         << "    ::android::Parcel reply;\n"
         << "    data.writeInterfaceToken(getInterfaceDescriptor());\n"
         << "    ::android::status_t err = remote()->transact(" << GetTransactionIdFor(method)
         << ", data, &reply);\n"
         << "    if (err == ::android::OK) {\n"
         << "      ::android::binder::Status _aidl_status;\n"
         << "      err = _aidl_status.readFromParcel(reply);\n"
         << "      if (err == ::android::OK && _aidl_status.isOk()) {\n"
         << "        reply.readUtf8FromUtf16(&cached_hash_);\n"
         << "      }\n"
         << "    }\n"
         << "  }\n"
         << "  return cached_hash_;\n"
         << "}\n";
    return unique_ptr<Declaration>(new LiteralDecl(code.str()));
  }
  return nullptr;
}

}  // namespace

unique_ptr<Document> BuildClientSource(const AidlTypenames& typenames,
                                       const AidlInterface& interface, const Options& options) {
  vector<string> include_list = {
      HeaderFile(interface, ClassNames::CLIENT, false),
      kParcelHeader,
      kAndroidBaseMacrosHeader
  };
  if (options.GenLog()) {
    include_list.emplace_back("chrono");
    include_list.emplace_back("functional");
    include_list.emplace_back("json/value.h");
  }
  vector<unique_ptr<Declaration>> file_decls;

  // The constructor just passes the IBinder instance up to the super
  // class.
  const string i_name = ClassName(interface, ClassNames::INTERFACE);
  file_decls.push_back(unique_ptr<Declaration>{new ConstructorImpl{
      ClassName(interface, ClassNames::CLIENT),
      ArgList{StringPrintf("const ::android::sp<::android::IBinder>& %s",
                           kImplVarName)},
      { "BpInterface<" + i_name + ">(" + kImplVarName + ")" }}});

  if (options.GenLog()) {
    string code;
    ClassName(interface, ClassNames::CLIENT);
    CodeWriterPtr writer = CodeWriter::ForString(&code);
    (*writer) << "std::function<void(const Json::Value&)> "
              << ClassName(interface, ClassNames::CLIENT) << "::logFunc;\n";
    writer->Close();
    file_decls.push_back(unique_ptr<Declaration>(new LiteralDecl(code)));
  }

  // Clients define a method per transaction.
  for (const auto& method : interface.GetMethods()) {
    unique_ptr<Declaration> m;
    if (method->IsUserDefined()) {
      m = DefineClientTransaction(typenames, interface, *method, options);
    } else {
      m = DefineClientMetaTransaction(typenames, interface, *method, options);
    }
    if (!m) { return nullptr; }
    file_decls.push_back(std::move(m));
  }
  return unique_ptr<Document>{new CppSource{
      include_list,
      NestInNamespaces(std::move(file_decls), interface.GetSplitPackage())}};
}

namespace {

bool HandleServerTransaction(const AidlTypenames& typenames, const AidlInterface& interface,
                             const AidlMethod& method, const Options& options, StatementBlock* b) {
  // Declare all the parameters now.  In the common case, we expect no errors
  // in serialization.
  for (const unique_ptr<AidlArgument>& a : method.GetArguments()) {
    if (!DeclareLocalVariable(*a, b, typenames)) {
      return false;
    }
  }

  // Declare a variable to hold the return value.
  if (method.GetType().GetName() != "void") {
    string type = CppNameOf(method.GetType(), typenames);
    b->AddLiteral(StringPrintf("%s %s", type.c_str(), kReturnVarName));
  }

  // Check that the client is calling the correct interface.
  IfStatement* interface_check = new IfStatement(
      new MethodCall(StringPrintf("%s.checkInterface",
                                  kDataVarName), "this"),
      true /* invert the check */);
  b->AddStatement(interface_check);
  interface_check->OnTrue()->AddStatement(
      new Assignment(kAndroidStatusVarName, "::android::BAD_TYPE"));
  interface_check->OnTrue()->AddLiteral("break");

  // Deserialize each "in" parameter to the transaction.
  for (const auto& a: method.GetArguments()) {
    // Deserialization looks roughly like:
    //     _aidl_ret_status = _aidl_data.ReadInt32(&in_param_name);
    //     if (_aidl_ret_status != ::android::OK) { break; }
    const string& var_name = "&" + BuildVarName(*a);
    if (a->IsIn()) {
      const string& readMethod = ParcelReadMethodOf(a->GetType(), typenames);
      b->AddStatement(
          new Assignment{kAndroidStatusVarName,
                         new MethodCall{string(kDataVarName) + "." + readMethod,
                                        ParcelReadCastOf(a->GetType(), typenames, var_name)}});
      b->AddStatement(BreakOnStatusNotOk());
    } else if (a->IsOut() && a->GetType().IsArray()) {
      // Special case, the length of the out array is written into the parcel.
      //     _aidl_ret_status = _aidl_data.resizeOutVector(&out_param_name);
      //     if (_aidl_ret_status != ::android::OK) { break; }
      b->AddStatement(
          new Assignment{kAndroidStatusVarName,
                         new MethodCall{string(kDataVarName) + ".resizeOutVector", var_name}});
      b->AddStatement(BreakOnStatusNotOk());
    }
  }

  if (options.GenTraces()) {
    b->AddStatement(new Statement(new MethodCall("atrace_begin",
        ArgList{{"ATRACE_TAG_AIDL",
        StringPrintf("\"%s::%s::cppServer\"",
                     interface.GetName().c_str(),
                     method.GetName().c_str())}})));
  }
  const string bn_name = ClassName(interface, ClassNames::SERVER);
  if (options.GenLog()) {
    b->AddLiteral(GenLogBeforeExecute(bn_name, method, true /* isServer */, false /* isNdk */),
                  false);
  }
  // Call the actual method.  This is implemented by the subclass.
  vector<unique_ptr<AstNode>> status_args;
  status_args.emplace_back(new MethodCall(
      method.GetName(), BuildArgList(typenames, method, false /* not for method decl */)));
  b->AddStatement(new Statement(new MethodCall(
      StringPrintf("%s %s", kBinderStatusLiteral, kStatusVarName),
      ArgList(std::move(status_args)))));

  if (options.GenTraces()) {
    b->AddStatement(new Statement(new MethodCall("atrace_end",
                                                 "ATRACE_TAG_AIDL")));
  }

  if (options.GenLog()) {
    b->AddLiteral(GenLogAfterExecute(bn_name, interface, method, kStatusVarName, kReturnVarName,
                                     true /* isServer */, false /* isNdk */),
                  false);
  }

  // Write exceptions during transaction handling to parcel.
  if (!method.IsOneway()) {
    b->AddStatement(new Assignment(
        kAndroidStatusVarName,
        StringPrintf("%s.writeToParcel(%s)", kStatusVarName, kReplyVarName)));
    b->AddStatement(BreakOnStatusNotOk());
    IfStatement* exception_check = new IfStatement(
        new LiteralExpression(StringPrintf("!%s.isOk()", kStatusVarName)));
    b->AddStatement(exception_check);
    exception_check->OnTrue()->AddLiteral("break");
  }

  // If we have a return value, write it first.
  if (method.GetType().GetName() != "void") {
    string writeMethod =
        string(kReplyVarName) + "->" + ParcelWriteMethodOf(method.GetType(), typenames);
    b->AddStatement(new Assignment(
        kAndroidStatusVarName,
        new MethodCall(writeMethod,
                       ParcelWriteCastOf(method.GetType(), typenames, kReturnVarName))));
    b->AddStatement(BreakOnStatusNotOk());
  }
  // Write each out parameter to the reply parcel.
  for (const AidlArgument* a : method.GetOutArguments()) {
    // Serialization looks roughly like:
    //     _aidl_ret_status = data.WriteInt32(out_param_name);
    //     if (_aidl_ret_status != ::android::OK) { break; }
    const string& writeMethod = ParcelWriteMethodOf(a->GetType(), typenames);
    b->AddStatement(new Assignment(
        kAndroidStatusVarName,
        new MethodCall(string(kReplyVarName) + "->" + writeMethod,
                       ParcelWriteCastOf(a->GetType(), typenames, BuildVarName(*a)))));
    b->AddStatement(BreakOnStatusNotOk());
  }

  return true;
}

bool HandleServerMetaTransaction(const AidlTypenames&, const AidlInterface& interface,
                                 const AidlMethod& method, const Options& options,
                                 StatementBlock* b) {
  CHECK(!method.IsUserDefined());

  if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
    std::ostringstream code;
    code << "_aidl_data.checkInterface(this);\n"
         << "_aidl_reply->writeNoException();\n"
         << "_aidl_reply->writeInt32(" << ClassName(interface, ClassNames::INTERFACE)
         << "::VERSION)";
    b->AddLiteral(code.str());
    return true;
  }
  if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
    std::ostringstream code;
    code << "_aidl_data.checkInterface(this);\n"
         << "_aidl_reply->writeNoException();\n"
         << "_aidl_reply->writeUtf8AsUtf16(" << ClassName(interface, ClassNames::INTERFACE)
         << "::HASH)";
    b->AddLiteral(code.str());
    return true;
  }
  return false;
}

}  // namespace

unique_ptr<Document> BuildServerSource(const AidlTypenames& typenames,
                                       const AidlInterface& interface, const Options& options) {
  const string bn_name = ClassName(interface, ClassNames::SERVER);
  vector<string> include_list{
      HeaderFile(interface, ClassNames::SERVER, false),
      kParcelHeader,
      kStabilityHeader,
  };
  if (options.GenLog()) {
    include_list.emplace_back("chrono");
    include_list.emplace_back("functional");
    include_list.emplace_back("json/value.h");
  }

  unique_ptr<ConstructorImpl> constructor{
      new ConstructorImpl{ClassName(interface, ClassNames::SERVER), ArgList{}, {}}};

  if (interface.IsVintfStability()) {
    constructor->GetStatementBlock()->AddLiteral("::android::internal::Stability::markVintf(this)");
  } else {
    constructor->GetStatementBlock()->AddLiteral(
        "::android::internal::Stability::markCompilationUnit(this)");
  }

  unique_ptr<MethodImpl> on_transact{new MethodImpl{
      kAndroidStatusLiteral, bn_name, "onTransact",
      ArgList{{StringPrintf("uint32_t %s", kCodeVarName),
               StringPrintf("const %s& %s", kAndroidParcelLiteral,
                            kDataVarName),
               StringPrintf("%s* %s", kAndroidParcelLiteral, kReplyVarName),
               StringPrintf("uint32_t %s", kFlagsVarName)}}
      }};

  // Declare the status_t variable
  on_transact->GetStatementBlock()->AddLiteral(
      StringPrintf("%s %s = %s", kAndroidStatusLiteral, kAndroidStatusVarName,
                   kAndroidStatusOk));

  // Add the all important switch statement, but retain a pointer to it.
  SwitchStatement* s = new SwitchStatement{kCodeVarName};
  on_transact->GetStatementBlock()->AddStatement(s);

  // The switch statement has a case statement for each transaction code.
  for (const auto& method : interface.GetMethods()) {
    StatementBlock* b = s->AddCase(GetTransactionIdFor(*method));
    if (!b) { return nullptr; }

    bool success = false;
    if (method->IsUserDefined()) {
      success = HandleServerTransaction(typenames, interface, *method, options, b);
    } else {
      success = HandleServerMetaTransaction(typenames, interface, *method, options, b);
    }
    if (!success) {
      return nullptr;
    }
  }

  // The switch statement has a default case which defers to the super class.
  // The superclass handles a few pre-defined transactions.
  StatementBlock* b = s->AddCase("");
  b->AddLiteral(StringPrintf(
                "%s = ::android::BBinder::onTransact(%s, %s, "
                "%s, %s)", kAndroidStatusVarName, kCodeVarName,
                kDataVarName, kReplyVarName, kFlagsVarName));

  // If we saw a null reference, we can map that to an appropriate exception.
  IfStatement* null_check = new IfStatement(
      new LiteralExpression(string(kAndroidStatusVarName) +
                            " == ::android::UNEXPECTED_NULL"));
  on_transact->GetStatementBlock()->AddStatement(null_check);
  null_check->OnTrue()->AddStatement(new Assignment(
      kAndroidStatusVarName,
      StringPrintf("%s::fromExceptionCode(%s::EX_NULL_POINTER)"
                   ".writeToParcel(%s)",
                   kBinderStatusLiteral, kBinderStatusLiteral,
                   kReplyVarName)));

  // Finally, the server's onTransact method just returns a status code.
  on_transact->GetStatementBlock()->AddLiteral(
      StringPrintf("return %s", kAndroidStatusVarName));
  vector<unique_ptr<Declaration>> decls;
  decls.push_back(std::move(constructor));
  decls.push_back(std::move(on_transact));

  if (options.Version() > 0) {
    std::ostringstream code;
    code << "int32_t " << bn_name << "::" << kGetInterfaceVersion << "() {\n"
         << "  return " << ClassName(interface, ClassNames::INTERFACE) << "::VERSION;\n"
         << "}\n";
    decls.emplace_back(new LiteralDecl(code.str()));
  }
  if (!options.Hash().empty()) {
    std::ostringstream code;
    code << "std::string " << bn_name << "::" << kGetInterfaceHash << "() {\n"
         << "  return " << ClassName(interface, ClassNames::INTERFACE) << "::HASH;\n"
         << "}\n";
    decls.emplace_back(new LiteralDecl(code.str()));
  }

  if (options.GenLog()) {
    string code;
    ClassName(interface, ClassNames::SERVER);
    CodeWriterPtr writer = CodeWriter::ForString(&code);
    (*writer) << "std::function<void(const Json::Value&)> "
              << ClassName(interface, ClassNames::SERVER) << "::logFunc;\n";
    writer->Close();
    decls.push_back(unique_ptr<Declaration>(new LiteralDecl(code)));
  }
  return unique_ptr<Document>{
      new CppSource{include_list, NestInNamespaces(std::move(decls), interface.GetSplitPackage())}};
}

unique_ptr<Document> BuildInterfaceSource(const AidlTypenames& typenames,
                                          const AidlInterface& interface,
                                          [[maybe_unused]] const Options& options) {
  vector<string> include_list{
      HeaderFile(interface, ClassNames::RAW, false),
      HeaderFile(interface, ClassNames::CLIENT, false),
  };

  string fq_name = ClassName(interface, ClassNames::INTERFACE);
  if (!interface.GetPackage().empty()) {
    fq_name = interface.GetPackage() + "." + fq_name;
  }

  vector<unique_ptr<Declaration>> decls;

  unique_ptr<MacroDecl> meta_if{new MacroDecl{
      "DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE",
      ArgList{vector<string>{ClassName(interface, ClassNames::BASE), '"' + fq_name + '"'}}}};
  decls.push_back(std::move(meta_if));

  for (const auto& constant : interface.GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();
    if (value.GetType() != AidlConstantValue::Type::STRING) continue;

    std::string cppType = CppNameOf(constant->GetType(), typenames);
    unique_ptr<MethodImpl> getter(new MethodImpl("const " + cppType + "&",
                                                 ClassName(interface, ClassNames::INTERFACE),
                                                 constant->GetName(), {}));
    getter->GetStatementBlock()->AddLiteral(
        StringPrintf("static const %s value(%s)", cppType.c_str(),
                     constant->ValueString(ConstantValueDecorator).c_str()));
    getter->GetStatementBlock()->AddLiteral("return value");
    decls.push_back(std::move(getter));
  }

  return unique_ptr<Document>{new CppSource{
      include_list,
      NestInNamespaces(std::move(decls), interface.GetSplitPackage())}};
}

unique_ptr<Document> BuildClientHeader(const AidlTypenames& typenames,
                                       const AidlInterface& interface, const Options& options) {
  const string i_name = ClassName(interface, ClassNames::INTERFACE);
  const string bp_name = ClassName(interface, ClassNames::CLIENT);

  vector<string> includes = {kIBinderHeader, kIInterfaceHeader, "utils/Errors.h",
                             HeaderFile(interface, ClassNames::RAW, false)};

  unique_ptr<ConstructorDecl> constructor{new ConstructorDecl{
      bp_name,
      ArgList{StringPrintf("const ::android::sp<::android::IBinder>& %s",
                           kImplVarName)},
      ConstructorDecl::IS_EXPLICIT
  }};
  unique_ptr<ConstructorDecl> destructor{new ConstructorDecl{
      "~" + bp_name,
      ArgList{},
      ConstructorDecl::IS_VIRTUAL | ConstructorDecl::IS_DEFAULT}};

  vector<unique_ptr<Declaration>> publics;
  publics.push_back(std::move(constructor));
  publics.push_back(std::move(destructor));

  for (const auto& method: interface.GetMethods()) {
    if (method->IsUserDefined()) {
      publics.push_back(BuildMethodDecl(*method, typenames, false));
    } else {
      publics.push_back(BuildMetaMethodDecl(*method, typenames, options, false));
    }
  }

  if (options.GenLog()) {
    includes.emplace_back("chrono");      // for std::chrono::steady_clock
    includes.emplace_back("functional");  // for std::function
    includes.emplace_back("json/value.h");
    publics.emplace_back(
        new LiteralDecl{"static std::function<void(const Json::Value&)> logFunc;\n"});
  }

  vector<unique_ptr<Declaration>> privates;

  if (options.Version() > 0) {
    privates.emplace_back(new LiteralDecl("int32_t cached_version_ = -1;\n"));
  }
  if (!options.Hash().empty()) {
    privates.emplace_back(new LiteralDecl("std::string cached_hash_ = \"-1\";\n"));
    privates.emplace_back(new LiteralDecl("std::mutex cached_hash_mutex_;\n"));
  }

  unique_ptr<ClassDecl> bp_class{new ClassDecl{
      bp_name,
      "::android::BpInterface<" + i_name + ">",
      std::move(publics),
      std::move(privates),
  }};

  return unique_ptr<Document>{
      new CppHeader{BuildHeaderGuard(interface, ClassNames::CLIENT), includes,
                    NestInNamespaces(std::move(bp_class), interface.GetSplitPackage())}};
}

unique_ptr<Document> BuildServerHeader(const AidlTypenames& /* typenames */,
                                       const AidlInterface& interface, const Options& options) {
  const string i_name = ClassName(interface, ClassNames::INTERFACE);
  const string bn_name = ClassName(interface, ClassNames::SERVER);

  unique_ptr<ConstructorDecl> constructor{
      new ConstructorDecl{bn_name, ArgList{}, ConstructorDecl::IS_EXPLICIT}};

  unique_ptr<Declaration> on_transact{new MethodDecl{
      kAndroidStatusLiteral, "onTransact",
      ArgList{{StringPrintf("uint32_t %s", kCodeVarName),
               StringPrintf("const %s& %s", kAndroidParcelLiteral,
                            kDataVarName),
               StringPrintf("%s* %s", kAndroidParcelLiteral, kReplyVarName),
               StringPrintf("uint32_t %s", kFlagsVarName)}},
      MethodDecl::IS_OVERRIDE
  }};
  vector<string> includes = {"binder/IInterface.h", HeaderFile(interface, ClassNames::RAW, false)};

  vector<unique_ptr<Declaration>> publics;
  publics.push_back(std::move(constructor));
  publics.push_back(std::move(on_transact));

  if (options.Version() > 0) {
    std::ostringstream code;
    code << "int32_t " << kGetInterfaceVersion << "() final override;\n";
    publics.emplace_back(new LiteralDecl(code.str()));
  }
  if (!options.Hash().empty()) {
    std::ostringstream code;
    code << "std::string " << kGetInterfaceHash << "();\n";
    publics.emplace_back(new LiteralDecl(code.str()));
  }

  if (options.GenLog()) {
    includes.emplace_back("chrono");      // for std::chrono::steady_clock
    includes.emplace_back("functional");  // for std::function
    includes.emplace_back("json/value.h");
    publics.emplace_back(
        new LiteralDecl{"static std::function<void(const Json::Value&)> logFunc;\n"});
  }
  unique_ptr<ClassDecl> bn_class{
      new ClassDecl{bn_name,
                    "::android::BnInterface<" + i_name + ">",
                    std::move(publics),
                    {}
      }};

  return unique_ptr<Document>{
      new CppHeader{BuildHeaderGuard(interface, ClassNames::SERVER), includes,
                    NestInNamespaces(std::move(bn_class), interface.GetSplitPackage())}};
}

unique_ptr<Document> BuildInterfaceHeader(const AidlTypenames& typenames,
                                          const AidlInterface& interface, const Options& options) {
  set<string> includes = {kIBinderHeader, kIInterfaceHeader, kStatusHeader, kStrongPointerHeader};

  for (const auto& method : interface.GetMethods()) {
    for (const auto& argument : method->GetArguments()) {
      AddHeaders(argument->GetType(), typenames, includes);
    }

    AddHeaders(method->GetType(), typenames, includes);
  }

  const string i_name = ClassName(interface, ClassNames::INTERFACE);
  unique_ptr<ClassDecl> if_class{new ClassDecl{i_name, "::android::IInterface"}};
  if_class->AddPublic(unique_ptr<Declaration>{new MacroDecl{
      "DECLARE_META_INTERFACE",
      ArgList{vector<string>{ClassName(interface, ClassNames::BASE)}}}});

  if (options.Version() > 0) {
    std::ostringstream code;
    code << "const int32_t VERSION = " << options.Version() << ";\n";

    if_class->AddPublic(unique_ptr<Declaration>(new LiteralDecl(code.str())));
  }
  if (!options.Hash().empty()) {
    std::ostringstream code;
    code << "const std::string HASH = \"" << options.Hash() << "\";\n";

    if_class->AddPublic(unique_ptr<Declaration>(new LiteralDecl(code.str())));
  }

  std::vector<std::unique_ptr<Declaration>> string_constants;
  unique_ptr<Enum> int_constant_enum{new Enum{"", "int32_t", false}};
  for (const auto& constant : interface.GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();

    switch (value.GetType()) {
      case AidlConstantValue::Type::STRING: {
        std::string cppType = CppNameOf(constant->GetType(), typenames);
        unique_ptr<Declaration> getter(new MethodDecl("const " + cppType + "&", constant->GetName(),
                                                      {}, MethodDecl::IS_STATIC));
        string_constants.push_back(std::move(getter));
        break;
      }
      case AidlConstantValue::Type::BOOLEAN:  // fall-through
      case AidlConstantValue::Type::INT8:     // fall-through
      case AidlConstantValue::Type::INT32: {
        int_constant_enum->AddValue(constant->GetName(),
                                    constant->ValueString(ConstantValueDecorator));
        break;
      }
      default: {
        LOG(FATAL) << "Unrecognized constant type: " << static_cast<int>(value.GetType());
      }
    }
  }
  if (int_constant_enum->HasValues()) {
    if_class->AddPublic(std::move(int_constant_enum));
  }
  if (!string_constants.empty()) {
    includes.insert(kString16Header);

    for (auto& string_constant : string_constants) {
      if_class->AddPublic(std::move(string_constant));
    }
  }

  if (options.GenTraces()) {
    includes.insert(kTraceHeader);
  }

  if (!interface.GetMethods().empty()) {
    for (const auto& method : interface.GetMethods()) {
      if (method->IsUserDefined()) {
        // Each method gets an enum entry and pure virtual declaration.
        if_class->AddPublic(BuildMethodDecl(*method, typenames, true));
      } else {
        if_class->AddPublic(BuildMetaMethodDecl(*method, typenames, options, true));
      }
    }
  }

  // Implement the default impl class.
  vector<unique_ptr<Declaration>> method_decls;
  // onAsBinder returns nullptr as this interface is not associated with a
  // real binder.
  method_decls.emplace_back(
      new LiteralDecl("::android::IBinder* onAsBinder() override {\n"
                      "  return nullptr;\n"
                      "}\n"));
  // Each interface method by default returns UNKNOWN_TRANSACTION with is
  // the same status that is returned by transact() when the method is
  // not implemented in the server side. In other words, these default
  // methods do nothing; they only exist to aid making a real default
  // impl class without having to override all methods in an interface.
  for (const auto& method : interface.GetMethods()) {
    if (method->IsUserDefined()) {
      std::ostringstream code;
      code << "::android::binder::Status " << method->GetName()
           << BuildArgList(typenames, *method, true, true).ToString() << " override {\n"
           << "  return ::android::binder::Status::fromStatusT(::android::UNKNOWN_TRANSACTION);\n"
           << "}\n";
      method_decls.emplace_back(new LiteralDecl(code.str()));
    } else {
      if (method->GetName() == kGetInterfaceVersion && options.Version() > 0) {
        std::ostringstream code;
        code << "int32_t " << kGetInterfaceVersion << "() override {\n"
             << "  return 0;\n"
             << "}\n";
        method_decls.emplace_back(new LiteralDecl(code.str()));
      }
      if (method->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
        std::ostringstream code;
        code << "std::string " << kGetInterfaceHash << "() override {\n"
             << "  return \"\";\n"
             << "}\n";
        method_decls.emplace_back(new LiteralDecl(code.str()));
      }
    }
  }

  vector<unique_ptr<Declaration>> decls;
  decls.emplace_back(std::move(if_class));
  decls.emplace_back(new ClassDecl{
      ClassName(interface, ClassNames::DEFAULT_IMPL), i_name, std::move(method_decls), {}});

  return unique_ptr<Document>{
      new CppHeader{BuildHeaderGuard(interface, ClassNames::INTERFACE),
                    vector<string>(includes.begin(), includes.end()),
                    NestInNamespaces(std::move(decls), interface.GetSplitPackage())}};
}

std::unique_ptr<Document> BuildParcelHeader(const AidlTypenames& typenames,
                                            const AidlStructuredParcelable& parcel,
                                            const Options&) {
  unique_ptr<ClassDecl> parcel_class{new ClassDecl{parcel.GetName(), "::android::Parcelable"}};

  set<string> includes = {kStatusHeader, kParcelHeader};
  includes.insert("tuple");
  for (const auto& variable : parcel.GetFields()) {
    AddHeaders(variable->GetType(), typenames, includes);
  }

  set<string> operators = {"<", ">", "==", ">=", "<=", "!="};
  for (const auto& op : operators) {
    std::ostringstream operator_code;
    std::vector<std::string> variable_name;
    std::vector<std::string> rhs_variable_name;
    for (const auto& variable : parcel.GetFields()) {
      variable_name.push_back(variable->GetName());
      rhs_variable_name.push_back("rhs." + variable->GetName());
    }

    operator_code << "inline bool operator" << op << "(const " << parcel.GetName()
                  << "& rhs) const {\n"
                  << "  return "
                  << "std::tie(" << Join(variable_name, ", ") << ")" << op << "std::tie("
                  << Join(rhs_variable_name, ", ") << ")"
                  << ";\n"
                  << "}\n";

    parcel_class->AddPublic(std::unique_ptr<LiteralDecl>(new LiteralDecl(operator_code.str())));
  }
  for (const auto& variable : parcel.GetFields()) {

    std::ostringstream out;
    std::string cppType = CppNameOf(variable->GetType(), typenames);
    out << cppType.c_str() << " " << variable->GetName().c_str();
    if (variable->GetDefaultValue()) {
      out << " = " << cppType.c_str() << "(" << variable->ValueString(ConstantValueDecorator)
          << ")";
    }
    out << ";\n";

    parcel_class->AddPublic(std::unique_ptr<LiteralDecl>(new LiteralDecl(out.str())));
  }

  unique_ptr<MethodDecl> read(new MethodDecl(kAndroidStatusLiteral, "readFromParcel",
                                             ArgList("const ::android::Parcel* _aidl_parcel"),
                                             MethodDecl::IS_OVERRIDE | MethodDecl::IS_FINAL));
  parcel_class->AddPublic(std::move(read));
  unique_ptr<MethodDecl> write(new MethodDecl(
      kAndroidStatusLiteral, "writeToParcel", ArgList("::android::Parcel* _aidl_parcel"),
      MethodDecl::IS_OVERRIDE | MethodDecl::IS_CONST | MethodDecl::IS_FINAL));
  parcel_class->AddPublic(std::move(write));

  return unique_ptr<Document>{new CppHeader{
      BuildHeaderGuard(parcel, ClassNames::RAW), vector<string>(includes.begin(), includes.end()),
      NestInNamespaces(std::move(parcel_class), parcel.GetSplitPackage())}};
}
std::unique_ptr<Document> BuildParcelSource(const AidlTypenames& typenames,
                                            const AidlStructuredParcelable& parcel,
                                            const Options&) {
  unique_ptr<MethodImpl> read{new MethodImpl{kAndroidStatusLiteral, parcel.GetName(),
                                             "readFromParcel",
                                             ArgList("const ::android::Parcel* _aidl_parcel")}};
  StatementBlock* read_block = read->GetStatementBlock();
  read_block->AddLiteral(
      StringPrintf("%s %s = %s", kAndroidStatusLiteral, kAndroidStatusVarName, kAndroidStatusOk));

  read_block->AddLiteral(
      "size_t _aidl_start_pos = _aidl_parcel->dataPosition();\n"
      "int32_t _aidl_parcelable_raw_size = _aidl_parcel->readInt32();\n"
      "if (_aidl_parcelable_raw_size < 0) return ::android::BAD_VALUE;\n"
      "size_t _aidl_parcelable_size = static_cast<size_t>(_aidl_parcelable_raw_size);\n");

  for (const auto& variable : parcel.GetFields()) {
    string method = ParcelReadMethodOf(variable->GetType(), typenames);

    read_block->AddStatement(new Assignment(
        kAndroidStatusVarName, new MethodCall(StringPrintf("_aidl_parcel->%s", method.c_str()),
                                              ParcelReadCastOf(variable->GetType(), typenames,
                                                               "&" + variable->GetName()))));
    read_block->AddStatement(ReturnOnStatusNotOk());
    read_block->AddLiteral(StringPrintf(
        "if (_aidl_parcel->dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) {\n"
        "  _aidl_parcel->setDataPosition(_aidl_start_pos + _aidl_parcelable_size);\n"
        "  return %s;\n"
        "}",
        kAndroidStatusVarName));
  }
  read_block->AddLiteral(StringPrintf("return %s", kAndroidStatusVarName));

  unique_ptr<MethodImpl> write{
      new MethodImpl{kAndroidStatusLiteral, parcel.GetName(), "writeToParcel",
                     ArgList("::android::Parcel* _aidl_parcel"), true /*const*/}};
  StatementBlock* write_block = write->GetStatementBlock();
  write_block->AddLiteral(
      StringPrintf("%s %s = %s", kAndroidStatusLiteral, kAndroidStatusVarName, kAndroidStatusOk));

  write_block->AddLiteral(
      "auto _aidl_start_pos = _aidl_parcel->dataPosition();\n"
      "_aidl_parcel->writeInt32(0);");

  for (const auto& variable : parcel.GetFields()) {
    string method = ParcelWriteMethodOf(variable->GetType(), typenames);
    write_block->AddStatement(new Assignment(
        kAndroidStatusVarName,
        new MethodCall(StringPrintf("_aidl_parcel->%s", method.c_str()),
                       ParcelWriteCastOf(variable->GetType(), typenames, variable->GetName()))));
    write_block->AddStatement(ReturnOnStatusNotOk());
  }

  write_block->AddLiteral(
      "auto _aidl_end_pos = _aidl_parcel->dataPosition();\n"
      "_aidl_parcel->setDataPosition(_aidl_start_pos);\n"
      "_aidl_parcel->writeInt32(_aidl_end_pos - _aidl_start_pos);\n"
      "_aidl_parcel->setDataPosition(_aidl_end_pos);");
  write_block->AddLiteral(StringPrintf("return %s", kAndroidStatusVarName));

  vector<unique_ptr<Declaration>> file_decls;
  file_decls.push_back(std::move(read));
  file_decls.push_back(std::move(write));

  set<string> includes = {};
  AddHeaders(parcel, includes);

  return unique_ptr<Document>{
      new CppSource{vector<string>(includes.begin(), includes.end()),
                    NestInNamespaces(std::move(file_decls), parcel.GetSplitPackage())}};
}

std::string GenerateEnumToString(const AidlTypenames& typenames,
                                 const AidlEnumDeclaration& enum_decl) {
  std::ostringstream code;
  code << "static inline std::string toString(" << enum_decl.GetName() << " val) {\n";
  code << "  switch(val) {\n";
  std::set<std::string> unique_cases;
  for (const auto& enumerator : enum_decl.GetEnumerators()) {
    std::string c = enumerator->ValueString(enum_decl.GetBackingType(), ConstantValueDecorator);
    // Only add a case if its value has not yet been used in the switch
    // statement. C++ does not allow multiple cases with the same value, but
    // enums does allow this. In this scenario, the first declared
    // enumerator with the given value is printed.
    if (unique_cases.count(c) == 0) {
      unique_cases.insert(c);
      code << "  case " << enum_decl.GetName() << "::" << enumerator->GetName() << ":\n";
      code << "    return \"" << enumerator->GetName() << "\";\n";
    }
  }
  code << "  default:\n";
  code << "    return std::to_string(static_cast<"
       << CppNameOf(enum_decl.GetBackingType(), typenames) << ">(val));\n";
  code << "  }\n";
  code << "}\n";
  return code.str();
}

std::unique_ptr<Document> BuildEnumHeader(const AidlTypenames& typenames,
                                          const AidlEnumDeclaration& enum_decl) {
  std::unique_ptr<Enum> generated_enum{
      new Enum{enum_decl.GetName(), CppNameOf(enum_decl.GetBackingType(), typenames), true}};
  for (const auto& enumerator : enum_decl.GetEnumerators()) {
    generated_enum->AddValue(
        enumerator->GetName(),
        enumerator->ValueString(enum_decl.GetBackingType(), ConstantValueDecorator));
  }

  std::set<std::string> includes = {
      "array",
      "binder/Enums.h",
      "string",
  };
  AddHeaders(enum_decl.GetBackingType(), typenames, includes);

  std::vector<std::unique_ptr<Declaration>> decls1;
  decls1.push_back(std::move(generated_enum));
  decls1.push_back(std::make_unique<LiteralDecl>(GenerateEnumToString(typenames, enum_decl)));

  std::vector<std::unique_ptr<Declaration>> decls2;
  decls2.push_back(std::make_unique<LiteralDecl>(GenerateEnumValues(enum_decl, {""})));

  return unique_ptr<Document>{
      new CppHeader{BuildHeaderGuard(enum_decl, ClassNames::RAW),
                    vector<string>(includes.begin(), includes.end()),
                    Append(NestInNamespaces(std::move(decls1), enum_decl.GetSplitPackage()),
                           NestInNamespaces(std::move(decls2), {"android", "internal"}))}};
}

bool WriteHeader(const Options& options, const AidlTypenames& typenames,
                 const AidlInterface& interface, const IoDelegate& io_delegate,
                 ClassNames header_type) {
  unique_ptr<Document> header;
  switch (header_type) {
    case ClassNames::INTERFACE:
      header = BuildInterfaceHeader(typenames, interface, options);
      header_type = ClassNames::RAW;
      break;
    case ClassNames::CLIENT:
      header = BuildClientHeader(typenames, interface, options);
      break;
    case ClassNames::SERVER:
      header = BuildServerHeader(typenames, interface, options);
      break;
    default:
      LOG(FATAL) << "aidl internal error";
  }
  if (!header) {
    LOG(ERROR) << "aidl internal error: Failed to generate header.";
    return false;
  }

  const string header_path = options.OutputHeaderDir() + HeaderFile(interface, header_type);
  unique_ptr<CodeWriter> code_writer(io_delegate.GetCodeWriter(header_path));
  header->Write(code_writer.get());

  const bool success = code_writer->Close();
  if (!success) {
    io_delegate.RemovePath(header_path);
  }

  return success;
}

}  // namespace internals

using namespace internals;

bool GenerateCppInterface(const string& output_file, const Options& options,
                          const AidlTypenames& typenames, const AidlInterface& interface,
                          const IoDelegate& io_delegate) {
  auto interface_src = BuildInterfaceSource(typenames, interface, options);
  auto client_src = BuildClientSource(typenames, interface, options);
  auto server_src = BuildServerSource(typenames, interface, options);

  if (!interface_src || !client_src || !server_src) {
    return false;
  }

  if (!WriteHeader(options, typenames, interface, io_delegate, ClassNames::INTERFACE) ||
      !WriteHeader(options, typenames, interface, io_delegate, ClassNames::CLIENT) ||
      !WriteHeader(options, typenames, interface, io_delegate, ClassNames::SERVER)) {
    return false;
  }

  unique_ptr<CodeWriter> writer = io_delegate.GetCodeWriter(output_file);
  interface_src->Write(writer.get());
  client_src->Write(writer.get());
  server_src->Write(writer.get());

  const bool success = writer->Close();
  if (!success) {
    io_delegate.RemovePath(output_file);
  }

  return success;
}

bool GenerateCppParcel(const string& output_file, const Options& options,
                       const AidlTypenames& typenames, const AidlStructuredParcelable& parcelable,
                       const IoDelegate& io_delegate) {
  auto header = BuildParcelHeader(typenames, parcelable, options);
  auto source = BuildParcelSource(typenames, parcelable, options);

  if (!header || !source) {
    return false;
  }

  const string header_path = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::RAW);
  unique_ptr<CodeWriter> header_writer(io_delegate.GetCodeWriter(header_path));
  header->Write(header_writer.get());
  CHECK(header_writer->Close());

  // TODO(b/111362593): no unecessary files just to have consistent output with interfaces
  const string bp_header = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  bp_writer->Write("#error TODO(b/111362593) parcelables do not have bp classes");
  CHECK(bp_writer->Close());
  const string bn_header = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  bn_writer->Write("#error TODO(b/111362593) parcelables do not have bn classes");
  CHECK(bn_writer->Close());

  unique_ptr<CodeWriter> source_writer = io_delegate.GetCodeWriter(output_file);
  source->Write(source_writer.get());
  CHECK(source_writer->Close());

  return true;
}

bool GenerateCppParcelDeclaration(const std::string& filename, const Options& options,
                                  const AidlParcelable& parcelable, const IoDelegate& io_delegate) {
  CodeWriterPtr source_writer = io_delegate.GetCodeWriter(filename);
  *source_writer
      << "// This file is intentionally left blank as placeholder for parcel declaration.\n";
  CHECK(source_writer->Close());

  // TODO(b/111362593): no unecessary files just to have consistent output with interfaces
  const string header_path = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::RAW);
  unique_ptr<CodeWriter> header_writer(io_delegate.GetCodeWriter(header_path));
  header_writer->Write("#error TODO(b/111362593) parcelables do not have headers");
  CHECK(header_writer->Close());
  const string bp_header = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  bp_writer->Write("#error TODO(b/111362593) parcelables do not have bp classes");
  CHECK(bp_writer->Close());
  const string bn_header = options.OutputHeaderDir() + HeaderFile(parcelable, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  bn_writer->Write("#error TODO(b/111362593) parcelables do not have bn classes");
  CHECK(bn_writer->Close());

  return true;
}

bool GenerateCppEnumDeclaration(const std::string& filename, const Options& options,
                                const AidlTypenames& typenames,
                                const AidlEnumDeclaration& enum_decl,
                                const IoDelegate& io_delegate) {
  auto header = BuildEnumHeader(typenames, enum_decl);
  if (!header) return false;

  const string header_path = options.OutputHeaderDir() + HeaderFile(enum_decl, ClassNames::RAW);
  unique_ptr<CodeWriter> header_writer(io_delegate.GetCodeWriter(header_path));
  header->Write(header_writer.get());
  CHECK(header_writer->Close());

  // TODO(b/111362593): no unnecessary files just to have consistent output with interfaces
  CodeWriterPtr source_writer = io_delegate.GetCodeWriter(filename);
  *source_writer
      << "// This file is intentionally left blank as placeholder for enum declaration.\n";
  CHECK(source_writer->Close());
  const string bp_header = options.OutputHeaderDir() + HeaderFile(enum_decl, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  bp_writer->Write("#error TODO(b/111362593) enums do not have bp classes");
  CHECK(bp_writer->Close());
  const string bn_header = options.OutputHeaderDir() + HeaderFile(enum_decl, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  bn_writer->Write("#error TODO(b/111362593) enums do not have bn classes");
  CHECK(bn_writer->Close());

  return true;
}

bool GenerateCpp(const string& output_file, const Options& options, const AidlTypenames& typenames,
                 const AidlDefinedType& defined_type, const IoDelegate& io_delegate) {
  const AidlStructuredParcelable* parcelable = defined_type.AsStructuredParcelable();
  if (parcelable != nullptr) {
    return GenerateCppParcel(output_file, options, typenames, *parcelable, io_delegate);
  }

  const AidlParcelable* parcelable_decl = defined_type.AsParcelable();
  if (parcelable_decl != nullptr) {
    return GenerateCppParcelDeclaration(output_file, options, *parcelable_decl, io_delegate);
  }

  const AidlEnumDeclaration* enum_decl = defined_type.AsEnumDeclaration();
  if (enum_decl != nullptr) {
    return GenerateCppEnumDeclaration(output_file, options, typenames, *enum_decl, io_delegate);
  }

  const AidlInterface* interface = defined_type.AsInterface();
  if (interface != nullptr) {
    return GenerateCppInterface(output_file, options, typenames, *interface, io_delegate);
  }

  CHECK(false) << "Unrecognized type sent for cpp generation.";
  return false;
}

}  // namespace cpp
}  // namespace aidl
}  // namespace android
