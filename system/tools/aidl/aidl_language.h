/*
 * Copyright (C) 2019, The Android Open Source Project
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

#pragma once

#include "aidl_typenames.h"
#include "code_writer.h"
#include "io_delegate.h"
#include "options.h"

#include <memory>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include <android-base/macros.h>
#include <android-base/strings.h>

struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;

using android::aidl::AidlTypenames;
using android::aidl::CodeWriter;
using android::aidl::Options;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
class AidlNode;

namespace android {
namespace aidl {
namespace mappings {
std::string dump_location(const AidlNode& method);
}  // namespace mappings
namespace java {
std::string dump_location(const AidlNode& method);
}  // namespace java
}  // namespace aidl
}  // namespace android

class AidlToken {
 public:
  AidlToken(const std::string& text, const std::string& comments);

  const std::string& GetText() const { return text_; }
  const std::string& GetComments() const { return comments_; }

 private:
  std::string text_;
  std::string comments_;

  DISALLOW_COPY_AND_ASSIGN(AidlToken);
};

class AidlLocation {
 public:
  struct Point {
    int line;
    int column;
  };

  AidlLocation(const std::string& file, Point begin, Point end);

  friend std::ostream& operator<<(std::ostream& os, const AidlLocation& l);
  friend class AidlNode;

 private:
  const std::string file_;
  Point begin_;
  Point end_;
};

#define AIDL_LOCATION_HERE                   \
  AidlLocation {                             \
    __FILE__, {__LINE__, 0}, { __LINE__, 0 } \
  }

std::ostream& operator<<(std::ostream& os, const AidlLocation& l);

// Anything that is locatable in a .aidl file.
class AidlNode {
 public:
  AidlNode(const AidlLocation& location);

  AidlNode(const AidlNode&) = default;
  AidlNode(AidlNode&&) = default;
  virtual ~AidlNode() = default;

  // DO NOT ADD. This is intentionally omitted. Nothing should refer to the location
  // for a functional purpose. It is only for error messages.
  // NO const AidlLocation& GetLocation() const { return location_; } NO

  // To be able to print AidlLocation (nothing else should use this information)
  friend class AidlError;
  friend std::string android::aidl::mappings::dump_location(const AidlNode&);
  friend std::string android::aidl::java::dump_location(const AidlNode&);

 private:
  std::string PrintLine() const;
  std::string PrintLocation() const;
  const AidlLocation location_;
};

// Generic point for printing any error in the AIDL compiler.
class AidlError {
 public:
  AidlError(bool fatal, const std::string& filename) : AidlError(fatal) { os_ << filename << ": "; }
  AidlError(bool fatal, const AidlLocation& location) : AidlError(fatal) {
    os_ << location << ": ";
  }
  AidlError(bool fatal, const AidlNode& node) : AidlError(fatal, node.location_) {}
  AidlError(bool fatal, const AidlNode* node) : AidlError(fatal, *node) {}

  template <typename T>
  AidlError(bool fatal, const std::unique_ptr<T>& node) : AidlError(fatal, *node) {}
  ~AidlError() {
    os_ << std::endl;
    if (fatal_) abort();
  }

  std::ostream& os_;

  static bool hadError() { return sHadError; }

 private:
  AidlError(bool fatal);

  bool fatal_;

  static bool sHadError;

  DISALLOW_COPY_AND_ASSIGN(AidlError);
};

#define AIDL_ERROR(CONTEXT) ::AidlError(false /*fatal*/, (CONTEXT)).os_
#define AIDL_FATAL(CONTEXT) ::AidlError(true /*fatal*/, (CONTEXT)).os_
#define AIDL_FATAL_IF(CONDITION, CONTEXT) \
  if (CONDITION) AIDL_FATAL(CONTEXT) << "Bad internal state: " << #CONDITION << ": "

namespace android {
namespace aidl {

class AidlTypenames;

}  // namespace aidl
}  // namespace android

