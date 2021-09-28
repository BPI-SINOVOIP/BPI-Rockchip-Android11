/*
 * Copyright (C) 2018, The Android Open Source Project
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

#include "generate_ndk.h"

#include "aidl.h"
#include "aidl_language.h"
#include "aidl_to_cpp_common.h"
#include "aidl_to_ndk.h"

#include <android-base/logging.h>

namespace android {
namespace aidl {
namespace ndk {

static constexpr const char* kClazz = "_g_aidl_clazz";
static constexpr const char* kDescriptor = "descriptor";
static constexpr const char* kVersion = "version";
static constexpr const char* kHash = "hash";
static constexpr const char* kCachedVersion = "_aidl_cached_version";
static constexpr const char* kCachedHash = "_aidl_cached_hash";
static constexpr const char* kCachedHashMutex = "_aidl_cached_hash_mutex";

using namespace internals;
using cpp::ClassNames;

void GenerateNdkInterface(const string& output_file, const Options& options,
                          const AidlTypenames& types, const AidlInterface& defined_type,
                          const IoDelegate& io_delegate) {
  const string i_header = options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::RAW);
  unique_ptr<CodeWriter> i_writer(io_delegate.GetCodeWriter(i_header));
  GenerateInterfaceHeader(*i_writer, types, defined_type, options);
  CHECK(i_writer->Close());

  const string bp_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  GenerateClientHeader(*bp_writer, types, defined_type, options);
  CHECK(bp_writer->Close());

  const string bn_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  GenerateServerHeader(*bn_writer, types, defined_type, options);
  CHECK(bn_writer->Close());

  unique_ptr<CodeWriter> source_writer = io_delegate.GetCodeWriter(output_file);
  GenerateSource(*source_writer, types, defined_type, options);
  CHECK(source_writer->Close());
}

void GenerateNdkParcel(const string& output_file, const Options& options,
                       const AidlTypenames& types, const AidlStructuredParcelable& defined_type,
                       const IoDelegate& io_delegate) {
  const string header_path =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::RAW);
  unique_ptr<CodeWriter> header_writer(io_delegate.GetCodeWriter(header_path));
  GenerateParcelHeader(*header_writer, types, defined_type, options);
  CHECK(header_writer->Close());

  const string bp_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  *bp_writer << "#error TODO(b/111362593) defined_types do not have bp classes\n";
  CHECK(bp_writer->Close());

  const string bn_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  *bn_writer << "#error TODO(b/111362593) defined_types do not have bn classes\n";
  CHECK(bn_writer->Close());

  unique_ptr<CodeWriter> source_writer = io_delegate.GetCodeWriter(output_file);
  GenerateParcelSource(*source_writer, types, defined_type, options);
  CHECK(source_writer->Close());
}

void GenerateNdkParcelDeclaration(const std::string& filename, const IoDelegate& io_delegate) {
  CodeWriterPtr code_writer = io_delegate.GetCodeWriter(filename);
  *code_writer
      << "// This file is intentionally left blank as placeholder for parcel declaration.\n";
  CHECK(code_writer->Close());
}

void GenerateNdkEnumDeclaration(const string& output_file, const Options& options,
                                const AidlTypenames& types, const AidlEnumDeclaration& defined_type,
                                const IoDelegate& io_delegate) {
  const string header_path =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::RAW);
  unique_ptr<CodeWriter> header_writer(io_delegate.GetCodeWriter(header_path));
  GenerateEnumHeader(*header_writer, types, defined_type, options);
  CHECK(header_writer->Close());

  const string bp_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::CLIENT);
  unique_ptr<CodeWriter> bp_writer(io_delegate.GetCodeWriter(bp_header));
  *bp_writer << "#error TODO(b/111362593) enums do not have bp classes\n";
  CHECK(bp_writer->Close());

  const string bn_header =
      options.OutputHeaderDir() + NdkHeaderFile(defined_type, ClassNames::SERVER);
  unique_ptr<CodeWriter> bn_writer(io_delegate.GetCodeWriter(bn_header));
  *bn_writer << "#error TODO(b/111362593) enums do not have bn classes\n";
  CHECK(bn_writer->Close());

  unique_ptr<CodeWriter> source_writer = io_delegate.GetCodeWriter(output_file);
  *source_writer
      << "// This file is intentionally left blank as placeholder for enum declaration.\n";
  CHECK(source_writer->Close());
}

void GenerateNdk(const string& output_file, const Options& options, const AidlTypenames& types,
                 const AidlDefinedType& defined_type, const IoDelegate& io_delegate) {
  if (const AidlStructuredParcelable* parcelable = defined_type.AsStructuredParcelable();
      parcelable != nullptr) {
    GenerateNdkParcel(output_file, options, types, *parcelable, io_delegate);
    return;
  }

  if (const AidlParcelable* parcelable_decl = defined_type.AsParcelable();
      parcelable_decl != nullptr) {
    GenerateNdkParcelDeclaration(output_file, io_delegate);
    return;
  }

  if (const AidlEnumDeclaration* enum_decl = defined_type.AsEnumDeclaration();
      enum_decl != nullptr) {
    GenerateNdkEnumDeclaration(output_file, options, types, *enum_decl, io_delegate);
    return;
  }

  if (const AidlInterface* interface = defined_type.AsInterface(); interface != nullptr) {
    GenerateNdkInterface(output_file, options, types, *interface, io_delegate);
    return;
  }

  CHECK(false) << "Unrecognized type sent for NDK cpp generation.";
}
namespace internals {

void EnterNdkNamespace(CodeWriter& out, const AidlDefinedType& defined_type) {
  out << "namespace aidl {\n";
  cpp::EnterNamespace(out, defined_type);
}
void LeaveNdkNamespace(CodeWriter& out, const AidlDefinedType& defined_type) {
  cpp::LeaveNamespace(out, defined_type);
  out << "}  // namespace aidl\n";
}

static void StatusCheckGoto(CodeWriter& out) {
  out << "if (_aidl_ret_status != STATUS_OK) goto _aidl_error;\n\n";
}
static void StatusCheckBreak(CodeWriter& out) {
  out << "if (_aidl_ret_status != STATUS_OK) break;\n\n";
}
static void StatusCheckReturn(CodeWriter& out) {
  out << "if (_aidl_ret_status != STATUS_OK) return _aidl_ret_status;\n\n";
}

static void GenerateHeaderIncludes(CodeWriter& out, const AidlTypenames& types,
                                   const AidlDefinedType& defined_type) {
  out << "#include <cstdint>\n";
  out << "#include <memory>\n";
  out << "#include <optional>\n";
  out << "#include <string>\n";
  out << "#include <vector>\n";
  out << "#ifdef BINDER_STABILITY_SUPPORT\n";
  out << "#include <android/binder_stability.h>\n";
  out << "#endif  // BINDER_STABILITY_SUPPORT\n";

  types.IterateTypes([&](const AidlDefinedType& other_defined_type) {
    if (&other_defined_type == &defined_type) return;

    if (other_defined_type.AsInterface() != nullptr) {
      out << "#include <"
          << NdkHeaderFile(other_defined_type, ClassNames::RAW, false /*use_os_sep*/) << ">\n";
    } else if (other_defined_type.AsStructuredParcelable() != nullptr) {
      out << "#include <"
          << NdkHeaderFile(other_defined_type, ClassNames::RAW, false /*use_os_sep*/) << ">\n";
    } else if (other_defined_type.AsParcelable() != nullptr) {
      out << "#include \"" << other_defined_type.AsParcelable()->GetCppHeader() << "\"\n";
    } else if (other_defined_type.AsEnumDeclaration() != nullptr) {
      out << "#include <"
          << NdkHeaderFile(other_defined_type, ClassNames::RAW, false /*use_os_sep*/) << ">\n";
    } else {
      AIDL_FATAL(defined_type) << "Unrecognized type.";
    }
  });
}
static void GenerateSourceIncludes(CodeWriter& out, const AidlTypenames& types,
                                   const AidlDefinedType& /*defined_type*/) {
  out << "#include <android/binder_parcel_utils.h>\n";

  types.IterateTypes([&](const AidlDefinedType& a_defined_type) {
    if (a_defined_type.AsInterface() != nullptr) {
      out << "#include <" << NdkHeaderFile(a_defined_type, ClassNames::CLIENT, false /*use_os_sep*/)
          << ">\n";
      out << "#include <" << NdkHeaderFile(a_defined_type, ClassNames::SERVER, false /*use_os_sep*/)
          << ">\n";
      out << "#include <" << NdkHeaderFile(a_defined_type, ClassNames::RAW, false /*use_os_sep*/)
          << ">\n";
    }
  });
}

