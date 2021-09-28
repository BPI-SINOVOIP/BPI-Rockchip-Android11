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

%{
#include "aidl_language.h"
#include "aidl_language_y-module.h"
#include "logging.h"
#include <android-base/parseint.h>
#include <set>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(yy::parser::semantic_type *, yy::parser::location_type *, void *);

AidlLocation loc(const yy::parser::location_type& begin, const yy::parser::location_type& end) {
  CHECK(begin.begin.filename == begin.end.filename);
  CHECK(begin.end.filename == end.begin.filename);
  CHECK(end.begin.filename == end.end.filename);
  AidlLocation::Point begin_point {
    .line = begin.begin.line,
    .column = begin.begin.column,
  };
  AidlLocation::Point end_point {
    .line = end.end.line,
    .column = end.end.column,
  };
  return AidlLocation(*begin.begin.filename, begin_point, end_point);
}

AidlLocation loc(const yy::parser::location_type& l) {
  return loc(l, l);
}

#define lex_scanner ps->Scanner()

%}

%initial-action {
    @$.begin.filename = @$.end.filename =
        const_cast<std::string *>(&ps->FileName());
}

%parse-param { Parser* ps }
%lex-param { void *lex_scanner }

%glr-parser
%skeleton "glr.cc"

%expect-rr 0

%define parse.error verbose
%locations

%union {
    AidlToken* token;
    char character;
    std::string *str;
    AidlAnnotation* annotation;
    AidlAnnotationParameter* param;
    std::map<std::string, std::shared_ptr<AidlConstantValue>>* param_list;
    std::vector<AidlAnnotation>* annotation_list;
    AidlTypeSpecifier* type;
    AidlArgument* arg;
    AidlArgument::Direction direction;
    AidlConstantValue* const_expr;
    AidlEnumerator* enumerator;
    std::vector<std::unique_ptr<AidlEnumerator>>* enumerators;
    AidlEnumDeclaration* enum_decl;
    std::vector<std::unique_ptr<AidlConstantValue>>* constant_value_list;
    std::vector<std::unique_ptr<AidlArgument>>* arg_list;
    AidlVariableDeclaration* variable;
    std::vector<std::unique_ptr<AidlVariableDeclaration>>* variable_list;
    AidlMethod* method;
    AidlMember* constant;
    std::vector<std::unique_ptr<AidlMember>>* interface_members;
    AidlQualifiedName* qname;
    AidlInterface* interface;
    AidlParcelable* parcelable;
    AidlDefinedType* declaration;
    std::vector<std::unique_ptr<AidlTypeSpecifier>>* type_args;
    std::vector<std::string>* type_params;
}

%destructor { } <character>
%destructor { } <direction>
%destructor { delete ($$); } <*>

%token<token> ANNOTATION "annotation"
%token<token> C_STR "string literal"
%token<token> IDENTIFIER "identifier"
%token<token> INTERFACE "interface"
%token<token> PARCELABLE "parcelable"
%token<token> ONEWAY "oneway"
%token<token> ENUM "enum"
%token<token> CONST "const"

%token<character> CHARVALUE "char literal"
%token<token> FLOATVALUE "float literal"
%token<token> HEXVALUE "hex literal"
%token<token> INTVALUE "int literal"

%token '(' ')' ',' '=' '[' ']' '.' '{' '}' ';'
%token UNKNOWN "unrecognized character"
%token CPP_HEADER "cpp_header"
%token IMPORT "import"
%token IN "in"
%token INOUT "inout"
%token OUT "out"
%token PACKAGE "package"
%token TRUE_LITERAL "true"
%token FALSE_LITERAL "false"

/* Operator precedence and associativity, as per
 * http://en.cppreference.com/w/cpp/language/operator_precedence */
/* Precedence level 13 - 14, LTR, logical operators*/
%left LOGICAL_OR
%left LOGICAL_AND
/* Precedence level 10 - 12, LTR, bitwise operators*/
%left '|'
%left '^'
%left '&'
/* Precedence level 9, LTR */
%left EQUALITY NEQ
/* Precedence level 8, LTR */
%left '<' '>' LEQ GEQ
/* Precedence level 7, LTR */
%left LSHIFT RSHIFT
/* Precedence level 6, LTR */
%left '+' '-'
/* Precedence level 5, LTR */
%left '*' '/' '%'
/* Precedence level 3, RTL; but we have to use %left here */
%right UNARY_PLUS UNARY_MINUS  '!' '~'