// unique_ptr<AidlTypeSpecifier> for type arugment,
// std::string for type parameter(T, U, and so on).
template <typename T>
class AidlParameterizable {
 public:
  AidlParameterizable(std::vector<T>* type_params) : type_params_(type_params) {}
  virtual ~AidlParameterizable() = default;
  bool IsGeneric() const { return type_params_ != nullptr; }
  const std::vector<T>& GetTypeParameters() const { return *type_params_; }
  bool CheckValid() const;

  virtual const AidlNode& AsAidlNode() const = 0;

 protected:
  AidlParameterizable(const AidlParameterizable&);

 private:
  const unique_ptr<std::vector<T>> type_params_;
  static_assert(std::is_same<T, unique_ptr<AidlTypeSpecifier>>::value ||
                std::is_same<T, std::string>::value);
};
template <>
bool AidlParameterizable<std::string>::CheckValid() const;

class AidlConstantValue;
class AidlConstantDeclaration;

// Transforms a value string into a language specific form. Raw value as produced by
// AidlConstantValue.
using ConstantValueDecorator =
    std::function<std::string(const AidlTypeSpecifier& type, const std::string& raw_value)>;

class AidlAnnotation : public AidlNode {
 public:
  static AidlAnnotation* Parse(
      const AidlLocation& location, const string& name,
      std::map<std::string, std::shared_ptr<AidlConstantValue>>* parameter_list);

  AidlAnnotation(const AidlAnnotation&) = default;
  AidlAnnotation(AidlAnnotation&&) = default;
  virtual ~AidlAnnotation() = default;
  bool CheckValid() const;

  const string& GetName() const { return name_; }
  string ToString(const ConstantValueDecorator& decorator) const;
  std::map<std::string, std::string> AnnotationParams(
      const ConstantValueDecorator& decorator) const;
  const string& GetComments() const { return comments_; }
  void SetComments(const string& comments) { comments_ = comments; }

 private:
  AidlAnnotation(const AidlLocation& location, const string& name);
  AidlAnnotation(const AidlLocation& location, const string& name,
                 std::map<std::string, std::shared_ptr<AidlConstantValue>>&& parameters);
  const string name_;
  string comments_;
  std::map<std::string, std::shared_ptr<AidlConstantValue>> parameters_;
};

static inline bool operator<(const AidlAnnotation& lhs, const AidlAnnotation& rhs) {
  return lhs.GetName() < rhs.GetName();
}
static inline bool operator==(const AidlAnnotation& lhs, const AidlAnnotation& rhs) {
  return lhs.GetName() == rhs.GetName();
}

class AidlAnnotatable : public AidlNode {
 public:
  AidlAnnotatable(const AidlLocation& location);

  AidlAnnotatable(const AidlAnnotatable&) = default;
  AidlAnnotatable(AidlAnnotatable&&) = default;
  virtual ~AidlAnnotatable() = default;

  void Annotate(vector<AidlAnnotation>&& annotations) {
    for (auto& annotation : annotations) {
      annotations_.emplace_back(std::move(annotation));
    }
  }
  bool IsNullable() const;
  bool IsUtf8InCpp() const;
  bool IsVintfStability() const;
  bool IsStableApiParcelable(Options::Language lang) const;
  bool IsHide() const;

  void DumpAnnotations(CodeWriter* writer) const;

  const AidlAnnotation* UnsupportedAppUsage() const;
  const AidlTypeSpecifier* BackingType(const AidlTypenames& typenames) const;
  std::string ToString() const;

  const vector<AidlAnnotation>& GetAnnotations() const { return annotations_; }
  bool CheckValidAnnotations() const;

 private:
  vector<AidlAnnotation> annotations_;
};

class AidlQualifiedName;

