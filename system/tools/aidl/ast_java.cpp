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

#include "ast_java.h"
#include "code_writer.h"

using std::vector;
using std::string;

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;

namespace android {
namespace aidl {
namespace java {

std::string AstNode::ToString() {
  std::string str;
  Write(CodeWriter::ForString(&str).get());
  return str;
}

void WriteModifiers(CodeWriter* to, int mod, int mask) {
  int m = mod & mask;

  if (m & OVERRIDE) {
    to->Write("@Override ");
  }

  if ((m & SCOPE_MASK) == PUBLIC) {
    to->Write("public ");
  } else if ((m & SCOPE_MASK) == PRIVATE) {
    to->Write("private ");
  } else if ((m & SCOPE_MASK) == PROTECTED) {
    to->Write("protected ");
  }

  if (m & STATIC) {
    to->Write("static ");
  }

  if (m & FINAL) {
    to->Write("final ");
  }

  if (m & ABSTRACT) {
    to->Write("abstract ");
  }
}

void WriteArgumentList(CodeWriter* to, const vector<std::shared_ptr<Expression>>& arguments) {
  size_t N = arguments.size();
  for (size_t i = 0; i < N; i++) {
    arguments[i]->Write(to);
    if (i != N - 1) {
      to->Write(", ");
    }
  }
}

Field::Field(int m, std::shared_ptr<Variable> v) : ClassElement(), modifiers(m), variable(v) {}

void Field::Write(CodeWriter* to) const {
  if (this->comment.length() != 0) {
    to->Write("%s\n", this->comment.c_str());
  }
  for (const auto& a : this->annotations) {
    to->Write("%s\n", a.c_str());
  }
  WriteModifiers(to, this->modifiers, SCOPE_MASK | STATIC | FINAL | OVERRIDE);
  this->variable->WriteDeclaration(to);

  if (this->value.length() != 0) {
    to->Write(" = %s", this->value.c_str());
  }
  to->Write(";\n");
}

LiteralExpression::LiteralExpression(const string& v) : value(v) {}

void LiteralExpression::Write(CodeWriter* to) const {
  to->Write("%s", this->value.c_str());
}

StringLiteralExpression::StringLiteralExpression(const string& v) : value(v) {}

void StringLiteralExpression::Write(CodeWriter* to) const {
  to->Write("\"%s\"", this->value.c_str());
}

Variable::Variable(const string& t, const string& n) : type(t), name(n) {}

void Variable::WriteDeclaration(CodeWriter* to) const {
  to->Write("%s %s", this->type.c_str(), this->name.c_str());
}

void Variable::Write(CodeWriter* to) const { to->Write("%s", name.c_str()); }

FieldVariable::FieldVariable(std::shared_ptr<Expression> o, const string& n)
    : receiver(o), name(n) {}

FieldVariable::FieldVariable(const string& c, const string& n) : receiver(c), name(n) {}

void FieldVariable::Write(CodeWriter* to) const {
  visit(
      overloaded{[&](std::shared_ptr<Expression> e) { e->Write(to); },
                 [&](const std::string& s) { to->Write("%s", s.c_str()); }, [](std::monostate) {}},
      this->receiver);
  to->Write(".%s", name.c_str());
}

LiteralStatement::LiteralStatement(const std::string& value) : value_(value) {}

void LiteralStatement::Write(CodeWriter* to) const {
  to->Write("%s", value_.c_str());
}

void StatementBlock::Write(CodeWriter* to) const {
  to->Write("{\n");
  to->Indent();
  int N = this->statements.size();
  for (int i = 0; i < N; i++) {
    this->statements[i]->Write(to);
  }
  to->Dedent();
  to->Write("}\n");
}

void StatementBlock::Add(std::shared_ptr<Statement> statement) {
  this->statements.push_back(statement);
}

void StatementBlock::Add(std::shared_ptr<Expression> expression) {
  this->statements.push_back(std::make_shared<ExpressionStatement>(expression));
}

ExpressionStatement::ExpressionStatement(std::shared_ptr<Expression> e) : expression(e) {}

void ExpressionStatement::Write(CodeWriter* to) const {
  this->expression->Write(to);
  to->Write(";\n");
}

Assignment::Assignment(std::shared_ptr<Variable> l, std::shared_ptr<Expression> r)
    : lvalue(l), rvalue(r) {}

Assignment::Assignment(std::shared_ptr<Variable> l, std::shared_ptr<Expression> r, string c)
    : lvalue(l), rvalue(r), cast(c) {}

void Assignment::Write(CodeWriter* to) const {
  this->lvalue->Write(to);
  to->Write(" = ");
  if (this->cast) {
    to->Write("(%s)", this->cast->c_str());
  }
  this->rvalue->Write(to);
}

MethodCall::MethodCall(const string& n) : name(n) {}

MethodCall::MethodCall(const string& n, const std::vector<std::shared_ptr<Expression>>& args)
    : name(n), arguments(args) {}

MethodCall::MethodCall(std::shared_ptr<Expression> o, const string& n) : receiver(o), name(n) {}

MethodCall::MethodCall(const std::string& t, const string& n) : receiver(t), name(n) {}

MethodCall::MethodCall(std::shared_ptr<Expression> o, const string& n,
                       const std::vector<std::shared_ptr<Expression>>& args)
    : receiver(o), name(n), arguments(args) {}

MethodCall::MethodCall(const std::string& t, const string& n,
                       const std::vector<std::shared_ptr<Expression>>& args)
    : receiver(t), name(n), arguments(args) {}

void MethodCall::Write(CodeWriter* to) const {
  visit(
      overloaded{[&](std::shared_ptr<Expression> e) {
                   e->Write(to);
                   to->Write(".");
                 },
                 [&](const std::string& s) { to->Write("%s.", s.c_str()); }, [](std::monostate) {}},
      this->receiver);
  to->Write("%s(", this->name.c_str());
  WriteArgumentList(to, this->arguments);
  to->Write(")");
}

Comparison::Comparison(std::shared_ptr<Expression> l, const string& o,
                       std::shared_ptr<Expression> r)
    : lvalue(l), op(o), rvalue(r) {}

void Comparison::Write(CodeWriter* to) const {
  to->Write("(");
  this->lvalue->Write(to);
  to->Write("%s", this->op.c_str());
  this->rvalue->Write(to);
  to->Write(")");
}

NewExpression::NewExpression(const std::string& n) : instantiableName(n) {}

NewExpression::NewExpression(const std::string& n,
                             const std::vector<std::shared_ptr<Expression>>& args)
    : instantiableName(n), arguments(args) {}

void NewExpression::Write(CodeWriter* to) const {
  to->Write("new %s(", this->instantiableName.c_str());
  WriteArgumentList(to, this->arguments);
  to->Write(")");
}

NewArrayExpression::NewArrayExpression(const std::string& t, std::shared_ptr<Expression> s)
    : type(t), size(s) {}

void NewArrayExpression::Write(CodeWriter* to) const {
  to->Write("new %s[", this->type.c_str());
  size->Write(to);
  to->Write("]");
}

Cast::Cast(const std::string& t, std::shared_ptr<Expression> e) : type(t), expression(e) {}

void Cast::Write(CodeWriter* to) const {
  to->Write("((%s)", this->type.c_str());
  expression->Write(to);
  to->Write(")");
}

VariableDeclaration::VariableDeclaration(std::shared_ptr<Variable> l, std::shared_ptr<Expression> r)
    : lvalue(l), rvalue(r) {}

VariableDeclaration::VariableDeclaration(std::shared_ptr<Variable> l) : lvalue(l) {}

void VariableDeclaration::Write(CodeWriter* to) const {
  this->lvalue->WriteDeclaration(to);
  if (this->rvalue != nullptr) {
    to->Write(" = ");
    this->rvalue->Write(to);
  }
  to->Write(";\n");
}

void IfStatement::Write(CodeWriter* to) const {
  if (this->expression != nullptr) {
    to->Write("if (");
    this->expression->Write(to);
    to->Write(") ");
  }
  this->statements->Write(to);
  if (this->elseif != nullptr) {
    to->Write("else ");
    this->elseif->Write(to);
  }
}

ReturnStatement::ReturnStatement(std::shared_ptr<Expression> e) : expression(e) {}

void ReturnStatement::Write(CodeWriter* to) const {
  to->Write("return ");
  this->expression->Write(to);
  to->Write(";\n");
}

void TryStatement::Write(CodeWriter* to) const {
  to->Write("try ");
  this->statements->Write(to);
}

void FinallyStatement::Write(CodeWriter* to) const {
  to->Write("finally ");
  this->statements->Write(to);
}

Case::Case(const string& c) { cases.push_back(c); }

void Case::Write(CodeWriter* to) const {
  int N = this->cases.size();
  if (N > 0) {
    for (int i = 0; i < N; i++) {
      string s = this->cases[i];
      if (s.length() != 0) {
        to->Write("case %s:\n", s.c_str());
      } else {
        to->Write("default:\n");
      }
    }
  } else {
    to->Write("default:\n");
  }
  statements->Write(to);
}

SwitchStatement::SwitchStatement(std::shared_ptr<Expression> e) : expression(e) {}

void SwitchStatement::Write(CodeWriter* to) const {
  to->Write("switch (");
  this->expression->Write(to);
  to->Write(")\n{\n");
  to->Indent();
  int N = this->cases.size();
  for (int i = 0; i < N; i++) {
    this->cases[i]->Write(to);
  }
  to->Dedent();
  to->Write("}\n");
}

void Method::Write(CodeWriter* to) const {
  size_t N, i;

  if (this->comment.length() != 0) {
    to->Write("%s\n", this->comment.c_str());
  }

  for (const auto& a : this->annotations) {
    to->Write("%s\n", a.c_str());
  }

  WriteModifiers(to, this->modifiers,
                 SCOPE_MASK | STATIC | ABSTRACT | FINAL | OVERRIDE);

  if (this->returnType) {
    to->Write("%s ", this->returnType->c_str());
  }

  to->Write("%s(", this->name.c_str());

  N = this->parameters.size();
  for (i = 0; i < N; i++) {
    this->parameters[i]->WriteDeclaration(to);
    if (i != N - 1) {
      to->Write(", ");
    }
  }

  to->Write(")");

  N = this->exceptions.size();
  for (i = 0; i < N; i++) {
    if (i == 0) {
      to->Write(" throws ");
    } else {
      to->Write(", ");
    }
    to->Write("%s", this->exceptions[i].c_str());
  }

  if (this->statements == nullptr) {
    to->Write(";\n");
  } else {
    to->Write("\n");
    this->statements->Write(to);
  }
}

void LiteralClassElement::Write(CodeWriter* to) const {
  to->Write("%s", element.c_str());
}

void Class::Write(CodeWriter* to) const {
  size_t N, i;

  if (this->comment.length() != 0) {
    to->Write("%s\n", this->comment.c_str());
  }
  for (const auto& a : this->annotations) {
    to->Write("%s\n", a.c_str());
  }

  WriteModifiers(to, this->modifiers, ALL_MODIFIERS);

  if (this->what == Class::CLASS) {
    to->Write("class ");
  } else {
    to->Write("interface ");
  }

  string name = this->type;
  size_t pos = name.rfind('.');
  if (pos != string::npos) {
    name = name.c_str() + pos + 1;
  }

  to->Write("%s", name.c_str());

  if (this->extends) {
    to->Write(" extends %s", this->extends->c_str());
  }

  N = this->interfaces.size();
  if (N != 0) {
    if (this->what == Class::CLASS) {
      to->Write(" implements");
    } else {
      to->Write(" extends");
    }
    for (i = 0; i < N; i++) {
      to->Write(" %s", this->interfaces[i].c_str());
    }
  }

  to->Write("\n");
  to->Write("{\n");
  to->Indent();

  N = this->elements.size();
  for (i = 0; i < N; i++) {
    this->elements[i]->Write(to);
  }

  to->Dedent();
  to->Write("}\n");
}

Document::Document(const std::string& comment,
                   const std::string& package,
                   std::unique_ptr<Class> clazz)
    : comment_(comment),
      package_(package),
      clazz_(std::move(clazz)) {
}

void Document::Write(CodeWriter* to) const {
  if (!comment_.empty()) {
    to->Write("%s\n", comment_.c_str());
  }
  to->Write(
      "/*\n"
      " * This file is auto-generated.  DO NOT MODIFY.\n"
      " */\n");
  if (!package_.empty()) {
    to->Write("package %s;\n", package_.c_str());
  }

  if (clazz_) {
    clazz_->Write(to);
  }
}

std::shared_ptr<Expression> NULL_VALUE = std::make_shared<LiteralExpression>("null");
std::shared_ptr<Expression> THIS_VALUE = std::make_shared<LiteralExpression>("this");
std::shared_ptr<Expression> SUPER_VALUE = std::make_shared<LiteralExpression>("super");
std::shared_ptr<Expression> TRUE_VALUE = std::make_shared<LiteralExpression>("true");
std::shared_ptr<Expression> FALSE_VALUE = std::make_shared<LiteralExpression>("false");
}  // namespace java
}  // namespace aidl
}  // namespace android
