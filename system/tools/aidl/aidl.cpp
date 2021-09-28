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

#include "aidl.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#endif

#include <android-base/strings.h>

#include "aidl_language.h"
#include "aidl_typenames.h"
#include "generate_aidl_mappings.h"
#include "generate_cpp.h"
#include "generate_java.h"
#include "generate_ndk.h"
#include "import_resolver.h"
#include "logging.h"
#include "options.h"
#include "os.h"

#ifndef O_BINARY
#  define O_BINARY  0
#endif

using android::base::Join;
using android::base::Split;
using std::cerr;
using std::endl;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace aidl {
namespace {

// Copied from android.is.IBinder.[FIRST|LAST]_CALL_TRANSACTION
const int kFirstCallTransaction = 1;
const int kLastCallTransaction = 0x00ffffff;

// Following IDs are all offsets from  kFirstCallTransaction

// IDs for meta transactions. Most of the meta transactions are implemented in
// the framework side (Binder.java or Binder.cpp). But these are the ones that
// are auto-implemented by the AIDL compiler.
const int kFirstMetaMethodId = kLastCallTransaction - kFirstCallTransaction;
const int kGetInterfaceVersionId = kFirstMetaMethodId;
const int kGetInterfaceHashId = kFirstMetaMethodId - 1;
// Additional meta transactions implemented by AIDL should use
// kFirstMetaMethodId -1, -2, ...and so on.

// Reserve 100 IDs for meta methods, which is more than enough. If we don't reserve,
// in the future, a newly added meta transaction ID will have a chance to
// collide with the user-defined methods that were added in the past. So,
// let's prevent users from using IDs in this range from the beginning.
const int kLastMetaMethodId = kFirstMetaMethodId - 99;

// Range of IDs that is allowed for user-defined methods.
const int kMinUserSetMethodId = 0;
const int kMaxUserSetMethodId = kLastMetaMethodId - 1;

bool check_filename(const std::string& filename, const AidlDefinedType& defined_type) {
    const char* p;
    string expected;
    string fn;
    size_t len;
    bool valid = false;

    if (!IoDelegate::GetAbsolutePath(filename, &fn)) {
      return false;
    }

    const std::string package = defined_type.GetPackage();
    if (!package.empty()) {
        expected = package;
        expected += '.';
    }

    len = expected.length();
    for (size_t i=0; i<len; i++) {
        if (expected[i] == '.') {
            expected[i] = OS_PATH_SEPARATOR;
        }
    }

    const std::string name = defined_type.GetName();
    expected.append(name, 0, name.find('.'));

    expected += ".aidl";

    len = fn.length();
    valid = (len >= expected.length());

    if (valid) {
        p = fn.c_str() + (len - expected.length());

#ifdef _WIN32
        if (OS_PATH_SEPARATOR != '/') {
            // Input filename under cygwin most likely has / separators
            // whereas the expected string uses \\ separators. Adjust
            // them accordingly.
          for (char *c = const_cast<char *>(p); *c; ++c) {
                if (*c == '/') *c = OS_PATH_SEPARATOR;
            }
        }
#endif

        // aidl assumes case-insensitivity on Mac Os and Windows.
#if defined(__linux__)
        valid = (expected == p);
#else
        valid = !strcasecmp(expected.c_str(), p);
#endif
    }

    if (!valid) {
      AIDL_ERROR(defined_type) << name << " should be declared in a file called " << expected;
    }

    return valid;
}

bool write_dep_file(const Options& options, const AidlDefinedType& defined_type,
                    const vector<string>& imports, const IoDelegate& io_delegate,
                    const string& input_file, const string& output_file) {
  string dep_file_name = options.DependencyFile();
  if (dep_file_name.empty() && options.AutoDepFile()) {
    dep_file_name = output_file + ".d";
  }

  if (dep_file_name.empty()) {
    return true;  // nothing to do
  }

  CodeWriterPtr writer = io_delegate.GetCodeWriter(dep_file_name);
  if (!writer) {
    LOG(ERROR) << "Could not open dependency file: " << dep_file_name;
    return false;
  }

  vector<string> source_aidl = {input_file};
  for (const auto& import : imports) {
    source_aidl.push_back(import);
  }

  // Encode that the output file depends on aidl input files.
  if (defined_type.AsUnstructuredParcelable() != nullptr &&
      options.TargetLanguage() == Options::Language::JAVA) {
    // Legacy behavior. For parcelable declarations in Java, don't emit output file as
    // the dependency target. b/141372861
    writer->Write(" : \\\n");
  } else {
    writer->Write("%s : \\\n", output_file.c_str());
  }
  writer->Write("  %s", Join(source_aidl, " \\\n  ").c_str());
  writer->Write("\n");

  if (!options.DependencyFileNinja()) {
    writer->Write("\n");
    // Output "<input_aidl_file>: " so make won't fail if the input .aidl file
    // has been deleted, moved or renamed in incremental build.
    for (const auto& src : source_aidl) {
      writer->Write("%s :\n", src.c_str());
    }
  }

  if (options.IsCppOutput()) {
    if (!options.DependencyFileNinja()) {
      using ::android::aidl::cpp::ClassNames;
      using ::android::aidl::cpp::HeaderFile;
      vector<string> headers;
      for (ClassNames c : {ClassNames::CLIENT, ClassNames::SERVER, ClassNames::RAW}) {
        headers.push_back(options.OutputHeaderDir() +
                          HeaderFile(defined_type, c, false /* use_os_sep */));
      }

      writer->Write("\n");

      // Generated headers also depend on the source aidl files.
      writer->Write("%s : \\\n    %s\n", Join(headers, " \\\n    ").c_str(),
                    Join(source_aidl, " \\\n    ").c_str());
    }
  }

  return true;
}

string generate_outputFileName(const Options& options, const AidlDefinedType& defined_type) {
  // create the path to the destination folder based on the
  // defined_type package name
  string result = options.OutputDir();

  string package = defined_type.GetPackage();
  size_t len = package.length();
  for (size_t i = 0; i < len; i++) {
    if (package[i] == '.') {
      package[i] = OS_PATH_SEPARATOR;
    }
  }

  result += package;

  // add the filename by replacing the .aidl extension to .java
  const string& name = defined_type.GetName();
  result += OS_PATH_SEPARATOR;
  result.append(name, 0, name.find('.'));
  if (options.TargetLanguage() == Options::Language::JAVA) {
    result += ".java";
  } else if (options.IsCppOutput()) {
    result += ".cpp";
  } else {
    LOG(FATAL) << "Should not reach here" << endl;
    return "";
  }

  return result;
}

bool check_and_assign_method_ids(const std::vector<std::unique_ptr<AidlMethod>>& items) {
  // Check whether there are any methods with manually assigned id's and any
  // that are not. Either all method id's must be manually assigned or all of
  // them must not. Also, check for uplicates of user set ID's and that the
  // ID's are within the proper bounds.
  set<int> usedIds;
  bool hasUnassignedIds = false;
  bool hasAssignedIds = false;
  int newId = kMinUserSetMethodId;
  for (const auto& item : items) {
    // However, meta transactions that are added by the AIDL compiler are
    // exceptions. They have fixed IDs but allowed to be with user-defined
    // methods having auto-assigned IDs. This is because the Ids of the meta
    // transactions must be stable during the entire lifetime of an interface.
    // In other words, their IDs must be the same even when new user-defined
    // methods are added.
    if (!item->IsUserDefined()) {
      continue;
    }
    if (item->HasId()) {
      hasAssignedIds = true;
    } else {
      item->SetId(newId++);
      hasUnassignedIds = true;
    }

    if (hasAssignedIds && hasUnassignedIds) {
      AIDL_ERROR(item) << "You must either assign id's to all methods or to none of them.";
      return false;
    }

    // Ensure that the user set id is not duplicated.
    if (usedIds.find(item->GetId()) != usedIds.end()) {
      // We found a duplicate id, so throw an error.
      AIDL_ERROR(item) << "Found duplicate method id (" << item->GetId() << ") for method "
                       << item->GetName();
      return false;
    }
    usedIds.insert(item->GetId());

    // Ensure that the user set id is within the appropriate limits
    if (item->GetId() < kMinUserSetMethodId || item->GetId() > kMaxUserSetMethodId) {
      AIDL_ERROR(item) << "Found out of bounds id (" << item->GetId() << ") for method "
                       << item->GetName() << ". Value for id must be between "
                       << kMinUserSetMethodId << " and " << kMaxUserSetMethodId << " inclusive.";
      return false;
    }
  }

  return true;
}

// TODO: Remove this in favor of using the YACC parser b/25479378
bool ParsePreprocessedLine(const string& line, string* decl,
                           vector<string>* package, string* class_name) {
  // erase all trailing whitespace and semicolons
  const size_t end = line.find_last_not_of(" ;\t");
  if (end == string::npos) {
    return false;
  }
  if (line.rfind(';', end) != string::npos) {
    return false;
  }

  decl->clear();
  string type;
  vector<string> pieces = Split(line.substr(0, end + 1), " \t");
  for (const string& piece : pieces) {
    if (piece.empty()) {
      continue;
    }
    if (decl->empty()) {
      *decl = std::move(piece);
    } else if (type.empty()) {
      type = std::move(piece);
    } else {
      return false;
    }
  }

  // Note that this logic is absolutely wrong.  Given a parcelable
  // org.some.Foo.Bar, the class name is Foo.Bar, but this code will claim that
  // the class is just Bar.  However, this was the way it was done in the past.
  //
  // See b/17415692
  size_t dot_pos = type.rfind('.');
  if (dot_pos != string::npos) {
    *class_name = type.substr(dot_pos + 1);
    *package = Split(type.substr(0, dot_pos), ".");
  } else {
    *class_name = type;
    package->clear();
  }

  return true;
}

}  // namespace

namespace internals {

bool parse_preprocessed_file(const IoDelegate& io_delegate, const string& filename,
                             AidlTypenames* typenames) {
  bool success = true;
  unique_ptr<LineReader> line_reader = io_delegate.GetLineReader(filename);
  if (!line_reader) {
    LOG(ERROR) << "cannot open preprocessed file: " << filename;
    success = false;
    return success;
  }

  string line;
  int lineno = 1;
  for ( ; line_reader->ReadLine(&line); ++lineno) {
    if (line.empty() || line.compare(0, 2, "//") == 0) {
      // skip comments and empty lines
      continue;
    }

    string decl;
    vector<string> package;
    string class_name;
    if (!ParsePreprocessedLine(line, &decl, &package, &class_name)) {
      success = false;
      break;
    }

    AidlLocation::Point point = {.line = lineno, .column = 0 /*column*/};
    AidlLocation location = AidlLocation(filename, point, point);

    if (decl == "parcelable") {
      // ParcelFileDescriptor is treated as a built-in type, but it's also in the framework.aidl.
      // So aidl should ignore built-in types in framework.aidl to prevent duplication.
      // (b/130899491)
      if (AidlTypenames::IsBuiltinTypename(class_name)) {
        continue;
      }
      AidlParcelable* doc = new AidlParcelable(
          location, new AidlQualifiedName(location, class_name, ""), package, "" /* comments */);
      typenames->AddPreprocessedType(unique_ptr<AidlParcelable>(doc));
    } else if (decl == "structured_parcelable") {
      auto temp = new std::vector<std::unique_ptr<AidlVariableDeclaration>>();
      AidlStructuredParcelable* doc =
          new AidlStructuredParcelable(location, new AidlQualifiedName(location, class_name, ""),
                                       package, "" /* comments */, temp);
      typenames->AddPreprocessedType(unique_ptr<AidlStructuredParcelable>(doc));
    } else if (decl == "interface") {
      auto temp = new std::vector<std::unique_ptr<AidlMember>>();
      AidlInterface* doc = new AidlInterface(location, class_name, "", false, temp, package);
      typenames->AddPreprocessedType(unique_ptr<AidlInterface>(doc));
    } else {
      success = false;
      break;
    }
  }
  if (!success) {
    LOG(ERROR) << filename << ':' << lineno
               << " malformed preprocessed file line: '" << line << "'";
  }

  return success;
}

AidlError load_and_validate_aidl(const std::string& input_file_name, const Options& options,
                                 const IoDelegate& io_delegate, AidlTypenames* typenames,
                                 vector<AidlDefinedType*>* defined_types,
                                 vector<string>* imported_files) {
  AidlError err = AidlError::OK;

  //////////////////////////////////////////////////////////////////////////
  // Loading phase
  //////////////////////////////////////////////////////////////////////////

  // Parse the main input file
  std::unique_ptr<Parser> main_parser = Parser::Parse(input_file_name, io_delegate, *typenames);
  if (main_parser == nullptr) {
    return AidlError::PARSE_ERROR;
  }
  int num_interfaces_or_structured_parcelables = 0;
  for (AidlDefinedType* type : main_parser->GetDefinedTypes()) {
    if (type->AsInterface() != nullptr || type->AsStructuredParcelable() != nullptr) {
      num_interfaces_or_structured_parcelables++;
    }
  }
  if (num_interfaces_or_structured_parcelables > 1) {
    AIDL_ERROR(input_file_name) << "You must declare only one type per a file.";
    return AidlError::BAD_TYPE;
  }

  // Import the preprocessed file
  for (const string& s : options.PreprocessedFiles()) {
    if (!parse_preprocessed_file(io_delegate, s, typenames)) {
      err = AidlError::BAD_PRE_PROCESSED_FILE;
    }
  }
  if (err != AidlError::OK) {
    return err;
  }

  // Find files to import and parse them
  vector<string> import_paths;
  ImportResolver import_resolver{io_delegate, input_file_name, options.ImportDirs(),
                                 options.InputFiles()};

  vector<string> type_from_import_statements;
  for (const auto& import : main_parser->GetImports()) {
    if (!AidlTypenames::IsBuiltinTypename(import->GetNeededClass())) {
      type_from_import_statements.emplace_back(import->GetNeededClass());
    }
  }

  // When referencing a type using fully qualified name it should be imported
  // without the import statement. To support that, add all unresolved
  // typespecs encountered during the parsing to the import_candidates list.
  // Note that there is no guarantee that the typespecs are all fully qualified.
  // It will be determined by calling FindImportFile().
  set<string> unresolved_types;
  for (const auto type : main_parser->GetUnresolvedTypespecs()) {
    if (!AidlTypenames::IsBuiltinTypename(type->GetName())) {
      unresolved_types.emplace(type->GetName());
    }
  }
  vector<string> import_candidates(type_from_import_statements);
  import_candidates.insert(import_candidates.end(), unresolved_types.begin(),
                           unresolved_types.end());
  for (const auto& import : import_candidates) {
    if (typenames->IsIgnorableImport(import)) {
      // There are places in the Android tree where an import doesn't resolve,
      // but we'll pick the type up through the preprocessed types.
      // This seems like an error, but legacy support demands we support it...
      continue;
    }
    string import_path = import_resolver.FindImportFile(import);
    if (import_path.empty()) {
      if (typenames->ResolveTypename(import).second) {
        // Couldn't find the *.aidl file for the type from the include paths, but we
        // have the type already resolved. This could happen when the type is
        // from the preprocessed aidl file. In that case, use the type from the
        // preprocessed aidl file as a last resort.
        continue;
      }

      if (std::find(type_from_import_statements.begin(), type_from_import_statements.end(),
                    import) != type_from_import_statements.end()) {
        // Complain only when the import from the import statement has failed.
        AIDL_ERROR(import) << "couldn't find import for class " << import;
        err = AidlError::BAD_IMPORT;
      }
      continue;
    }

    import_paths.emplace_back(import_path);

    std::unique_ptr<Parser> import_parser = Parser::Parse(import_path, io_delegate, *typenames);
    if (import_parser == nullptr) {
      cerr << "error while importing " << import_path << " for " << import << endl;
      err = AidlError::BAD_IMPORT;
      continue;
    }
  }
  if (err != AidlError::OK) {
    return err;
  }

  for (const auto& imported_file : options.ImportFiles()) {
    import_paths.emplace_back(imported_file);

    std::unique_ptr<Parser> import_parser = Parser::Parse(imported_file, io_delegate, *typenames);
    if (import_parser == nullptr) {
      AIDL_ERROR(imported_file) << "error while importing " << imported_file;
      err = AidlError::BAD_IMPORT;
      continue;
    }
  }
  if (err != AidlError::OK) {
    return err;
  }
  const bool is_check_api = options.GetTask() == Options::Task::CHECK_API;

  // Resolve the unresolved type references found from the input file
  if (!is_check_api && !main_parser->Resolve()) {
    // Resolution is not need for check api because all typespecs are
    // using fully qualified names.
    return AidlError::BAD_TYPE;
  }

  typenames->IterateTypes([&](const AidlDefinedType& type) {
    AidlEnumDeclaration* enum_decl = const_cast<AidlEnumDeclaration*>(type.AsEnumDeclaration());
    if (enum_decl != nullptr) {
      // BackingType is filled in for all known enums, including imported enums,
      // because other types that may use enums, such as Interface or
      // StructuredParcelable, need to know the enum BackingType when
      // generating code.
      if (auto backing_type = enum_decl->BackingType(*typenames); backing_type != nullptr) {
        enum_decl->SetBackingType(std::unique_ptr<const AidlTypeSpecifier>(backing_type));
      } else {
        // Default to byte type for enums.
        auto byte_type =
            std::make_unique<AidlTypeSpecifier>(AIDL_LOCATION_HERE, "byte", false, nullptr, "");
        byte_type->Resolve(*typenames);
        enum_decl->SetBackingType(std::move(byte_type));
      }

      if (!enum_decl->Autofill()) {
        err = AidlError::BAD_TYPE;
      }
    }
  });
  if (err != AidlError::OK) {
    return err;
  }

  //////////////////////////////////////////////////////////////////////////
  // Validation phase
  //////////////////////////////////////////////////////////////////////////

  // For legacy reasons, by default, compiling an unstructured parcelable (which contains no output)
  // is allowed. This must not be returned as an error until the very end of this procedure since
  // this may be considered a success, and we should first check that there are not other, more
  // serious failures.
  bool contains_unstructured_parcelable = false;

  const int num_defined_types = main_parser->GetDefinedTypes().size();
  for (const auto defined_type : main_parser->GetDefinedTypes()) {
    CHECK(defined_type != nullptr);

    // Language specific validation
    if (!defined_type->LanguageSpecificCheckValid(options.TargetLanguage())) {
      return AidlError::BAD_TYPE;
    }

    AidlParcelable* unstructuredParcelable = defined_type->AsUnstructuredParcelable();
    if (unstructuredParcelable != nullptr) {
      if (!unstructuredParcelable->CheckValid(*typenames)) {
        return AidlError::BAD_TYPE;
      }
      bool isStable = unstructuredParcelable->IsStableApiParcelable(options.TargetLanguage());
      if (options.IsStructured() && !isStable) {
        AIDL_ERROR(unstructuredParcelable)
            << "Cannot declared parcelable in a --structured interface. Parcelable must be defined "
               "in AIDL directly.";
        return AidlError::NOT_STRUCTURED;
      }
      if (options.FailOnParcelable()) {
        AIDL_ERROR(unstructuredParcelable)
            << "Refusing to generate code with unstructured parcelables. Declared parcelables "
               "should be in their own file and/or cannot be used with --structured interfaces.";
        // Continue parsing for more errors
      }

      contains_unstructured_parcelable = true;
      continue;
    }

    if (defined_type->IsVintfStability() &&
        (options.GetStability() != Options::Stability::VINTF || !options.IsStructured())) {
      AIDL_ERROR(defined_type)
          << "Must compile @VintfStability type w/ aidl_interface 'stability: \"vintf\"'";
      return AidlError::NOT_STRUCTURED;
    }

    // Ensure that a type is either an interface, structured parcelable, or
    // enum.
    AidlInterface* interface = defined_type->AsInterface();
    AidlStructuredParcelable* parcelable = defined_type->AsStructuredParcelable();
    AidlEnumDeclaration* enum_decl = defined_type->AsEnumDeclaration();
    CHECK(!!interface + !!parcelable + !!enum_decl == 1);

    // Ensure that foo.bar.IFoo is defined in <some_path>/foo/bar/IFoo.aidl
    if (num_defined_types == 1 && !check_filename(input_file_name, *defined_type)) {
      return AidlError::BAD_PACKAGE;
    }

    // Check the referenced types in parsed_doc to make sure we've imported them
    if (!is_check_api) {
      // No need to do this for check api because all typespecs are already
      // using fully qualified name and we don't import in AIDL files.
      if (!defined_type->CheckValid(*typenames)) {
        return AidlError::BAD_TYPE;
      }
    }

    if (interface != nullptr) {
      // add the meta-method 'int getInterfaceVersion()' if version is specified.
      if (options.Version() > 0) {
        AidlTypeSpecifier* ret =
            new AidlTypeSpecifier(AIDL_LOCATION_HERE, "int", false, nullptr, "");
        ret->Resolve(*typenames);
        vector<unique_ptr<AidlArgument>>* args = new vector<unique_ptr<AidlArgument>>();
        AidlMethod* method =
            new AidlMethod(AIDL_LOCATION_HERE, false, ret, "getInterfaceVersion", args, "",
                           kGetInterfaceVersionId, false /* is_user_defined */);
        interface->GetMutableMethods().emplace_back(method);
      }
      // add the meta-method 'string getInterfaceHash()' if hash is specified.
      if (!options.Hash().empty()) {
        AidlTypeSpecifier* ret =
            new AidlTypeSpecifier(AIDL_LOCATION_HERE, "String", false, nullptr, "");
        ret->Resolve(*typenames);
        vector<unique_ptr<AidlArgument>>* args = new vector<unique_ptr<AidlArgument>>();
        AidlMethod* method = new AidlMethod(AIDL_LOCATION_HERE, false, ret, kGetInterfaceHash, args,
                                            "", kGetInterfaceHashId, false /* is_user_defined */);
        interface->GetMutableMethods().emplace_back(method);
      }
      if (!check_and_assign_method_ids(interface->GetMethods())) {
        return AidlError::BAD_METHOD_ID;
      }

      // Verify and resolve the constant declarations
      for (const auto& constant : interface->GetConstantDeclarations()) {
        switch (constant->GetValue().GetType()) {
          case AidlConstantValue::Type::STRING:    // fall-through
          case AidlConstantValue::Type::INT8:      // fall-through
          case AidlConstantValue::Type::INT32:     // fall-through
          case AidlConstantValue::Type::INT64:     // fall-through
          case AidlConstantValue::Type::FLOATING:  // fall-through
          case AidlConstantValue::Type::UNARY:     // fall-through
          case AidlConstantValue::Type::BINARY: {
            bool success = constant->CheckValid(*typenames);
            if (!success) {
              return AidlError::BAD_TYPE;
            }
            if (constant->ValueString(cpp::ConstantValueDecorator).empty()) {
              return AidlError::BAD_TYPE;
            }
            break;
          }
          default:
            LOG(FATAL) << "Unrecognized constant type: "
                       << static_cast<int>(constant->GetValue().GetType());
            break;
        }
      }
    }
  }

  typenames->IterateTypes([&](const AidlDefinedType& type) {
    if (options.IsStructured() && type.AsUnstructuredParcelable() != nullptr &&
        !type.AsUnstructuredParcelable()->IsStableApiParcelable(options.TargetLanguage())) {
      err = AidlError::NOT_STRUCTURED;
      LOG(ERROR) << type.GetCanonicalName()
                 << " is not structured, but this is a structured interface.";
    }
    if (options.GetStability() == Options::Stability::VINTF && !type.IsVintfStability()) {
      err = AidlError::NOT_STRUCTURED;
      LOG(ERROR) << type.GetCanonicalName()
                 << " does not have VINTF level stability, but this interface requires it.";
    }
  });

  if (err != AidlError::OK) {
    return err;
  }

  if (defined_types != nullptr) {
    *defined_types = main_parser->GetDefinedTypes();
  }

  if (imported_files != nullptr) {
    *imported_files = import_paths;
  }

  if (contains_unstructured_parcelable) {
    // Considered a success for the legacy case, so this must be returned last.
    return AidlError::FOUND_PARCELABLE;
  }

  return AidlError::OK;
}

} // namespace internals

int compile_aidl(const Options& options, const IoDelegate& io_delegate) {
  const Options::Language lang = options.TargetLanguage();
  for (const string& input_file : options.InputFiles()) {
    AidlTypenames typenames;

    vector<AidlDefinedType*> defined_types;
    vector<string> imported_files;

    AidlError aidl_err = internals::load_and_validate_aidl(
        input_file, options, io_delegate, &typenames, &defined_types, &imported_files);
    bool allowError = aidl_err == AidlError::FOUND_PARCELABLE && !options.FailOnParcelable();
    if (aidl_err != AidlError::OK && !allowError) {
      return 1;
    }

    for (const auto defined_type : defined_types) {
      CHECK(defined_type != nullptr);

      string output_file_name = options.OutputFile();
      // if needed, generate the output file name from the base folder
      if (output_file_name.empty() && !options.OutputDir().empty()) {
        output_file_name = generate_outputFileName(options, *defined_type);
        if (output_file_name.empty()) {
          return 1;
        }
      }

      if (!write_dep_file(options, *defined_type, imported_files, io_delegate, input_file,
                          output_file_name)) {
        return 1;
      }

      bool success = false;
      if (lang == Options::Language::CPP) {
        success =
            cpp::GenerateCpp(output_file_name, options, typenames, *defined_type, io_delegate);
      } else if (lang == Options::Language::NDK) {
        ndk::GenerateNdk(output_file_name, options, typenames, *defined_type, io_delegate);
        success = true;
      } else if (lang == Options::Language::JAVA) {
        if (defined_type->AsUnstructuredParcelable() != nullptr) {
          // Legacy behavior. For parcelable declarations in Java, don't generate output file.
          success = true;
        } else {
          success =
              java::generate_java(output_file_name, defined_type, typenames, io_delegate, options);
        }
      } else {
        LOG(FATAL) << "Should not reach here" << endl;
        return 1;
      }
      if (!success) {
        return 1;
      }
    }
  }
  return 0;
}

bool dump_mappings(const Options& options, const IoDelegate& io_delegate) {
  android::aidl::mappings::SignatureMap all_mappings;
  for (const string& input_file : options.InputFiles()) {
    AidlTypenames typenames;
    vector<AidlDefinedType*> defined_types;
    vector<string> imported_files;

    AidlError aidl_err = internals::load_and_validate_aidl(
        input_file, options, io_delegate, &typenames, &defined_types, &imported_files);
    if (aidl_err != AidlError::OK) {
      LOG(WARNING) << "AIDL file is invalid.\n";
      continue;
    }
    for (const auto defined_type : defined_types) {
      auto mappings = mappings::generate_mappings(defined_type, typenames);
      all_mappings.insert(mappings.begin(), mappings.end());
    }
  }
  std::stringstream mappings_str;
  for (const auto& mapping : all_mappings) {
    mappings_str << mapping.first << "\n" << mapping.second << "\n";
  }
  auto code_writer = io_delegate.GetCodeWriter(options.OutputFile());
  code_writer->Write("%s", mappings_str.str().c_str());
  return true;
}

bool preprocess_aidl(const Options& options, const IoDelegate& io_delegate) {
  unique_ptr<CodeWriter> writer = io_delegate.GetCodeWriter(options.OutputFile());

  for (const auto& file : options.InputFiles()) {
    AidlTypenames typenames;
    std::unique_ptr<Parser> p = Parser::Parse(file, io_delegate, typenames);
    if (p == nullptr) return false;

    for (const auto& defined_type : p->GetDefinedTypes()) {
      if (!writer->Write("%s %s;\n", defined_type->GetPreprocessDeclarationName().c_str(),
                         defined_type->GetCanonicalName().c_str())) {
        return false;
      }
    }
  }

  return writer->Close();
}

static string GetApiDumpPathFor(const AidlDefinedType& defined_type, const Options& options) {
  string package_as_path = Join(Split(defined_type.GetPackage(), "."), OS_PATH_SEPARATOR);
  CHECK(!options.OutputDir().empty() && options.OutputDir().back() == '/');
  return options.OutputDir() + package_as_path + OS_PATH_SEPARATOR + defined_type.GetName() +
         ".aidl";
}

bool dump_api(const Options& options, const IoDelegate& io_delegate) {
  for (const auto& file : options.InputFiles()) {
    AidlTypenames typenames;
    vector<AidlDefinedType*> defined_types;
    if (internals::load_and_validate_aidl(file, options, io_delegate, &typenames, &defined_types,
                                          nullptr) == AidlError::OK) {
      for (const auto type : defined_types) {
        unique_ptr<CodeWriter> writer =
            io_delegate.GetCodeWriter(GetApiDumpPathFor(*type, options));
        if (!type->GetPackage().empty()) {
          (*writer) << kPreamble << "package " << type->GetPackage() << ";\n";
        }
        type->Dump(writer.get());
      }
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace aidl
}  // namespace android