// AidlTypeSpecifier represents a reference to either a built-in type,
// a defined type, or a variant (e.g., array of generic) of a type.
class AidlTypeSpecifier final : public AidlAnnotatable,
                                public AidlParameterizable<unique_ptr<AidlTypeSpecifier>> {
 public:
  AidlTypeSpecifier(const AidlLocation& location, const string& unresolved_name, bool is_array,
                    vector<unique_ptr<AidlTypeSpecifier>>* type_params, const string& comments);
  virtual ~AidlTypeSpecifier() = default;

  // Copy of this type which is not an array.
  AidlTypeSpecifier ArrayBase() const;

  // Returns the full-qualified name of the base type.
  // int -> int
  // int[] -> int
  // List<String> -> List
  // IFoo -> foo.bar.IFoo (if IFoo is in package foo.bar)
  const string& GetName() const {
    if (IsResolved()) {
      return fully_qualified_name_;
    } else {
      return GetUnresolvedName();
    }
  }

  // Returns string representation of this type specifier.
  // This is GetBaseTypeName() + array modifier or generic type parameters
  string ToString() const;

  std::string Signature() const;

  const string& GetUnresolvedName() const { return unresolved_name_; }

  bool IsHidden() const;

  const string& GetComments() const { return comments_; }

  const std::vector<std::string> GetSplitName() const { return split_name_; }

  void SetComments(const string& comment) { comments_ = comment; }

  bool IsResolved() const { return fully_qualified_name_ != ""; }

  bool IsArray() const { return is_array_; }

  // Resolve the base type name to a fully-qualified name. Return false if the
  // resolution fails.
  bool Resolve(const AidlTypenames& typenames);

  bool CheckValid(const AidlTypenames& typenames) const;
  bool LanguageSpecificCheckValid(Options::Language lang) const;
  const AidlNode& AsAidlNode() const override { return *this; }

 private:
  AidlTypeSpecifier(const AidlTypeSpecifier&) = default;

  const string unresolved_name_;
  string fully_qualified_name_;
  bool is_array_;
  string comments_;
  vector<string> split_name_;
};

// Returns the universal value unaltered.
std::string AidlConstantValueDecorator(const AidlTypeSpecifier& type, const std::string& raw_value);

class AidlConstantValue;
class AidlVariableDeclaration : public AidlNode {
 public:
  AidlVariableDeclaration(const AidlLocation& location, AidlTypeSpecifier* type,
                          const std::string& name);
  AidlVariableDeclaration(const AidlLocation& location, AidlTypeSpecifier* type,
                          const std::string& name, AidlConstantValue* default_value);
  virtual ~AidlVariableDeclaration() = default;

  std::string GetName() const { return name_; }
  const AidlTypeSpecifier& GetType() const { return *type_; }
  const AidlConstantValue* GetDefaultValue() const { return default_value_.get(); }

  AidlTypeSpecifier* GetMutableType() { return type_.get(); }

  bool CheckValid(const AidlTypenames& typenames) const;
  std::string ToString() const;
  std::string Signature() const;

  std::string ValueString(const ConstantValueDecorator& decorator) const;

 private:
  std::unique_ptr<AidlTypeSpecifier> type_;
  std::string name_;
  std::unique_ptr<AidlConstantValue> default_value_;

  DISALLOW_COPY_AND_ASSIGN(AidlVariableDeclaration);
};

class AidlArgument : public AidlVariableDeclaration {
 public:
  enum Direction { IN_DIR = 1, OUT_DIR = 2, INOUT_DIR = 3 };

  AidlArgument(const AidlLocation& location, AidlArgument::Direction direction,
               AidlTypeSpecifier* type, const std::string& name);
  AidlArgument(const AidlLocation& location, AidlTypeSpecifier* type, const std::string& name);
  virtual ~AidlArgument() = default;

  Direction GetDirection() const { return direction_; }
  bool IsOut() const { return direction_ & OUT_DIR; }
  bool IsIn() const { return direction_ & IN_DIR; }
  bool DirectionWasSpecified() const { return direction_specified_; }
  string GetDirectionSpecifier() const;

  std::string ToString() const;
  std::string Signature() const;

 private:
  Direction direction_;
  bool direction_specified_;

  DISALLOW_COPY_AND_ASSIGN(AidlArgument);
};

class AidlMethod;
class AidlConstantDeclaration;
class AidlEnumDeclaration;
class AidlMember : public AidlNode {
 public:
  AidlMember(const AidlLocation& location);
  virtual ~AidlMember() = default;

