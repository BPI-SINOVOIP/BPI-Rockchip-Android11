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

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum {
  PACKAGE_PRIVATE = 0x00000000,
  PUBLIC = 0x00000001,
  PRIVATE = 0x00000002,
  PROTECTED = 0x00000003,
  SCOPE_MASK = 0x00000003,

  STATIC = 0x00000010,
  FINAL = 0x00000020,
  ABSTRACT = 0x00000040,

  OVERRIDE = 0x00000100,

  ALL_MODIFIERS = 0xffffffff
};

namespace android {
namespace aidl {
class CodeWriter;
}  // namespace aidl
}  // namespace android

namespace android {
namespace aidl {
namespace java {

// Write the modifiers that are set in both mod and mask
void WriteModifiers(CodeWriter* to, int mod, int mask);

struct AstNode {
  AstNode() = default;
  virtual ~AstNode() = default;
  virtual void Write(CodeWriter* to) const = 0;
  std::string ToString();
};

struct ClassElement : public AstNode {
  ClassElement() = default;
  virtual ~ClassElement() = default;
};

struct Expression : public AstNode {
  virtual ~Expression() = default;
};

struct LiteralExpression : public Expression {
  std::string value;

  explicit LiteralExpression(const std::string& value);
  virtual ~LiteralExpression() = default;
  void Write(CodeWriter* to) const override;
};

// TODO: also escape the contents.  not needed for now
struct StringLiteralExpression : public Expression {
  std::string value;

  explicit StringLiteralExpression(const std::string& value);
  virtual ~StringLiteralExpression() = default;
  void Write(CodeWriter* to) const override;
};

struct Variable : public Expression {
  const std::string type;
  std::string name;

  Variable() = default;
  Variable(const std::string& type, const std::string& name);
  virtual ~Variable() = default;

  void WriteDeclaration(CodeWriter* to) const;
  void Write(CodeWriter* to) const;
};

struct FieldVariable : public Expression {
  std::variant<std::shared_ptr<Expression>, std::string> receiver;
  std::string name;

  FieldVariable(std::shared_ptr<Expression> object, const std::string& name);
  FieldVariable(const std::string& clazz, const std::string& name);
  virtual ~FieldVariable() = default;

  void Write(CodeWriter* to) const;
};

struct Field : public ClassElement {
  std::string comment;
  std::vector<std::string> annotations;
  int modifiers = 0;
  std::shared_ptr<Variable> variable = nullptr;
  std::string value;

  Field() = default;
  Field(int modifiers, std::shared_ptr<Variable> variable);
  virtual ~Field() = default;

  void Write(CodeWriter* to) const override;
};

struct Statement : public AstNode {
  virtual ~Statement() = default;
};

struct LiteralStatement : public Statement {
 public:
  LiteralStatement(const std::string& value);
  virtual ~LiteralStatement() = default;
  void Write(CodeWriter* to) const override;

 private:
  const std::string value_;
};

struct StatementBlock : public Statement {
  std::vector<std::shared_ptr<Statement>> statements;

  StatementBlock() = default;
  virtual ~StatementBlock() = default;
  void Write(CodeWriter* to) const override;

  void Add(std::shared_ptr<Statement> statement);
  void Add(std::shared_ptr<Expression> expression);
};

struct ExpressionStatement : public Statement {
  std::shared_ptr<Expression> expression;

  explicit ExpressionStatement(std::shared_ptr<Expression> expression);
  virtual ~ExpressionStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct Assignment : public Expression {
  std::shared_ptr<Variable> lvalue;
  std::shared_ptr<Expression> rvalue;
  std::optional<std::string> cast = std::nullopt;

  Assignment(std::shared_ptr<Variable> lvalue, std::shared_ptr<Expression> rvalue);
  Assignment(std::shared_ptr<Variable> lvalue, std::shared_ptr<Expression> rvalue,
             std::string cast);
  virtual ~Assignment() = default;
  void Write(CodeWriter* to) const override;
};

struct MethodCall : public Expression {
  std::variant<std::monostate, std::shared_ptr<Expression>, std::string> receiver;
  std::string name;
  std::vector<std::shared_ptr<Expression>> arguments;
  std::vector<std::string> exceptions;

  explicit MethodCall(const std::string& name);
  MethodCall(const std::string& name, const std::vector<std::shared_ptr<Expression>>& args);
  MethodCall(std::shared_ptr<Expression> obj, const std::string& name);
  MethodCall(const std::string& clazz, const std::string& name);
  MethodCall(std::shared_ptr<Expression> obj, const std::string& name,
             const std::vector<std::shared_ptr<Expression>>& args);
  MethodCall(const std::string&, const std::string& name,
             const std::vector<std::shared_ptr<Expression>>& args);
  virtual ~MethodCall() = default;
  void Write(CodeWriter* to) const override;
};

struct Comparison : public Expression {
  std::shared_ptr<Expression> lvalue;
  std::string op;
  std::shared_ptr<Expression> rvalue;

