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

#include "aidl.h"
#include "aidl_language.h"
#include "import_resolver.h"
#include "logging.h"
#include "options.h"

#include <map>
#include <string>
#include <vector>

#include <android-base/strings.h>

namespace android {
namespace aidl {

using std::map;
using std::set;
using std::string;
using std::vector;

static set<AidlAnnotation> get_strict_annotations(const AidlAnnotatable& node) {
  // This must be symmetrical (if you can add something, you must be able to
  // remove it). The reason is that we have no way of knowing which interface a
  // server serves and which interface a client serves (e.g. a callback
  // interface). Note that this is being overly lenient. It makes sense for
  // newer code to start accepting nullable things. However, here, we don't know
  // if the client of an interface or the server of an interface is newer.
  //
  // Here are two examples to demonstrate this:
  // - a new implementation might change so that it no longer returns null
  // values (remove @nullable)
  // - a new implementation might start accepting null values (add @nullable)
  static const set<std::string> kIgnoreAnnotations{
      "nullable",
  };
  set<AidlAnnotation> annotations;
  for (const AidlAnnotation& annotation : node.GetAnnotations()) {
    if (kIgnoreAnnotations.find(annotation.GetName()) == kIgnoreAnnotations.end()) {
      annotations.insert(annotation);
    }
  }
  return annotations;
}

static bool have_compatible_annotations(const AidlAnnotatable& older,
                                        const AidlAnnotatable& newer) {
  set<AidlAnnotation> olderAnnotations = get_strict_annotations(older);
  set<AidlAnnotation> newerAnnotations = get_strict_annotations(newer);

  if (olderAnnotations != newerAnnotations) {
    const string from = older.ToString().empty() ? "(empty)" : older.ToString();
    const string to = newer.ToString().empty() ? "(empty)" : newer.ToString();
    AIDL_ERROR(newer) << "Changed annotations: " << from << " to " << to;
    return false;
  }
  return true;
}

static bool are_compatible_types(const AidlTypeSpecifier& older, const AidlTypeSpecifier& newer) {
  bool compatible = true;
  if (older.ToString() != newer.ToString()) {
    AIDL_ERROR(newer) << "Type changed: " << older.ToString() << " to " << newer.ToString() << ".";
    compatible = false;
  }
  compatible &= have_compatible_annotations(older, newer);
  return compatible;
}

static bool are_compatible_interfaces(const AidlInterface& older, const AidlInterface& newer) {
  bool compatible = true;
  compatible &= have_compatible_annotations(older, newer);

  map<string, AidlMethod*> new_methods;
  for (const auto& m : newer.AsInterface()->GetMethods()) {
    new_methods.emplace(m->Signature(), m.get());
  }

  for (const auto& old_m : older.AsInterface()->GetMethods()) {
    const auto found = new_methods.find(old_m->Signature());
    if (found == new_methods.end()) {
      AIDL_ERROR(old_m) << "Removed or changed method: " << older.GetCanonicalName() << "."
                        << old_m->Signature();
      compatible = false;
      continue;
    }

    // Compare IDs to detect method reordering. IDs are assigned by their
    // textual order, so if there is an ID mismatch, that means reordering
    // has happened.
    const auto new_m = found->second;

    if (old_m->IsOneway() != new_m->IsOneway()) {
      AIDL_ERROR(new_m) << "Oneway attribute " << (old_m->IsOneway() ? "removed" : "added") << ": "
                        << older.GetCanonicalName() << "." << old_m->Signature();
      compatible = false;
    }

    if (old_m->GetId() != new_m->GetId()) {
      AIDL_ERROR(new_m) << "Transaction ID changed: " << older.GetCanonicalName() << "."
                        << old_m->Signature() << " is changed from " << old_m->GetId() << " to "
                        << new_m->GetId() << ".";
      compatible = false;
    }

    compatible &= are_compatible_types(old_m->GetType(), new_m->GetType());

    const auto& old_args = old_m->GetArguments();
    const auto& new_args = new_m->GetArguments();
    // this is guaranteed because arguments are part of AidlMethod::Signature()
    CHECK(old_args.size() == new_args.size());
    for (size_t i = 0; i < old_args.size(); i++) {
      const AidlArgument& old_a = *(old_args.at(i));
      const AidlArgument& new_a = *(new_args.at(i));
      compatible &= are_compatible_types(old_a.GetType(), new_a.GetType());

      if (old_a.GetDirection() != new_a.GetDirection()) {
        AIDL_ERROR(new_m) << "Direction changed: " << old_a.GetDirectionSpecifier() << " to "
                          << new_a.GetDirectionSpecifier() << ".";
        compatible = false;
      }
    }
  }

  map<string, AidlConstantDeclaration*> new_constdecls;
  for (const auto& c : newer.AsInterface()->GetConstantDeclarations()) {
    new_constdecls.emplace(c->GetName(), c.get());
  }

  for (const auto& old_c : older.AsInterface()->GetConstantDeclarations()) {
    const auto found = new_constdecls.find(old_c->GetName());
    if (found == new_constdecls.end()) {
      AIDL_ERROR(old_c) << "Removed constant declaration: " << older.GetCanonicalName() << "."
                        << old_c->GetName();
      compatible = false;
      continue;
    }

    const auto new_c = found->second;
    compatible &= are_compatible_types(old_c->GetType(), new_c->GetType());

    const string old_value = old_c->ValueString(AidlConstantValueDecorator);
    const string new_value = new_c->ValueString(AidlConstantValueDecorator);
    if (old_value != new_value) {
      AIDL_ERROR(newer) << "Changed constant value: " << older.GetCanonicalName() << "."
                        << old_c->GetName() << " from " << old_value << " to " << new_value << ".";
      compatible = false;
    }
  }
  return compatible;
}

static bool are_compatible_parcelables(const AidlStructuredParcelable& older,
                                       const AidlStructuredParcelable& newer) {
  const auto& old_fields = older.GetFields();
  const auto& new_fields = newer.GetFields();
  if (old_fields.size() > new_fields.size()) {
    // you can add new fields only at the end
    AIDL_ERROR(newer) << "Number of fields in " << older.GetCanonicalName() << " is reduced from "
                      << old_fields.size() << " to " << new_fields.size() << ".";
    return false;
  }

  bool compatible = true;
  for (size_t i = 0; i < old_fields.size(); i++) {
    const auto& old_field = old_fields.at(i);
    const auto& new_field = new_fields.at(i);
    compatible &= are_compatible_types(old_field->GetType(), new_field->GetType());

    const string old_value = old_field->ValueString(AidlConstantValueDecorator);
    const string new_value = new_field->ValueString(AidlConstantValueDecorator);
    if (old_value != new_value) {
      AIDL_ERROR(newer) << "Changed default value: " << old_value << " to " << new_value << ".";
      compatible = false;
    }
  }

  // Reordering of fields is an incompatible change.
  for (size_t i = 0; i < new_fields.size(); i++) {
    const auto& new_field = new_fields.at(i);
    auto found = std::find_if(old_fields.begin(), old_fields.end(), [&new_field](const auto& f) {
      return new_field->GetName() == f->GetName();
    });
    if (found != old_fields.end()) {
      size_t old_index = std::distance(old_fields.begin(), found);
      if (old_index != i) {
        AIDL_ERROR(new_field) << "Reordered " << new_field->GetName() << " from " << old_index
                              << " to " << i << ".";
        compatible = false;
      }
    }
  }

  return compatible;
}

static bool are_compatible_enums(const AidlEnumDeclaration& older,
                                 const AidlEnumDeclaration& newer) {
  if (!are_compatible_types(older.GetBackingType(), newer.GetBackingType())) {
    AIDL_ERROR(newer) << "Changed backing types.";
    return false;
  }

  std::map<std::string, const AidlConstantValue*> old_enum_map;
  for (const auto& enumerator : older.GetEnumerators()) {
    old_enum_map[enumerator->GetName()] = enumerator->GetValue();
  }
  std::map<std::string, const AidlConstantValue*> new_enum_map;
  for (const auto& enumerator : newer.GetEnumerators()) {
    new_enum_map[enumerator->GetName()] = enumerator->GetValue();
  }

  bool compatible = true;
  for (const auto& [name, value] : old_enum_map) {
    if (new_enum_map.find(name) == new_enum_map.end()) {
      AIDL_ERROR(newer) << "Removed enumerator from " << older.GetCanonicalName() << ": " << name;
      compatible = false;
      continue;
    }
    const string old_value =
        old_enum_map[name]->ValueString(older.GetBackingType(), AidlConstantValueDecorator);
    const string new_value =
        new_enum_map[name]->ValueString(newer.GetBackingType(), AidlConstantValueDecorator);
    if (old_value != new_value) {
      AIDL_ERROR(newer) << "Changed enumerator value: " << older.GetCanonicalName() << "::" << name
                        << " from " << old_value << " to " << new_value << ".";
      compatible = false;
    }
  }
  return compatible;
}

bool check_api(const Options& options, const IoDelegate& io_delegate) {
  CHECK(options.IsStructured());
  CHECK(options.InputFiles().size() == 2) << "--checkapi requires two inputs "
                                          << "but got " << options.InputFiles().size();
  AidlTypenames old_tns;
  const string old_dir = options.InputFiles().at(0);
  vector<AidlDefinedType*> old_types;
  vector<string> old_files = io_delegate.ListFiles(old_dir);
  if (old_files.size() == 0) {
    AIDL_ERROR(old_dir) << "No API file exist";
    return false;
  }
  for (const auto& file : old_files) {
    if (!android::base::EndsWith(file, ".aidl")) continue;

    vector<AidlDefinedType*> types;
    if (internals::load_and_validate_aidl(file, options, io_delegate, &old_tns, &types,
                                          nullptr /* imported_files */) != AidlError::OK) {
      AIDL_ERROR(file) << "Failed to read.";
      return false;
    }
    old_types.insert(old_types.end(), types.begin(), types.end());
  }

  AidlTypenames new_tns;
  const string new_dir = options.InputFiles().at(1);
  vector<AidlDefinedType*> new_types;
  vector<string> new_files = io_delegate.ListFiles(new_dir);
  if (new_files.size() == 0) {
    AIDL_ERROR(new_dir) << "No API file exist";
    return false;
  }
  for (const auto& file : new_files) {
    if (!android::base::EndsWith(file, ".aidl")) continue;

    vector<AidlDefinedType*> types;
    if (internals::load_and_validate_aidl(file, options, io_delegate, &new_tns, &types,
                                          nullptr /* imported_files */) != AidlError::OK) {
      AIDL_ERROR(file) << "Failed to read.";
      return false;
    }
    new_types.insert(new_types.end(), types.begin(), types.end());
  }

  map<string, AidlDefinedType*> new_map;
  for (const auto t : new_types) {
    new_map.emplace(t->GetCanonicalName(), t);
  }

  bool compatible = true;
  for (const auto old_type : old_types) {
    const auto found = new_map.find(old_type->GetCanonicalName());
    if (found == new_map.end()) {
      AIDL_ERROR(old_type) << "Removed type: " << old_type->GetCanonicalName();
      compatible = false;
      continue;
    }
    const auto new_type = found->second;

    if (old_type->AsInterface() != nullptr) {
      if (new_type->AsInterface() == nullptr) {
        AIDL_ERROR(new_type) << "Type mismatch: " << old_type->GetCanonicalName()
                             << " is changed from " << old_type->GetPreprocessDeclarationName()
                             << " to " << new_type->GetPreprocessDeclarationName();
        compatible = false;
        continue;
      }
      compatible &=
          are_compatible_interfaces(*(old_type->AsInterface()), *(new_type->AsInterface()));
    } else if (old_type->AsStructuredParcelable() != nullptr) {
      if (new_type->AsStructuredParcelable() == nullptr) {
        AIDL_ERROR(new_type) << "Parcelable" << new_type->GetCanonicalName()
                             << " is not structured. ";
        compatible = false;
        continue;
      }
      compatible &= are_compatible_parcelables(*(old_type->AsStructuredParcelable()),
                                               *(new_type->AsStructuredParcelable()));
    } else if (old_type->AsEnumDeclaration() != nullptr) {
      if (new_type->AsEnumDeclaration() == nullptr) {
        AIDL_ERROR(new_type) << "Type mismatch: " << old_type->GetCanonicalName()
                             << " is changed from " << old_type->GetPreprocessDeclarationName()
                             << " to " << new_type->GetPreprocessDeclarationName();
        compatible = false;
        continue;
      }
      compatible &=
          are_compatible_enums(*(old_type->AsEnumDeclaration()), *(new_type->AsEnumDeclaration()));
    } else {
      AIDL_ERROR(old_type) << "Unsupported type " << old_type->GetPreprocessDeclarationName()
                           << " for " << old_type->GetCanonicalName();
      compatible = false;
    }
  }

  return compatible;
}

}  // namespace aidl
}  // namespace android