static void GenerateConstantDeclarations(CodeWriter& out, const AidlInterface& interface) {
  for (const auto& constant : interface.GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();
    CHECK(value.GetType() != AidlConstantValue::Type::UNARY &&
          value.GetType() != AidlConstantValue::Type::BINARY);
    if (value.GetType() == AidlConstantValue::Type::STRING) {
      out << "static const char* " << constant->GetName() << ";\n";
    }
  }
  out << "\n";

  bool hasIntegralConstant = false;
  for (const auto& constant : interface.GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();
    CHECK(value.GetType() != AidlConstantValue::Type::UNARY &&
          value.GetType() != AidlConstantValue::Type::BINARY);
    if (value.GetType() == AidlConstantValue::Type::BOOLEAN ||
        value.GetType() == AidlConstantValue::Type::INT8 ||
        value.GetType() == AidlConstantValue::Type::INT32) {
      hasIntegralConstant = true;
      break;
    }
  }

  if (hasIntegralConstant) {
    out << "enum : int32_t {\n";
    out.Indent();
    for (const auto& constant : interface.GetConstantDeclarations()) {
      const AidlConstantValue& value = constant->GetValue();
      if (value.GetType() == AidlConstantValue::Type::BOOLEAN ||
          value.GetType() == AidlConstantValue::Type::INT8 ||
          value.GetType() == AidlConstantValue::Type::INT32) {
        out << constant->GetName() << " = " << constant->ValueString(ConstantValueDecorator)
            << ",\n";
      }
    }
    out.Dedent();
    out << "};\n";
  }
}
static void GenerateConstantDefinitions(CodeWriter& out, const AidlInterface& interface) {
  const std::string clazz = ClassName(interface, ClassNames::INTERFACE);

  for (const auto& constant : interface.GetConstantDeclarations()) {
    const AidlConstantValue& value = constant->GetValue();
    CHECK(value.GetType() != AidlConstantValue::Type::UNARY &&
          value.GetType() != AidlConstantValue::Type::BINARY);
    if (value.GetType() == AidlConstantValue::Type::STRING) {
      out << "const char* " << clazz << "::" << constant->GetName() << " = "
          << constant->ValueString(ConstantValueDecorator) << ";\n";
    }
  }
}