  virtual AidlMethod* AsMethod() { return nullptr; }
  virtual AidlConstantDeclaration* AsConstantDeclaration() { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AidlMember);
};

class AidlUnaryConstExpression;
class AidlBinaryConstExpression;

class AidlConstantValue : public AidlNode {
 public:
  enum class Type {
    // WARNING: Don't change this order! The order is used to determine type
    // promotion during a binary expression.
    BOOLEAN,
    INT8,
    INT32,
    INT64,
    ARRAY,
    CHARACTER,
    STRING,
    FLOATING,
    UNARY,
    BINARY,
    ERROR,
  };

  /*
   * Return the value casted to the given type.
   */
  template <typename T>
  T cast() const;

  virtual ~AidlConstantValue() = default;

  static AidlConstantValue* Boolean(const AidlLocation& location, bool value);
  static AidlConstantValue* Character(const AidlLocation& location, char value);
  // example: 123, -5498, maybe any size
  static AidlConstantValue* Integral(const AidlLocation& location, const string& value);
  static AidlConstantValue* Floating(const AidlLocation& location, const std::string& value);
  static AidlConstantValue* Array(const AidlLocation& location,
                                  std::unique_ptr<vector<unique_ptr<AidlConstantValue>>> values);
  // example: "\"asdf\""
  static AidlConstantValue* String(const AidlLocation& location, const string& value);

  // Construct an AidlConstantValue by evaluating the other integral constant's
  // value string. This does not preserve the structure of the copied constant.
  // Returns nullptr and logs if value cannot be copied.
  static AidlConstantValue* ShallowIntegralCopy(const AidlConstantValue& other);

  Type GetType() const { return final_type_; }

  virtual bool CheckValid() const;

  // Raw value of type (currently valid in C++ and Java). Empty string on error.
  string ValueString(const AidlTypeSpecifier& type, const ConstantValueDecorator& decorator) const;

 private:
  AidlConstantValue(const AidlLocation& location, Type parsed_type, int64_t parsed_value,
                    const string& checked_value);
  AidlConstantValue(const AidlLocation& location, Type type, const string& checked_value);
  AidlConstantValue(const AidlLocation& location, Type type,
                    std::unique_ptr<vector<unique_ptr<AidlConstantValue>>> values);
  static string ToString(Type type);
  static bool ParseIntegral(const string& value, int64_t* parsed_value, Type* parsed_type);
  static bool IsHex(const string& value);

  virtual bool evaluate(const AidlTypeSpecifier& type) const;

  const Type type_ = Type::ERROR;
  const vector<unique_ptr<AidlConstantValue>> values_;  // if type_ == ARRAY
  const string value_;                                  // otherwise

  // State for tracking evaluation of expressions
  mutable bool is_valid_ = false;      // cache of CheckValid, but may be marked false in evaluate
  mutable bool is_evaluated_ = false;  // whether evaluate has been called
  mutable Type final_type_;
  mutable int64_t final_value_;
  mutable string final_string_value_ = "";

  DISALLOW_COPY_AND_ASSIGN(AidlConstantValue);

  friend AidlUnaryConstExpression;
  friend AidlBinaryConstExpression;
};

class AidlUnaryConstExpression : public AidlConstantValue {
 public:
  AidlUnaryConstExpression(const AidlLocation& location, const string& op,
                           std::unique_ptr<AidlConstantValue> rval);

  static bool IsCompatibleType(Type type, const string& op);
  bool CheckValid() const override;
 private:
  bool evaluate(const AidlTypeSpecifier& type) const override;

  std::unique_ptr<AidlConstantValue> unary_;
  const string op_;
};

class AidlBinaryConstExpression : public AidlConstantValue {
 public:
  AidlBinaryConstExpression(const AidlLocation& location, std::unique_ptr<AidlConstantValue> lval,
                            const string& op, std::unique_ptr<AidlConstantValue> rval);

  bool CheckValid() const override;