%type<declaration> decl
%type<variable_list> variable_decls
%type<variable> variable_decl
%type<interface_members> interface_members
%type<declaration> unannotated_decl
%type<interface> interface_decl
%type<parcelable> parcelable_decl
%type<method> method_decl
%type<constant> constant_decl
%type<enumerator> enumerator
%type<enumerators> enumerators enum_decl_body
%type<enum_decl> enum_decl
%type<param> parameter
%type<param_list> parameter_list
%type<param_list> parameter_non_empty_list
%type<annotation> annotation
%type<annotation_list>annotation_list
%type<type> type
%type<type> unannotated_type
%type<arg_list> arg_list arg_non_empty_list
%type<arg> arg
%type<direction> direction
%type<type_args> type_args
%type<type_params> type_params
%type<qname> qualified_name
%type<const_expr> const_expr
%type<constant_value_list> constant_value_list
%type<constant_value_list> constant_value_non_empty_list

%type<token> identifier error
%%
document
 : package imports decls {};

/* A couple of tokens that are keywords elsewhere are identifiers when
 * occurring in the identifier position. Therefore identifier is a
 * non-terminal, which is either an IDENTIFIER token, or one of the
 * aforementioned keyword tokens.
 */
identifier
 : IDENTIFIER
  { $$ = $1; }
 | CPP_HEADER
  { $$ = new AidlToken("cpp_header", ""); }
 ;

package
 : {}
 | PACKAGE qualified_name ';'
  { ps->SetPackage(unique_ptr<AidlQualifiedName>($2)); };

imports
 : {}
 | import imports {};

import
 : IMPORT qualified_name ';'
  { ps->AddImport(std::make_unique<AidlImport>(loc(@2), $2->GetDotName()));
    delete $2;
  };

qualified_name
 : identifier {
    $$ = new AidlQualifiedName(loc(@1), $1->GetText(), $1->GetComments());
    delete $1;
  }
 | qualified_name '.' identifier
  { $$ = $1;
    $$->AddTerm($3->GetText());
    delete $3;
  };

decls
 : decl {
    if ($1 != nullptr) {
      ps->AddDefinedType(unique_ptr<AidlDefinedType>($1));
    }
  }
 | decls decl {
    if ($2 != nullptr) {
      ps->AddDefinedType(unique_ptr<AidlDefinedType>($2));
    }
  };

decl
 : annotation_list unannotated_decl
   {
    $$ = $2;

    if ($1->size() > 0 && $$ != nullptr) {
      // copy comments from annotation to decl
      $$->SetComments($1->begin()->GetComments());
      $$->Annotate(std::move(*$1));
    }

    delete $1;
   }
 ;

unannotated_decl
 : parcelable_decl
  { $$ = $1; }
 | interface_decl
  { $$ = $1; }
 | enum_decl
  { $$ = $1; }
 ;

type_params
 : identifier {
    $$ = new std::vector<std::string>();
    $$->emplace_back($1->GetText());
    delete $1;
  }
 | type_params ',' identifier {
    $1->emplace_back($3->GetText());
    $$ = $1;
    delete $3;
  };


parcelable_decl
 : PARCELABLE qualified_name ';' {
    $$ = new AidlParcelable(loc(@2), $2, ps->Package(), $1->GetComments());
    delete $1;
  }
 | PARCELABLE qualified_name '<' type_params '>' ';' {
    $$ = new AidlParcelable(loc(@2), $2, ps->Package(), $1->GetComments(), "", $4);
    delete $1;
 }
 | PARCELABLE qualified_name CPP_HEADER C_STR ';' {
    $$ = new AidlParcelable(loc(@2), $2, ps->Package(), $1->GetComments(), $4->GetText());
    delete $1;
    delete $4;
  }
 | PARCELABLE identifier '{' variable_decls '}' {
    AidlQualifiedName* name = new AidlQualifiedName(loc(@2), $2->GetText(), $2->GetComments());
    $$ = new AidlStructuredParcelable(loc(@2), name, ps->Package(), $1->GetComments(), $4);
    delete $1;
    delete $2;
    delete $4;
 }
 | PARCELABLE error ';' {
    ps->AddError();
    $$ = nullptr;
    delete $1;
  };