void GenerateSource(CodeWriter& out, const AidlTypenames& types, const AidlInterface& defined_type,
                    const Options& options) {
  GenerateSourceIncludes(out, types, defined_type);
  out << "\n";

  EnterNdkNamespace(out, defined_type);
  GenerateClassSource(out, types, defined_type, options);
  GenerateClientSource(out, types, defined_type, options);
  GenerateServerSource(out, types, defined_type, options);
  GenerateInterfaceSource(out, types, defined_type, options);
  LeaveNdkNamespace(out, defined_type);
}

static std::string MethodId(const AidlMethod& m) {
  return "(FIRST_CALL_TRANSACTION + " + std::to_string(m.GetId()) + " /*" + m.GetName() + "*/)";
}

static void GenerateClientMethodDefinition(CodeWriter& out, const AidlTypenames& types,
                                           const AidlInterface& defined_type,
                                           const AidlMethod& method,
                                           const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::CLIENT);

  out << NdkMethodDecl(types, method, clazz) << " {\n";
  out.Indent();
  out << "binder_status_t _aidl_ret_status = STATUS_OK;\n";
  out << "::ndk::ScopedAStatus _aidl_status;\n";

  if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
    out << "const std::lock_guard<std::mutex> lock(" << kCachedHashMutex << ");\n";
    out << "if (" << kCachedHash << " != \"-1\") {\n";
    out.Indent();
    out << "*_aidl_return = " << kCachedHash << ";\n"
        << "_aidl_status.set(AStatus_fromStatus(_aidl_ret_status));\n"
        << "return _aidl_status;\n";
    out.Dedent();
    out << "}\n";
  } else if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
    out << "if (" << kCachedVersion << " != -1) {\n";
    out.Indent();
    out << "*_aidl_return = " << kCachedVersion << ";\n"
        << "_aidl_status.set(AStatus_fromStatus(_aidl_ret_status));\n"
        << "return _aidl_status;\n";
    out.Dedent();
    out << "}\n";
  }
  out << "::ndk::ScopedAParcel _aidl_in;\n";
  out << "::ndk::ScopedAParcel _aidl_out;\n";
  out << "\n";

  if (options.GenLog()) {
    out << cpp::GenLogBeforeExecute(ClassName(defined_type, ClassNames::CLIENT), method,
                                    false /* isServer */, true /* isNdk */);
  }

  out << "_aidl_ret_status = AIBinder_prepareTransaction(asBinder().get(), _aidl_in.getR());\n";
  StatusCheckGoto(out);

  for (const auto& arg : method.GetArguments()) {
    const std::string var_name = cpp::BuildVarName(*arg);

    if (arg->IsIn()) {
      out << "_aidl_ret_status = ";
      const std::string prefix = (arg->IsOut() ? "*" : "");
      WriteToParcelFor({out, types, arg->GetType(), "_aidl_in.get()", prefix + var_name});
      out << ";\n";
      StatusCheckGoto(out);
    } else if (arg->IsOut() && arg->GetType().IsArray()) {
      out << "_aidl_ret_status = ::ndk::AParcel_writeVectorSize(_aidl_in.get(), *" << var_name
          << ");\n";
    }
  }
  out << "_aidl_ret_status = AIBinder_transact(\n";
  out.Indent();
  out << "asBinder().get(),\n";
  out << MethodId(method) << ",\n";
  out << "_aidl_in.getR(),\n";
  out << "_aidl_out.getR(),\n";
  out << (method.IsOneway() ? "FLAG_ONEWAY" : "0") << "\n";
  out << "#ifdef BINDER_STABILITY_SUPPORT\n";
  out << "| FLAG_PRIVATE_LOCAL\n";
  out << "#endif  // BINDER_STABILITY_SUPPORT\n";
  out << ");\n";
  out.Dedent();

  // If the method is not implmented in the server side but the client has
  // provided the default implementation, call it instead of failing hard.
  const std::string iface = ClassName(defined_type, ClassNames::INTERFACE);
  out << "if (_aidl_ret_status == STATUS_UNKNOWN_TRANSACTION && ";
  out << iface << "::getDefaultImpl()) {\n";
  out.Indent();
  out << "return " << iface << "::getDefaultImpl()->" << method.GetName() << "(";
  out << NdkArgList(types, method, FormatArgNameOnly) << ");\n";
  out.Dedent();
  out << "}\n";

  StatusCheckGoto(out);

  if (!method.IsOneway()) {
    out << "_aidl_ret_status = AParcel_readStatusHeader(_aidl_out.get(), _aidl_status.getR());\n";
    StatusCheckGoto(out);

    out << "if (!AStatus_isOk(_aidl_status.get())) return _aidl_status;\n\n";
  }

  if (method.GetType().GetName() != "void") {
    out << "_aidl_ret_status = ";
    ReadFromParcelFor({out, types, method.GetType(), "_aidl_out.get()", "_aidl_return"});
    out << ";\n";
    StatusCheckGoto(out);
    if (method.GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      out << kCachedHash << " = *_aidl_return;\n";
    } else if (method.GetName() == kGetInterfaceVersion && options.Version() > 0) {
      out << kCachedVersion << " = *_aidl_return;\n";
    }
  }
  for (const AidlArgument* arg : method.GetOutArguments()) {
    out << "_aidl_ret_status = ";
    ReadFromParcelFor({out, types, arg->GetType(), "_aidl_out.get()", cpp::BuildVarName(*arg)});
    out << ";\n";
    StatusCheckGoto(out);
  }

  out << "_aidl_error:\n";
  out << "_aidl_status.set(AStatus_fromStatus(_aidl_ret_status));\n";
  if (options.GenLog()) {
    out << cpp::GenLogAfterExecute(ClassName(defined_type, ClassNames::CLIENT), defined_type,
                                   method, "_aidl_status", "_aidl_return", false /* isServer */,
                                   true /* isNdk */);
  }
  out << "return _aidl_status;\n";
  out.Dedent();
  out << "}\n";
}