  static bool AreCompatibleTypes(Type t1, Type t2);
  // Returns the promoted kind for both operands
  static Type UsualArithmeticConversion(Type left, Type right);
  // Returns the promoted integral type where INT32 is the smallest type
  static Type IntegralPromotion(Type in);

 private:
  bool evaluate(const AidlTypeSpecifier& type) const override;

  std::unique_ptr<AidlConstantValue> left_val_;
  std::unique_ptr<AidlConstantValue> right_val_;
  const string op_;
};

struct AidlAnnotationParameter {
  std::string name;
  std::unique_ptr<AidlConstantValue> value;
};

class AidlConstantDeclaration : public AidlMember {
 public:
  AidlConstantDeclaration(const AidlLocation& location, AidlTypeSpecifier* specifier,
                          const string& name, AidlConstantValue* value);
  virtual ~AidlConstantDeclaration() = default;

  const AidlTypeSpecifier& GetType() const { return *type_; }
  AidlTypeSpecifier* GetMutableType() { return type_.get(); }
  const string& GetName() const { return name_; }
  const AidlConstantValue& GetValue() const { return *value_; }
  bool CheckValid(const AidlTypenames& typenames) const;

  string ToString() const;
  string Signature() const;
  string ValueString(const ConstantValueDecorator& decorator) const {
    return value_->ValueString(GetType(), decorator);
  }

  AidlConstantDeclaration* AsConstantDeclaration() override { return this; }

 private:
  const unique_ptr<AidlTypeSpecifier> type_;
  const string name_;
  unique_ptr<AidlConstantValue> value_;

  DISALLOW_COPY_AND_ASSIGN(AidlConstantDeclaration);
};

class AidlMethod : public AidlMember {
 public:
  AidlMethod(const AidlLocation& location, bool oneway, AidlTypeSpecifier* type, const string& name,
             vector<unique_ptr<AidlArgument>>* args, const string& comments);
  AidlMethod(const AidlLocation& location, bool oneway, AidlTypeSpecifier* type, const string& name,
             vector<unique_ptr<AidlArgument>>* args, const string& comments, int id,
             bool is_user_defined = true);
  virtual ~AidlMethod() = default;

  AidlMethod* AsMethod() override { return this; }
  bool IsHidden() const;
  const string& GetComments() const { return comments_; }
  const AidlTypeSpecifier& GetType() const { return *type_; }
  AidlTypeSpecifier* GetMutableType() { return type_.get(); }

  // set if this method is part of an interface that is marked oneway
  void ApplyInterfaceOneway(bool oneway) { oneway_ = oneway_ || oneway; }
  bool IsOneway() const { return oneway_; }

  const std::string& GetName() const { return name_; }
  bool HasId() const { return has_id_; }
  int GetId() const { return id_; }
  void SetId(unsigned id) { id_ = id; }

  bool IsUserDefined() const { return is_user_defined_; }

  const std::vector<std::unique_ptr<AidlArgument>>& GetArguments() const {
    return arguments_;
  }
  // An inout parameter will appear in both GetInArguments()
  // and GetOutArguments().  AidlMethod retains ownership of the argument
  // pointers returned in this way.
  const std::vector<const AidlArgument*>& GetInArguments() const {
    return in_arguments_;
  }
  const std::vector<const AidlArgument*>& GetOutArguments() const {
    return out_arguments_;
  }

  // name + type parameter types
  // i.e, foo(int, String)
  std::string Signature() const;

  // return type + name + type parameter types + annotations
  // i.e, boolean foo(int, @Nullable String)
  std::string ToString() const;

 private:
  bool oneway_;
  std::string comments_;
  std::unique_ptr<AidlTypeSpecifier> type_;
  std::string name_;
  const std::vector<std::unique_ptr<AidlArgument>> arguments_;
  std::vector<const AidlArgument*> in_arguments_;
  std::vector<const AidlArgument*> out_arguments_;
  bool has_id_;
  int id_;
  bool is_user_defined_ = true;

  DISALLOW_COPY_AND_ASSIGN(AidlMethod);
};