variable_decls
 : /* empty */ {
    $$ = new std::vector<std::unique_ptr<AidlVariableDeclaration>>;
 }
 | variable_decls variable_decl {
    $$ = $1;
    if ($2 != nullptr) {
      $$->push_back(std::unique_ptr<AidlVariableDeclaration>($2));
    }
 };

variable_decl
 : type identifier ';' {
   $$ = new AidlVariableDeclaration(loc(@2), $1, $2->GetText());
   delete $2;
 }
 | type identifier '=' const_expr ';' {
   // TODO(b/123321528): Support enum type default assignments (TestEnum foo = TestEnum.FOO).
   $$ = new AidlVariableDeclaration(loc(@2), $1, $2->GetText(),  $4);
   delete $2;
 }
 | error ';' {
   ps->AddError();
   $$ = nullptr;
 }

interface_decl
 : INTERFACE identifier '{' interface_members '}' {
    $$ = new AidlInterface(loc(@1), $2->GetText(), $1->GetComments(), false, $4, ps->Package());
    delete $1;
    delete $2;
  }
 | ONEWAY INTERFACE identifier '{' interface_members '}' {
    $$ = new AidlInterface(loc(@2), $3->GetText(),  $1->GetComments(), true, $5, ps->Package());
    delete $1;
    delete $2;
    delete $3;
  }
 | INTERFACE error '{' interface_members '}' {
    ps->AddError();
    $$ = nullptr;
    delete $1;
    delete $4;
  };

interface_members
 :
  { $$ = new std::vector<std::unique_ptr<AidlMember>>(); }
 | interface_members method_decl
  { $1->push_back(std::unique_ptr<AidlMember>($2)); $$ = $1; }
 | interface_members constant_decl
  { $1->push_back(std::unique_ptr<AidlMember>($2)); $$ = $1; }
 | interface_members error ';' {
    ps->AddError();
    $$ = $1;
  };

const_expr
 : TRUE_LITERAL { $$ = AidlConstantValue::Boolean(loc(@1), true); }
 | FALSE_LITERAL { $$ = AidlConstantValue::Boolean(loc(@1), false); }
 | CHARVALUE { $$ = AidlConstantValue::Character(loc(@1), $1); }
 | INTVALUE {
    $$ = AidlConstantValue::Integral(loc(@1), $1->GetText());
    if ($$ == nullptr) {
      std::cerr << "ERROR: Could not parse integer: "
                << $1->GetText() << " at " << @1 << ".\n";
      ps->AddError();
      $$ = AidlConstantValue::Integral(loc(@1), "0");
    }
    delete $1;
  }
 | FLOATVALUE {
    $$ = AidlConstantValue::Floating(loc(@1), $1->GetText());
    delete $1;
  }
 | HEXVALUE {
    $$ = AidlConstantValue::Integral(loc(@1), $1->GetText());
    if ($$ == nullptr) {
      std::cerr << "ERROR: Could not parse hexvalue: "
                << $1->GetText() << " at " << @1 << ".\n";
      ps->AddError();
      $$ = AidlConstantValue::Integral(loc(@1), "0");
    }
    delete $1;
  }
 | C_STR {
    $$ = AidlConstantValue::String(loc(@1), $1->GetText());
    delete $1;
  }
 | '{' constant_value_list '}' {
    $$ = AidlConstantValue::Array(loc(@1), std::unique_ptr<vector<unique_ptr<AidlConstantValue>>>($2));
  }
 | const_expr LOGICAL_OR const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "||", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr LOGICAL_AND const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "&&", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '|' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "|" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '^' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "^" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '&' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "&" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr EQUALITY const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "==", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr NEQ const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "!=", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '<' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "<" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '>' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), ">" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr LEQ const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "<=", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr GEQ const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), ">=", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr LSHIFT const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "<<", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr RSHIFT const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), ">>", std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '+' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "+" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '-' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "-" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '*' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "*" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '/' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "/" , std::unique_ptr<AidlConstantValue>($3));
  }
 | const_expr '%' const_expr {
    $$ = new AidlBinaryConstExpression(loc(@1), std::unique_ptr<AidlConstantValue>($1), "%" , std::unique_ptr<AidlConstantValue>($3));
  }
 | '+' const_expr %prec UNARY_PLUS  {
    $$ = new AidlUnaryConstExpression(loc(@1), "+", std::unique_ptr<AidlConstantValue>($2));
  }
 | '-' const_expr %prec UNARY_MINUS {
    $$ = new AidlUnaryConstExpression(loc(@1), "-", std::unique_ptr<AidlConstantValue>($2));
  }
 | '!' const_expr {
    $$ = new AidlUnaryConstExpression(loc(@1), "!", std::unique_ptr<AidlConstantValue>($2));
  }
 | '~' const_expr {
    $$ = new AidlUnaryConstExpression(loc(@1), "~", std::unique_ptr<AidlConstantValue>($2));
  }
 | '(' const_expr ')'
  {
    $$ = $2;
  }
 | '(' error ')'
   {
     std::cerr << "ERROR: invalid const expression within parenthesis at " << @1 << ".\n";
     ps->AddError();
     // to avoid segfaults
     $$ = AidlConstantValue::Integral(loc(@1), "0");
   }
 ;