static void GenerateServerCaseDefinition(CodeWriter& out, const AidlTypenames& types,
                                         const AidlInterface& defined_type,
                                         const AidlMethod& method, const Options& options) {
  out << "case " << MethodId(method) << ": {\n";
  out.Indent();
  for (const auto& arg : method.GetArguments()) {
    out << NdkNameOf(types, arg->GetType(), StorageMode::STACK) << " " << cpp::BuildVarName(*arg)
        << ";\n";
  }
  if (method.GetType().GetName() != "void") {
    out << NdkNameOf(types, method.GetType(), StorageMode::STACK) << " _aidl_return;\n";
  }
  out << "\n";

  for (const auto& arg : method.GetArguments()) {
    const std::string var_name = cpp::BuildVarName(*arg);

    if (arg->IsIn()) {
      out << "_aidl_ret_status = ";
      ReadFromParcelFor({out, types, arg->GetType(), "_aidl_in", "&" + var_name});
      out << ";\n";
      StatusCheckBreak(out);
    } else if (arg->IsOut() && arg->GetType().IsArray()) {
      out << "_aidl_ret_status = ::ndk::AParcel_resizeVector(_aidl_in, &" << var_name << ");\n";
    }
  }
  if (options.GenLog()) {
    out << cpp::GenLogBeforeExecute(ClassName(defined_type, ClassNames::SERVER), method,
                                    true /* isServer */, true /* isNdk */);
  }
  out << "::ndk::ScopedAStatus _aidl_status = _aidl_impl->" << method.GetName() << "("
      << NdkArgList(types, method, FormatArgForCall) << ");\n";

  if (options.GenLog()) {
    out << cpp::GenLogAfterExecute(ClassName(defined_type, ClassNames::SERVER), defined_type,
                                   method, "_aidl_status", "_aidl_return", true /* isServer */,
                                   true /* isNdk */);
  }
  if (method.IsOneway()) {
    // For a oneway transaction, the kernel will have already returned a result. This is for the
    // in-process case when a oneway transaction is parceled/unparceled in the same process.
    out << "_aidl_ret_status = STATUS_OK;\n";
  } else {
    out << "_aidl_ret_status = AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());\n";
    StatusCheckBreak(out);

    out << "if (!AStatus_isOk(_aidl_status.get())) break;\n\n";

    if (method.GetType().GetName() != "void") {
      out << "_aidl_ret_status = ";
      WriteToParcelFor({out, types, method.GetType(), "_aidl_out", "_aidl_return"});
      out << ";\n";
      StatusCheckBreak(out);
    }
    for (const AidlArgument* arg : method.GetOutArguments()) {
      out << "_aidl_ret_status = ";
      WriteToParcelFor({out, types, arg->GetType(), "_aidl_out", cpp::BuildVarName(*arg)});
      out << ";\n";
      StatusCheckBreak(out);
    }
  }
  out << "break;\n";
  out.Dedent();
  out << "}\n";
}