class AidlDefinedType;
class AidlInterface;
class AidlParcelable;
class AidlStructuredParcelable;

class AidlQualifiedName : public AidlNode {
 public:
  AidlQualifiedName(const AidlLocation& location, const std::string& term,
                    const std::string& comments);
  virtual ~AidlQualifiedName() = default;

  const std::vector<std::string>& GetTerms() const { return terms_; }
  const std::string& GetComments() const { return comments_; }
  std::string GetDotName() const { return android::base::Join(terms_, '.'); }
  std::string GetColonName() const { return android::base::Join(terms_, "::"); }

  void AddTerm(const std::string& term);

 private:
  std::vector<std::string> terms_;
  std::string comments_;

  DISALLOW_COPY_AND_ASSIGN(AidlQualifiedName);
};

class AidlInterface;
class AidlParcelable;
class AidlStructuredParcelable;
// AidlDefinedType represents either an interface, parcelable, or enum that is
// defined in the source file.
class AidlDefinedType : public AidlAnnotatable {
 public:
  AidlDefinedType(const AidlLocation& location, const std::string& name,
                  const std::string& comments, const std::vector<std::string>& package);
  virtual ~AidlDefinedType() = default;

  const std::string& GetName() const { return name_; };
  bool IsHidden() const;
  const std::string& GetComments() const { return comments_; }
  void SetComments(const std::string comments) { comments_ = comments; }

  /* dot joined package, example: "android.package.foo" */
  std::string GetPackage() const;
  /* dot joined package and name, example: "android.package.foo.IBar" */
  std::string GetCanonicalName() const;
  const std::vector<std::string>& GetSplitPackage() const { return package_; }

  virtual std::string GetPreprocessDeclarationName() const = 0;

  virtual const AidlStructuredParcelable* AsStructuredParcelable() const { return nullptr; }
  virtual const AidlParcelable* AsParcelable() const { return nullptr; }
  virtual const AidlEnumDeclaration* AsEnumDeclaration() const { return nullptr; }
  virtual const AidlInterface* AsInterface() const { return nullptr; }
  virtual const AidlParameterizable<std::string>* AsParameterizable() const { return nullptr; }
  virtual bool CheckValid(const AidlTypenames&) const { return CheckValidAnnotations(); }
  virtual bool LanguageSpecificCheckValid(Options::Language lang) const = 0;
  AidlStructuredParcelable* AsStructuredParcelable() {
    return const_cast<AidlStructuredParcelable*>(
        const_cast<const AidlDefinedType*>(this)->AsStructuredParcelable());
  }
  AidlParcelable* AsParcelable() {
    return const_cast<AidlParcelable*>(const_cast<const AidlDefinedType*>(this)->AsParcelable());
  }
  AidlEnumDeclaration* AsEnumDeclaration() {
    return const_cast<AidlEnumDeclaration*>(
        const_cast<const AidlDefinedType*>(this)->AsEnumDeclaration());
  }
  AidlInterface* AsInterface() {
    return const_cast<AidlInterface*>(const_cast<const AidlDefinedType*>(this)->AsInterface());
  }

  AidlParameterizable<std::string>* AsParameterizable() {
    return const_cast<AidlParameterizable<std::string>*>(
        const_cast<const AidlDefinedType*>(this)->AsParameterizable());
  }

  const AidlParcelable* AsUnstructuredParcelable() const {
    if (this->AsStructuredParcelable() != nullptr) return nullptr;
    return this->AsParcelable();
  }
  AidlParcelable* AsUnstructuredParcelable() {
    return const_cast<AidlParcelable*>(
        const_cast<const AidlDefinedType*>(this)->AsUnstructuredParcelable());
  }

  virtual void Dump(CodeWriter* writer) const = 0;
  void DumpHeader(CodeWriter* writer) const;

 private:
  std::string name_;
  std::string comments_;
  const std::vector<std::string> package_;

  DISALLOW_COPY_AND_ASSIGN(AidlDefinedType);
};