  Comparison(std::shared_ptr<Expression> lvalue, const std::string& op,
             std::shared_ptr<Expression> rvalue);
  virtual ~Comparison() = default;
  void Write(CodeWriter* to) const override;
};

struct NewExpression : public Expression {
  const std::string instantiableName;
  std::vector<std::shared_ptr<Expression>> arguments;

  explicit NewExpression(const std::string& name);
  NewExpression(const std::string& name, const std::vector<std::shared_ptr<Expression>>& args);
  virtual ~NewExpression() = default;
  void Write(CodeWriter* to) const override;
};

struct NewArrayExpression : public Expression {
  const std::string type;
  std::shared_ptr<Expression> size;

  NewArrayExpression(const std::string& type, std::shared_ptr<Expression> size);
  virtual ~NewArrayExpression() = default;
  void Write(CodeWriter* to) const override;
};

struct Cast : public Expression {
  const std::string type;
  std::shared_ptr<Expression> expression = nullptr;

  Cast() = default;
  Cast(const std::string& type, std::shared_ptr<Expression> expression);
  virtual ~Cast() = default;
  void Write(CodeWriter* to) const override;
};

struct VariableDeclaration : public Statement {
  std::shared_ptr<Variable> lvalue = nullptr;
  std::shared_ptr<Expression> rvalue = nullptr;

  explicit VariableDeclaration(std::shared_ptr<Variable> lvalue);
  VariableDeclaration(std::shared_ptr<Variable> lvalue, std::shared_ptr<Expression> rvalue);
  virtual ~VariableDeclaration() = default;
  void Write(CodeWriter* to) const override;
};

struct IfStatement : public Statement {
  std::shared_ptr<Expression> expression = nullptr;
  std::shared_ptr<StatementBlock> statements = std::make_shared<StatementBlock>();
  std::shared_ptr<IfStatement> elseif = nullptr;

  IfStatement() = default;
  virtual ~IfStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct ReturnStatement : public Statement {
  std::shared_ptr<Expression> expression;

  explicit ReturnStatement(std::shared_ptr<Expression> expression);
  virtual ~ReturnStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct TryStatement : public Statement {
  std::shared_ptr<StatementBlock> statements = std::make_shared<StatementBlock>();

  TryStatement() = default;
  virtual ~TryStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct FinallyStatement : public Statement {
  std::shared_ptr<StatementBlock> statements = std::make_shared<StatementBlock>();

  FinallyStatement() = default;
  virtual ~FinallyStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct Case : public AstNode {
  std::vector<std::string> cases;
  std::shared_ptr<StatementBlock> statements = std::make_shared<StatementBlock>();

  Case() = default;
  explicit Case(const std::string& c);
  virtual ~Case() = default;
  void Write(CodeWriter* to) const override;
};

struct SwitchStatement : public Statement {
  std::shared_ptr<Expression> expression;
  std::vector<std::shared_ptr<Case>> cases;

  explicit SwitchStatement(std::shared_ptr<Expression> expression);
  virtual ~SwitchStatement() = default;
  void Write(CodeWriter* to) const override;
};

struct Method : public ClassElement {
  std::string comment;
  std::vector<std::string> annotations;
  int modifiers = 0;
  std::optional<std::string> returnType = std::nullopt;  // nullopt means constructor
  std::string name;
  std::vector<std::shared_ptr<Variable>> parameters;
  std::vector<std::string> exceptions;
  std::shared_ptr<StatementBlock> statements = nullptr;

  Method() = default;
  virtual ~Method() = default;

  void Write(CodeWriter* to) const override;
};

struct LiteralClassElement : public ClassElement {
  std::string element;

  LiteralClassElement(std::string e) : element(e) {}
  virtual ~LiteralClassElement() = default;

  void Write(CodeWriter* to) const override;
};

struct Class : public ClassElement {
  enum { CLASS, INTERFACE };

  std::string comment;
  std::vector<std::string> annotations;
  int modifiers = 0;
  int what = CLASS;  // CLASS or INTERFACE
  std::string type;
  std::optional<std::string> extends = std::nullopt;
  std::vector<std::string> interfaces;
  std::vector<std::shared_ptr<ClassElement>> elements;

  Class() = default;
  virtual ~Class() = default;

  void Write(CodeWriter* to) const override;
};

class Document : public AstNode {
 public:
  Document(const std::string& comment,
           const std::string& package,
           std::unique_ptr<Class> clazz);
  virtual ~Document() = default;
  void Write(CodeWriter* to) const override;

 private:
  std::string comment_;
  std::string package_;
  std::unique_ptr<Class> clazz_;
};

extern std::shared_ptr<Expression> NULL_VALUE;
extern std::shared_ptr<Expression> THIS_VALUE;
extern std::shared_ptr<Expression> SUPER_VALUE;
extern std::shared_ptr<Expression> TRUE_VALUE;
extern std::shared_ptr<Expression> FALSE_VALUE;
}  // namespace java
}  // namespace aidl
}  // namespace android