void GenerateClassSource(CodeWriter& out, const AidlTypenames& types,
                         const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::INTERFACE);
  const std::string bn_clazz = ClassName(defined_type, ClassNames::SERVER);

  out << "static binder_status_t "
      << "_aidl_onTransact"
      << "(AIBinder* _aidl_binder, transaction_code_t _aidl_code, const AParcel* _aidl_in, "
         "AParcel* _aidl_out) {\n";
  out.Indent();
  out << "(void)_aidl_in;\n";
  out << "(void)_aidl_out;\n";
  out << "binder_status_t _aidl_ret_status = STATUS_UNKNOWN_TRANSACTION;\n";
  if (!defined_type.GetMethods().empty()) {
    // we know this cast is valid because this method is only called by the ICInterface
    // AIBinder_Class object which is associated with this class.
    out << "std::shared_ptr<" << bn_clazz << "> _aidl_impl = std::static_pointer_cast<" << bn_clazz
        << ">(::ndk::ICInterface::asInterface(_aidl_binder));\n";
    out << "switch (_aidl_code) {\n";
    out.Indent();
    for (const auto& method : defined_type.GetMethods()) {
      GenerateServerCaseDefinition(out, types, defined_type, *method, options);
    }
    out.Dedent();
    out << "}\n";
  } else {
    out << "(void)_aidl_binder;\n";
    out << "(void)_aidl_code;\n";
  }
  out << "return _aidl_ret_status;\n";
  out.Dedent();
  out << "}\n\n";

  out << "static AIBinder_Class* " << kClazz << " = ::ndk::ICInterface::defineClass(" << clazz
      << "::" << kDescriptor << ", _aidl_onTransact);\n\n";
}
void GenerateClientSource(CodeWriter& out, const AidlTypenames& types,
                          const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::CLIENT);

  out << clazz << "::" << clazz << "(const ::ndk::SpAIBinder& binder) : BpCInterface(binder) {}\n";
  out << clazz << "::~" << clazz << "() {}\n";
  if (options.GenLog()) {
    out << "std::function<void(const Json::Value&)> " << clazz << "::logFunc;\n";
  }
  out << "\n";
  for (const auto& method : defined_type.GetMethods()) {
    GenerateClientMethodDefinition(out, types, defined_type, *method, options);
  }
}
void GenerateServerSource(CodeWriter& out, const AidlTypenames& types,
                          const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::SERVER);
  const std::string iface = ClassName(defined_type, ClassNames::INTERFACE);

  out << "// Source for " << clazz << "\n";
  out << clazz << "::" << clazz << "() {}\n";
  out << clazz << "::~" << clazz << "() {}\n";
  if (options.GenLog()) {
    out << "std::function<void(const Json::Value&)> " << clazz << "::logFunc;\n";
  }
  out << "::ndk::SpAIBinder " << clazz << "::createBinder() {\n";
  out.Indent();
  out << "AIBinder* binder = AIBinder_new(" << kClazz << ", static_cast<void*>(this));\n";

  out << "#ifdef BINDER_STABILITY_SUPPORT\n";
  if (defined_type.IsVintfStability()) {
    out << "AIBinder_markVintfStability(binder);\n";
  } else {
    out << "AIBinder_markCompilationUnitStability(binder);\n";
  }
  out << "#endif  // BINDER_STABILITY_SUPPORT\n";

  out << "return ::ndk::SpAIBinder(binder);\n";
  out.Dedent();
  out << "}\n";

  // Implement the meta methods
  for (const auto& method : defined_type.GetMethods()) {
    if (method->IsUserDefined()) {
      continue;
    }
    if (method->GetName() == kGetInterfaceVersion && options.Version() > 0) {
      out << NdkMethodDecl(types, *method, clazz) << " {\n";
      out.Indent();
      out << "*_aidl_return = " << iface << "::" << kVersion << ";\n";
      out << "return ::ndk::ScopedAStatus(AStatus_newOk());\n";
      out.Dedent();
      out << "}\n";
    }
    if (method->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      out << NdkMethodDecl(types, *method, clazz) << " {\n";
      out.Indent();
      out << "*_aidl_return = " << iface << "::" << kHash << ";\n";
      out << "return ::ndk::ScopedAStatus(AStatus_newOk());\n";
      out.Dedent();
      out << "}\n";
    }
  }
}
void GenerateInterfaceSource(CodeWriter& out, const AidlTypenames& types,
                             const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::INTERFACE);
  const std::string bp_clazz = ClassName(defined_type, ClassNames::CLIENT);

  out << "// Source for " << clazz << "\n";
  out << "const char* " << clazz << "::" << kDescriptor << " = \""
      << defined_type.GetCanonicalName() << "\";\n";
  out << clazz << "::" << clazz << "() {}\n";
  out << clazz << "::~" << clazz << "() {}\n";
  out << "\n";
  GenerateConstantDefinitions(out, defined_type);
  out << "\n";

  out << "std::shared_ptr<" << clazz << "> " << clazz
      << "::fromBinder(const ::ndk::SpAIBinder& binder) {\n";
  out.Indent();
  out << "if (!AIBinder_associateClass(binder.get(), " << kClazz << ")) { return nullptr; }\n";
  out << "std::shared_ptr<::ndk::ICInterface> interface = "
         "::ndk::ICInterface::asInterface(binder.get());\n";
  out << "if (interface) {\n";
  out.Indent();
  out << "return std::static_pointer_cast<" << clazz << ">(interface);\n";
  out.Dedent();
  out << "}\n";
  out << "return ::ndk::SharedRefBase::make<" << bp_clazz << ">(binder);\n";
  out.Dedent();
  out << "}\n\n";

  out << "binder_status_t " << clazz << "::writeToParcel(AParcel* parcel, const std::shared_ptr<"
      << clazz << ">& instance) {\n";
  out.Indent();
  out << "return AParcel_writeStrongBinder(parcel, instance ? instance->asBinder().get() : "
         "nullptr);\n";
  out.Dedent();
  out << "}\n";

  out << "binder_status_t " << clazz << "::readFromParcel(const AParcel* parcel, std::shared_ptr<"
      << clazz << ">* instance) {\n";
  out.Indent();
  out << "::ndk::SpAIBinder binder;\n";
  out << "binder_status_t status = AParcel_readStrongBinder(parcel, binder.getR());\n";
  out << "if (status != STATUS_OK) return status;\n";
  out << "*instance = " << clazz << "::fromBinder(binder);\n";
  out << "return STATUS_OK;\n";
  out.Dedent();
  out << "}\n";

  // defintion for static member setDefaultImpl
  out << "bool " << clazz << "::setDefaultImpl(std::shared_ptr<" << clazz << "> impl) {\n";
  out.Indent();
  out << "// Only one user of this interface can use this function\n";
  out << "// at a time. This is a heuristic to detect if two different\n";
  out << "// users in the same process use this function.\n";
  out << "assert(!" << clazz << "::default_impl);\n";
  out << "if (impl) {\n";
  out.Indent();
  out << clazz << "::default_impl = impl;\n";
  out << "return true;\n";
  out.Dedent();
  out << "}\n";
  out << "return false;\n";
  out.Dedent();
  out << "}\n";

  // definition for static member getDefaultImpl
  out << "const std::shared_ptr<" << clazz << ">& " << clazz << "::getDefaultImpl() {\n";
  out.Indent();
  out << "return " << clazz << "::default_impl;\n";
  out.Dedent();
  out << "}\n";

  // definition for the static field default_impl
  out << "std::shared_ptr<" << clazz << "> " << clazz << "::default_impl = nullptr;\n";

  // default implementation for the <Name>Default class members
  const std::string defaultClazz = clazz + "Default";
  for (const auto& method : defined_type.GetMethods()) {
    if (method->IsUserDefined()) {
      out << "::ndk::ScopedAStatus " << defaultClazz << "::" << method->GetName() << "("
          << NdkArgList(types, *method, FormatArgNameUnused) << ") {\n";
      out.Indent();
      out << "::ndk::ScopedAStatus _aidl_status;\n";
      out << "_aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));\n";
      out << "return _aidl_status;\n";
      out.Dedent();
      out << "}\n";
    } else {
      if (method->GetName() == kGetInterfaceVersion && options.Version() > 0) {
        out << "::ndk::ScopedAStatus " << defaultClazz << "::" << method->GetName() << "("
            << "int32_t* _aidl_return) {\n";
        out.Indent();
        out << "*_aidl_return = 0;\n";
        out << "return ::ndk::ScopedAStatus(AStatus_newOk());\n";
        out.Dedent();
        out << "}\n";
      }
      if (method->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
        out << "::ndk::ScopedAStatus " << defaultClazz << "::" << method->GetName() << "("
            << "std::string* _aidl_return) {\n";
        out.Indent();
        out << "*_aidl_return = \"\";\n";
        out << "return ::ndk::ScopedAStatus(AStatus_newOk());\n";
        out.Dedent();
        out << "}\n";
      }
    }
  }

  out << "::ndk::SpAIBinder " << defaultClazz << "::asBinder() {\n";
  out.Indent();
  out << "return ::ndk::SpAIBinder();\n";
  out.Dedent();
  out << "}\n";

  out << "bool " << defaultClazz << "::isRemote() {\n";
  out.Indent();
  out << "return false;\n";
  out.Dedent();
  out << "}\n";
}