class AidlParcelable : public AidlDefinedType, public AidlParameterizable<std::string> {
 public:
  AidlParcelable(const AidlLocation& location, AidlQualifiedName* name,
                 const std::vector<std::string>& package, const std::string& comments,
                 const std::string& cpp_header = "",
                 std::vector<std::string>* type_params = nullptr);
  virtual ~AidlParcelable() = default;

  // C++ uses "::" instead of "." to refer to a inner class.
  std::string GetCppName() const { return name_->GetColonName(); }
  std::string GetCppHeader() const { return cpp_header_; }

  bool CheckValid(const AidlTypenames& typenames) const override;
  bool LanguageSpecificCheckValid(Options::Language lang) const override;
  const AidlParcelable* AsParcelable() const override { return this; }
  const AidlParameterizable<std::string>* AsParameterizable() const override { return this; }
  const AidlNode& AsAidlNode() const override { return *this; }
  std::string GetPreprocessDeclarationName() const override { return "parcelable"; }

  void Dump(CodeWriter* writer) const override;

 private:
  std::unique_ptr<AidlQualifiedName> name_;
  std::string cpp_header_;

  DISALLOW_COPY_AND_ASSIGN(AidlParcelable);
};

class AidlStructuredParcelable : public AidlParcelable {
 public:
  AidlStructuredParcelable(const AidlLocation& location, AidlQualifiedName* name,
                           const std::vector<std::string>& package, const std::string& comments,
                           std::vector<std::unique_ptr<AidlVariableDeclaration>>* variables);

  const std::vector<std::unique_ptr<AidlVariableDeclaration>>& GetFields() const {
    return variables_;
  }

  const AidlStructuredParcelable* AsStructuredParcelable() const override { return this; }
  std::string GetPreprocessDeclarationName() const override { return "structured_parcelable"; }

  void Dump(CodeWriter* writer) const override;

  bool CheckValid(const AidlTypenames& typenames) const override;
  bool LanguageSpecificCheckValid(Options::Language lang) const override;

 private:
  const std::vector<std::unique_ptr<AidlVariableDeclaration>> variables_;

  DISALLOW_COPY_AND_ASSIGN(AidlStructuredParcelable);
};

class AidlEnumerator : public AidlNode {
 public:
  AidlEnumerator(const AidlLocation& location, const std::string& name, AidlConstantValue* value,
                 const std::string& comments);
  virtual ~AidlEnumerator() = default;

  const std::string& GetName() const { return name_; }
  AidlConstantValue* GetValue() const { return value_.get(); }
  const std::string& GetComments() const { return comments_; }
  bool CheckValid(const AidlTypeSpecifier& enum_backing_type) const;

  string ValueString(const AidlTypeSpecifier& backing_type,
                     const ConstantValueDecorator& decorator) const;

  void SetValue(std::unique_ptr<AidlConstantValue> value) { value_ = std::move(value); }

 private:
  const std::string name_;
  unique_ptr<AidlConstantValue> value_;
  const std::string comments_;

  DISALLOW_COPY_AND_ASSIGN(AidlEnumerator);
};

class AidlEnumDeclaration : public AidlDefinedType {
 public:
  AidlEnumDeclaration(const AidlLocation& location, const string& name,
                      std::vector<std::unique_ptr<AidlEnumerator>>* enumerators,
                      const std::vector<std::string>& package, const std::string& comments);
  virtual ~AidlEnumDeclaration() = default;

  void SetBackingType(std::unique_ptr<const AidlTypeSpecifier> type);
  const AidlTypeSpecifier& GetBackingType() const { return *backing_type_; }
  const std::vector<std::unique_ptr<AidlEnumerator>>& GetEnumerators() const {
    return enumerators_;
  }
  bool Autofill();
  bool CheckValid(const AidlTypenames& typenames) const override;
  bool LanguageSpecificCheckValid(Options::Language) const override { return true; }
  std::string GetPreprocessDeclarationName() const override { return "enum"; }
  void Dump(CodeWriter* writer) const override;

  const AidlEnumDeclaration* AsEnumDeclaration() const override { return this; }