constant_value_list
 : /* empty */ {
    $$ = new std::vector<std::unique_ptr<AidlConstantValue>>;
 }
 | constant_value_non_empty_list {
    $$ = $1;
 }
 ;

constant_value_non_empty_list
 : const_expr {
    $$ = new std::vector<std::unique_ptr<AidlConstantValue>>;
    $$->push_back(std::unique_ptr<AidlConstantValue>($1));
 }
 | constant_value_non_empty_list ',' const_expr {
    $$ = $1;
    $$->push_back(std::unique_ptr<AidlConstantValue>($3));
 }
 ;

constant_decl
 : CONST type identifier '=' const_expr ';' {
    $2->SetComments($1->GetComments());
    $$ = new AidlConstantDeclaration(loc(@3), $2, $3->GetText(), $5);
    delete $1;
    delete $3;
   }
 ;

enumerator
 : identifier '=' const_expr {
    $$ = new AidlEnumerator(loc(@1), $1->GetText(), $3, $1->GetComments());
    delete $1;
   }
 | identifier {
    $$ = new AidlEnumerator(loc(@1), $1->GetText(), nullptr, $1->GetComments());
    delete $1;
   }
 ;

enumerators
 : enumerator {
    $$ = new std::vector<std::unique_ptr<AidlEnumerator>>();
    $$->push_back(std::unique_ptr<AidlEnumerator>($1));
   }
 | enumerators ',' enumerator {
    $1->push_back(std::unique_ptr<AidlEnumerator>($3));
    $$ = $1;
   }
 ;

enum_decl_body
 : '{' enumerators '}' { $$ = $2; }
 | '{' enumerators ',' '}' { $$ = $2; }
 ;

enum_decl
 : ENUM identifier enum_decl_body {
    $$ = new AidlEnumDeclaration(loc(@2), $2->GetText(), $3, ps->Package(), $1->GetComments());
    delete $1;
    delete $2;
    delete $3;
   }
 ;

method_decl
 : type identifier '(' arg_list ')' ';' {
    $$ = new AidlMethod(loc(@2), false, $1, $2->GetText(), $4, $1->GetComments());
    delete $2;
  }
 | annotation_list ONEWAY type identifier '(' arg_list ')' ';' {
    const std::string& comments = ($1->size() > 0) ? $1->begin()->GetComments() : $2->GetComments();
    $$ = new AidlMethod(loc(@4), true, $3, $4->GetText(), $6, comments);
    $3->Annotate(std::move(*$1));
    delete $1;
    delete $2;
    delete $4;
  }
 | type identifier '(' arg_list ')' '=' INTVALUE ';' {
    int32_t serial = 0;
    if (!android::base::ParseInt($7->GetText(), &serial)) {
        AIDL_ERROR(loc(@7)) << "Could not parse int value: " << $7->GetText();
        ps->AddError();
    }
    $$ = new AidlMethod(loc(@2), false, $1, $2->GetText(), $4, $1->GetComments(), serial);
    delete $2;
    delete $7;
  }
 | annotation_list ONEWAY type identifier '(' arg_list ')' '=' INTVALUE ';' {
    const std::string& comments = ($1->size() > 0) ? $1->begin()->GetComments() : $2->GetComments();
    int32_t serial = 0;
    if (!android::base::ParseInt($9->GetText(), &serial)) {
        AIDL_ERROR(loc(@9)) << "Could not parse int value: " << $9->GetText();
        ps->AddError();
    }
    $$ = new AidlMethod(loc(@4), true, $3, $4->GetText(), $6, comments, serial);
    $3->Annotate(std::move(*$1));
    delete $1;
    delete $2;
    delete $4;
    delete $9;
  };