void GenerateClientHeader(CodeWriter& out, const AidlTypenames& types,
                          const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::CLIENT);

  out << "#pragma once\n\n";
  out << "#include \"" << NdkHeaderFile(defined_type, ClassNames::RAW, false /*use_os_sep*/)
      << "\"\n";
  out << "\n";
  out << "#include <android/binder_ibinder.h>\n";
  if (options.GenLog()) {
    out << "#include <json/value.h>\n";
    out << "#include <functional>\n";
    out << "#include <chrono>\n";
    out << "#include <sstream>\n";
  }
  out << "\n";
  EnterNdkNamespace(out, defined_type);
  out << "class " << clazz << " : public ::ndk::BpCInterface<"
      << ClassName(defined_type, ClassNames::INTERFACE) << "> {\n";
  out << "public:\n";
  out.Indent();
  out << clazz << "(const ::ndk::SpAIBinder& binder);\n";
  out << "virtual ~" << clazz << "();\n";
  out << "\n";
  for (const auto& method : defined_type.GetMethods()) {
    out << NdkMethodDecl(types, *method) << " override;\n";
  }

  if (options.Version() > 0) {
    out << "int32_t " << kCachedVersion << " = -1;\n";
  }

  if (!options.Hash().empty()) {
    out << "std::string " << kCachedHash << " = \"-1\";\n";
    out << "std::mutex " << kCachedHashMutex << ";\n";
  }
  if (options.GenLog()) {
    out << "static std::function<void(const Json::Value&)> logFunc;\n";
  }
  out.Dedent();
  out << "};\n";
  LeaveNdkNamespace(out, defined_type);
}
void GenerateServerHeader(CodeWriter& out, const AidlTypenames& types,
                          const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::SERVER);
  const std::string iface = ClassName(defined_type, ClassNames::INTERFACE);

  out << "#pragma once\n\n";
  out << "#include \"" << NdkHeaderFile(defined_type, ClassNames::RAW, false /*use_os_sep*/)
      << "\"\n";
  out << "\n";
  out << "#include <android/binder_ibinder.h>\n";
  out << "\n";
  EnterNdkNamespace(out, defined_type);
  out << "class " << clazz << " : public ::ndk::BnCInterface<" << iface << "> {\n";
  out << "public:\n";
  out.Indent();
  out << clazz << "();\n";
  out << "virtual ~" << clazz << "();\n";

  // Declare the meta methods
  for (const auto& method : defined_type.GetMethods()) {
    if (method->IsUserDefined()) {
      continue;
    }
    if (method->GetName() == kGetInterfaceVersion && options.Version() > 0) {
      out << NdkMethodDecl(types, *method) << " final override;\n";
    } else if (method->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      out << NdkMethodDecl(types, *method) << " final override;\n";
    } else {
      AIDL_FATAL(defined_type) << "Meta method '" << method->GetName() << "' is unimplemented.";
    }
  }
  if (options.GenLog()) {
    out << "static std::function<void(const Json::Value&)> logFunc;\n";
  }
  out.Dedent();
  out << "protected:\n";
  out.Indent();
  out << "::ndk::SpAIBinder createBinder() override;\n";
  out.Dedent();
  out << "private:\n";
  out.Indent();
  out.Dedent();
  out << "};\n";
  LeaveNdkNamespace(out, defined_type);
}
void GenerateInterfaceHeader(CodeWriter& out, const AidlTypenames& types,
                             const AidlInterface& defined_type, const Options& options) {
  const std::string clazz = ClassName(defined_type, ClassNames::INTERFACE);

  out << "#pragma once\n\n";
  out << "#include <android/binder_interface_utils.h>\n";
  if (options.GenLog()) {
    out << "#include <json/value.h>\n";
    out << "#include <functional>\n";
    out << "#include <chrono>\n";
    out << "#include <sstream>\n";
  }
  out << "\n";

  GenerateHeaderIncludes(out, types, defined_type);
  out << "\n";

  EnterNdkNamespace(out, defined_type);
  out << "class " << clazz << " : public ::ndk::ICInterface {\n";
  out << "public:\n";
  out.Indent();
  out << "static const char* " << kDescriptor << ";\n";
  out << clazz << "();\n";
  out << "virtual ~" << clazz << "();\n";
  out << "\n";
  GenerateConstantDeclarations(out, defined_type);
  if (options.Version() > 0) {
    out << "static const int32_t " << kVersion << " = " << std::to_string(options.Version())
        << ";\n";
  }
  if (!options.Hash().empty()) {
    out << "static inline const std::string " << kHash << " = \"" << options.Hash() << "\";\n";
  }
  out << "\n";
  out << "static std::shared_ptr<" << clazz << "> fromBinder(const ::ndk::SpAIBinder& binder);\n";
  out << "static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<" << clazz
      << ">& instance);";
  out << "\n";
  out << "static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<" << clazz
      << ">* instance);";
  out << "\n";
  out << "static bool setDefaultImpl(std::shared_ptr<" << clazz << "> impl);";
  out << "\n";
  out << "static const std::shared_ptr<" << clazz << ">& getDefaultImpl();";
  out << "\n";
  for (const auto& method : defined_type.GetMethods()) {
    out << "virtual " << NdkMethodDecl(types, *method) << " = 0;\n";
  }
  out.Dedent();
  out << "private:\n";
  out.Indent();
  out << "static std::shared_ptr<" << clazz << "> default_impl;\n";
  out.Dedent();
  out << "};\n";

  const std::string defaultClazz = clazz + "Default";

  out << "class " << defaultClazz << " : public " << clazz << " {\n";
  out << "public:\n";
  out.Indent();
  for (const auto& method : defined_type.GetMethods()) {
    if (method->IsUserDefined()) {
      out << NdkMethodDecl(types, *method) << " override;\n";
    } else if (method->GetName() == kGetInterfaceVersion && options.Version() > 0) {
      out << NdkMethodDecl(types, *method) << " override;\n";
    } else if (method->GetName() == kGetInterfaceHash && !options.Hash().empty()) {
      out << NdkMethodDecl(types, *method) << " override;\n";
    }
  }
  out << "::ndk::SpAIBinder asBinder() override;\n";
  out << "bool isRemote() override;\n";
  out.Dedent();
  out << "};\n";

  LeaveNdkNamespace(out, defined_type);
}
void GenerateParcelHeader(CodeWriter& out, const AidlTypenames& types,
                          const AidlStructuredParcelable& defined_type,
                          const Options& /*options*/) {
  const std::string clazz = ClassName(defined_type, ClassNames::RAW);

  out << "#pragma once\n";
  out << "#include <android/binder_interface_utils.h>\n";
  out << "\n";

  GenerateHeaderIncludes(out, types, defined_type);

  EnterNdkNamespace(out, defined_type);
  out << "class " << clazz << " {\n";
  out << "public:\n";
  out.Indent();
  out << "static const char* descriptor;\n";
  out << "\n";
  for (const auto& variable : defined_type.GetFields()) {
    out << NdkNameOf(types, variable->GetType(), StorageMode::STACK) << " " << variable->GetName();
    if (variable->GetDefaultValue()) {
      out << " = " << variable->ValueString(ConstantValueDecorator);
    }
    out << ";\n";
  }
  out << "\n";
  out << "binder_status_t readFromParcel(const AParcel* parcel);\n";
  out << "binder_status_t writeToParcel(AParcel* parcel) const;\n";
  out.Dedent();
  out << "};\n";
  LeaveNdkNamespace(out, defined_type);
}
void GenerateParcelSource(CodeWriter& out, const AidlTypenames& types,
                          const AidlStructuredParcelable& defined_type,
                          const Options& /*options*/) {
  const std::string clazz = ClassName(defined_type, ClassNames::RAW);

  out << "#include \"" << NdkHeaderFile(defined_type, ClassNames::RAW, false /*use_os_sep*/)
      << "\"\n";
  out << "\n";
  GenerateSourceIncludes(out, types, defined_type);
  out << "\n";
  EnterNdkNamespace(out, defined_type);
  out << "const char* " << clazz << "::" << kDescriptor << " = \""
      << defined_type.GetCanonicalName() << "\";\n";
  out << "\n";

  out << "binder_status_t " << clazz << "::readFromParcel(const AParcel* parcel) {\n";
  out.Indent();
  out << "int32_t _aidl_parcelable_size;\n";
  out << "int32_t _aidl_start_pos = AParcel_getDataPosition(parcel);\n";
  out << "binder_status_t _aidl_ret_status = AParcel_readInt32(parcel, &_aidl_parcelable_size);\n";
  out << "if (_aidl_parcelable_size < 0) return STATUS_BAD_VALUE;\n";
  StatusCheckReturn(out);

  for (const auto& variable : defined_type.GetFields()) {
    out << "_aidl_ret_status = ";
    ReadFromParcelFor({out, types, variable->GetType(), "parcel", "&" + variable->GetName()});
    out << ";\n";
    StatusCheckReturn(out);
    out << "if (AParcel_getDataPosition(parcel) - _aidl_start_pos >= _aidl_parcelable_size) {\n"
        << "  AParcel_setDataPosition(parcel, _aidl_start_pos + _aidl_parcelable_size);\n"
        << "  return _aidl_ret_status;\n"
        << "}\n";
  }
  out << "AParcel_setDataPosition(parcel, _aidl_start_pos + _aidl_parcelable_size);\n"
      << "return _aidl_ret_status;\n";
  out.Dedent();
  out << "}\n";

  out << "binder_status_t " << clazz << "::writeToParcel(AParcel* parcel) const {\n";
  out.Indent();
  out << "binder_status_t _aidl_ret_status;\n";

  out << "size_t _aidl_start_pos = AParcel_getDataPosition(parcel);\n";
  out << "_aidl_ret_status = AParcel_writeInt32(parcel, 0);\n";
  StatusCheckReturn(out);

  for (const auto& variable : defined_type.GetFields()) {
    out << "_aidl_ret_status = ";
    WriteToParcelFor({out, types, variable->GetType(), "parcel", variable->GetName()});
    out << ";\n";
    StatusCheckReturn(out);
  }
  out << "size_t _aidl_end_pos = AParcel_getDataPosition(parcel);\n";
  out << "AParcel_setDataPosition(parcel, _aidl_start_pos);\n";
  out << "AParcel_writeInt32(parcel, _aidl_end_pos - _aidl_start_pos);\n";
  out << "AParcel_setDataPosition(parcel, _aidl_end_pos);\n";

  out << "return _aidl_ret_status;\n";
  out.Dedent();
  out << "}\n";
  out << "\n";
  LeaveNdkNamespace(out, defined_type);
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
       << NdkNameOf(typenames, enum_decl.GetBackingType(), StorageMode::STACK) << ">(val));\n";
  code << "  }\n";
  code << "}\n";
  return code.str();
}