 private:
  const std::string name_;
  const std::vector<std::unique_ptr<AidlEnumerator>> enumerators_;
  std::unique_ptr<const AidlTypeSpecifier> backing_type_;

  DISALLOW_COPY_AND_ASSIGN(AidlEnumDeclaration);
};

class AidlInterface final : public AidlDefinedType {
 public:
  AidlInterface(const AidlLocation& location, const std::string& name, const std::string& comments,
                bool oneway_, std::vector<std::unique_ptr<AidlMember>>* members,
                const std::vector<std::string>& package);
  virtual ~AidlInterface() = default;

  const std::vector<std::unique_ptr<AidlMethod>>& GetMethods() const
      { return methods_; }
  std::vector<std::unique_ptr<AidlMethod>>& GetMutableMethods() { return methods_; }
  const std::vector<std::unique_ptr<AidlConstantDeclaration>>& GetConstantDeclarations() const {
    return constants_;
  }

  const AidlInterface* AsInterface() const override { return this; }
  std::string GetPreprocessDeclarationName() const override { return "interface"; }

  void Dump(CodeWriter* writer) const override;

  bool CheckValid(const AidlTypenames& typenames) const override;
  bool LanguageSpecificCheckValid(Options::Language lang) const override;

 private:
  std::vector<std::unique_ptr<AidlMethod>> methods_;
  std::vector<std::unique_ptr<AidlConstantDeclaration>> constants_;

  DISALLOW_COPY_AND_ASSIGN(AidlInterface);
};

class AidlImport : public AidlNode {
 public:
  AidlImport(const AidlLocation& location, const std::string& needed_class);
  virtual ~AidlImport() = default;

  const std::string& GetFilename() const { return filename_; }
  const std::string& GetNeededClass() const { return needed_class_; }

 private:
  std::string filename_;
  std::string needed_class_;

  DISALLOW_COPY_AND_ASSIGN(AidlImport);
};

class Parser {
 public:
  ~Parser();

  // Parse contents of file |filename|. Should only be called once.
  static std::unique_ptr<Parser> Parse(const std::string& filename,
                                       const android::aidl::IoDelegate& io_delegate,
                                       AidlTypenames& typenames);

  void AddError() { error_++; }
  bool HasError() { return error_ != 0; }

  const std::string& FileName() const { return filename_; }
  void* Scanner() const { return scanner_; }

  void AddImport(std::unique_ptr<AidlImport>&& import);
  const std::vector<std::unique_ptr<AidlImport>>& GetImports() {
    return imports_;
  }

  void SetPackage(unique_ptr<AidlQualifiedName> name) { package_ = std::move(name); }
  std::vector<std::string> Package() const;

  void DeferResolution(AidlTypeSpecifier* typespec) {
    unresolved_typespecs_.emplace_back(typespec);
  }

  const vector<AidlTypeSpecifier*>& GetUnresolvedTypespecs() const { return unresolved_typespecs_; }

  bool Resolve();

  void AddDefinedType(unique_ptr<AidlDefinedType> type) {
    // Parser does NOT own AidlDefinedType, it just has references to the types
    // that it encountered while parsing the input file.
    defined_types_.emplace_back(type.get());

    // AidlDefinedType IS owned by AidlTypenames
    if (!typenames_.AddDefinedType(std::move(type))) {
      AddError();
    }
  }

  vector<AidlDefinedType*>& GetDefinedTypes() { return defined_types_; }

 private:
  explicit Parser(const std::string& filename, std::string& raw_buffer,
                  android::aidl::AidlTypenames& typenames);

  std::string filename_;
  std::unique_ptr<AidlQualifiedName> package_;
  AidlTypenames& typenames_;

  void* scanner_ = nullptr;
  YY_BUFFER_STATE buffer_;
  int error_ = 0;

  std::vector<std::unique_ptr<AidlImport>> imports_;
  vector<AidlDefinedType*> defined_types_;
  vector<AidlTypeSpecifier*> unresolved_typespecs_;

  DISALLOW_COPY_AND_ASSIGN(Parser);
};