arg_non_empty_list
 : arg {
    $$ = new std::vector<std::unique_ptr<AidlArgument>>();
    $$->push_back(std::unique_ptr<AidlArgument>($1));
  }
 | arg_non_empty_list ',' arg {
    $$ = $1;
    $$->push_back(std::unique_ptr<AidlArgument>($3));
  };

arg_list
 : /*empty*/
   { $$ = new std::vector<std::unique_ptr<AidlArgument>>(); }
 | arg_non_empty_list { $$ = $1; }
 ;

arg
 : direction type identifier {
    $$ = new AidlArgument(loc(@3), $1, $2, $3->GetText());
    delete $3;
  }
 | type identifier {
    $$ = new AidlArgument(loc(@2), $1, $2->GetText());
    delete $2;
  }
 ;

unannotated_type
 : qualified_name {
    $$ = new AidlTypeSpecifier(loc(@1), $1->GetDotName(), false, nullptr, $1->GetComments());
    ps->DeferResolution($$);
    delete $1;
  }
 | qualified_name '[' ']' {
    $$ = new AidlTypeSpecifier(loc(@1), $1->GetDotName(), true, nullptr, $1->GetComments());
    ps->DeferResolution($$);
    delete $1;
  }
 | qualified_name '<' type_args '>' {
    $$ = new AidlTypeSpecifier(loc(@1), $1->GetDotName(), false, $3, $1->GetComments());
    ps->DeferResolution($$);
    delete $1;
  };

type
 : annotation_list unannotated_type {
    $$ = $2;
    if ($1->size() > 0) {
      // copy comments from annotation to type
      $2->SetComments($1->begin()->GetComments());
    }
    $2->Annotate(std::move(*$1));
    delete $1;
  };

type_args
 : unannotated_type {
    $$ = new std::vector<std::unique_ptr<AidlTypeSpecifier>>();
    $$->emplace_back($1);
  }
 | type_args ',' unannotated_type {
    $1->emplace_back($3);
    $$ = $1;
  };

annotation_list
 :
  { $$ = new std::vector<AidlAnnotation>(); }
 | annotation_list annotation
  {
    if ($2 != nullptr) {
      $1->emplace_back(std::move(*$2));
      delete $2;
    }
    $$ = $1;
  };

parameter
  : identifier '=' const_expr {
    $$ = new AidlAnnotationParameter{$1->GetText(), std::unique_ptr<AidlConstantValue>($3)};
    delete $1;
  };

parameter_list
  : /*empty*/{
    $$ = new std::map<std::string, std::shared_ptr<AidlConstantValue>>();
  }
  | parameter_non_empty_list  {
    $$ = $1;
  };

parameter_non_empty_list
  : parameter {
    $$ = new std::map<std::string, std::shared_ptr<AidlConstantValue>>();
    $$->emplace(std::move($1->name), $1->value.release());
    delete $1;
  }
  | parameter_non_empty_list ',' parameter {
    $$ = $1;
    if ($$->find($3->name) != $$->end()) {
      AIDL_ERROR(loc(@3)) << "Trying to redefine parameter " << $3->name << ".";
      ps->AddError();
    }
    $$->emplace(std::move($3->name), std::move($3->value));
    delete $3;
  };

annotation
 : ANNOTATION
  {
    $$ = AidlAnnotation::Parse(loc(@1), $1->GetText(), nullptr);
    if ($$) {
      $$->SetComments($1->GetComments());
    } else {
      ps->AddError();
    }
    delete $1;
  };
 | ANNOTATION '(' parameter_list ')' {
    $$ = AidlAnnotation::Parse(loc(@1, @4), $1->GetText(), $3);
    if ($$) {
      $$->SetComments($1->GetComments());
    } else {
      ps->AddError();
    }
    delete $1;
    delete $3;
 }

direction
 : IN
  { $$ = AidlArgument::IN_DIR; }
 | OUT
  { $$ = AidlArgument::OUT_DIR; }
 | INOUT
  { $$ = AidlArgument::INOUT_DIR; };

%%

#include <ctype.h>
#include <stdio.h>

void yy::parser::error(const yy::parser::location_type& l, const std::string& errstr) {
  AIDL_ERROR(loc(l)) << errstr;
  // parser will return error value
}