void GenerateEnumHeader(CodeWriter& out, const AidlTypenames& types,
                        const AidlEnumDeclaration& enum_decl, const Options& /*options*/) {
  out << "#pragma once\n";
  out << "\n";

  GenerateHeaderIncludes(out, types, enum_decl);
  // enum specific headers
  out << "#include <array>\n";
  out << "#include <android/binder_enums.h>\n";

  EnterNdkNamespace(out, enum_decl);
  out << "enum class " << enum_decl.GetName() << " : "
      << NdkNameOf(types, enum_decl.GetBackingType(), StorageMode::STACK) << " {\n";
  out.Indent();
  for (const auto& enumerator : enum_decl.GetEnumerators()) {
    out << enumerator->GetName() << " = "
        << enumerator->ValueString(enum_decl.GetBackingType(), ConstantValueDecorator) << ",\n";
  }
  out.Dedent();
  out << "};\n";
  out << "\n";
  out << GenerateEnumToString(types, enum_decl);
  LeaveNdkNamespace(out, enum_decl);

  out << "namespace ndk {\n";
  out << "namespace internal {\n";
  out << cpp::GenerateEnumValues(enum_decl, {"aidl"});
  out << "}  // namespace internal\n";
  out << "}  // namespace android\n";
}

}  // namespace internals
}  // namespace ndk
}  // namespace aidl
}  // namespace android
