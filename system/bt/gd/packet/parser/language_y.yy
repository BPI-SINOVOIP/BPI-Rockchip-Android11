%{
  #include <iostream>
  #include <vector>
  #include <list>
  #include <map>

  #include "declarations.h"
  #include "logging.h"
  #include "language_y.h"
  #include "field_list.h"
  #include "fields/all_fields.h"

  extern int yylex(yy::parser::semantic_type*, yy::parser::location_type*, void *);

  ParseLocation toParseLocation(yy::parser::location_type loc) {
    return ParseLocation(loc.begin.line);
  }
  #define LOC toParseLocation(yylloc)
%}

%parse-param { void* scanner }
%parse-param { Declarations* decls }
%lex-param { void* scanner }

%glr-parser
%skeleton "glr.cc"

%expect-rr 0

%debug
%define parse.error verbose
%locations
%verbose

%union {
  int integer;
  std::string* string;

  EnumDef* enum_definition;
  std::map<int, std::string>* enumeration_values;
  std::pair<int, std::string>* enumeration_value;

  PacketDef* packet_definition_value;
  FieldList* packet_field_definitions;
  PacketField* packet_field_type;

  StructDef* struct_definition_value;

  std::map<std::string, std::variant<int64_t, std::string>>* constraint_list_t;
  std::pair<std::string, std::variant<int64_t, std::string>>* constraint_t;
}

%token <integer> INTEGER
%token <integer> IS_LITTLE_ENDIAN
%token <string> IDENTIFIER
%token <string> SIZE_MODIFIER
%token <string> STRING

%token ENUM "enum"
%token PACKET "packet"
%token PAYLOAD "payload"
%token BODY "body"
%token STRUCT "struct"
%token SIZE "size"
%token COUNT "count"
%token FIXED "fixed"
%token RESERVED "reserved"
%token GROUP "group"
%token CUSTOM_FIELD "custom_field"
%token CHECKSUM "checksum"
%token CHECKSUM_START "checksum_start"
%token PADDING "padding"

%type<enum_definition> enum_definition
%type<enumeration_values> enumeration_list
%type<enumeration_value> enumeration

%type<packet_definition_value> packet_definition;
%type<packet_field_definitions> field_definition_list;
%type<packet_field_type> field_definition;
%type<packet_field_type> group_field_definition;
%type<packet_field_type> type_def_field_definition;
%type<packet_field_type> scalar_field_definition;
%type<packet_field_type> checksum_start_field_definition;
%type<packet_field_type> padding_field_definition;
%type<packet_field_type> size_field_definition;
%type<packet_field_type> payload_field_definition;
%type<packet_field_type> body_field_definition;
%type<packet_field_type> fixed_field_definition;
%type<packet_field_type> reserved_field_definition;
%type<packet_field_type> array_field_definition;

%type<struct_definition_value> struct_definition;

%type<constraint_list_t> constraint_list;
%type<constraint_t> constraint;
%destructor { std::cout << "DESTROYING STRING " << *$$ << "\n"; delete $$; } IDENTIFIER STRING SIZE_MODIFIER

%%

file
  : IS_LITTLE_ENDIAN declarations
  {
    decls->is_little_endian = ($1 == 1);
    if (decls->is_little_endian) {
      DEBUG() << "LITTLE ENDIAN ";
    } else {
      DEBUG() << "BIG ENDIAN ";
    }
  }

declarations
  : /* empty */
  | declarations declaration

declaration
  : enum_definition
    {
      DEBUG() << "FOUND ENUM\n\n";
      decls->AddTypeDef($1->name_, $1);
    }
  | packet_definition
    {
      DEBUG() << "FOUND PACKET\n\n";
      decls->AddPacketDef($1->name_, std::move(*$1));
      delete $1;
    }
  | struct_definition
    {
      DEBUG() << "FOUND STRUCT\n\n";
      decls->AddTypeDef($1->name_, $1);
    }
  | group_definition
    {
      // All actions are handled in group_definition
    }
  | checksum_definition
    {
      // All actions are handled in checksum_definition
    }
  | custom_field_definition
    {
      // All actions are handled in custom_field_definition
    }

enum_definition
  : ENUM IDENTIFIER ':' INTEGER '{' enumeration_list ',' '}'
    {
      DEBUG() << "Enum Declared: name=" << *$2
                << " size=" << $4 << "\n";

      $$ = new EnumDef(std::move(*$2), $4);
      for (const auto& e : *$6) {
        $$->AddEntry(e.second, e.first);
      }
      delete $2;
      delete $6;
    }

enumeration_list
  : enumeration
    {
      DEBUG() << "Enumerator with comma\n";
      $$ = new std::map<int, std::string>();
      $$->insert(std::move(*$1));
      delete $1;
    }
  | enumeration_list ',' enumeration
    {
      DEBUG() << "Enumerator with list\n";
      $$ = $1;
      $$->insert(std::move(*$3));
      delete $3;
    }

enumeration
  : IDENTIFIER '=' INTEGER
    {
      DEBUG() << "Enumerator: name=" << *$1
                << " value=" << $3 << "\n";
      $$ = new std::pair($3, std::move(*$1));
      delete $1;
    }

group_definition
  : GROUP IDENTIFIER '{' field_definition_list '}'
    {
      decls->AddGroupDef(*$2, $4);
      delete $2;
    }

checksum_definition
  : CHECKSUM IDENTIFIER ':' INTEGER STRING
    {
      DEBUG() << "Checksum field defined\n";
      decls->AddTypeDef(*$2, new ChecksumDef(*$2, *$5, $4));
      delete $2;
      delete $5;
    }

custom_field_definition
  : CUSTOM_FIELD IDENTIFIER ':' INTEGER STRING
    {
      decls->AddTypeDef(*$2, new CustomFieldDef(*$2, *$5, $4));
      delete $2;
      delete $5;
    }
  | CUSTOM_FIELD IDENTIFIER STRING
    {
      decls->AddTypeDef(*$2, new CustomFieldDef(*$2, *$3));
      delete $2;
      delete $3;
    }

struct_definition
  : STRUCT IDENTIFIER '{' field_definition_list '}'
    {
      auto&& struct_name = *$2;
      auto&& field_definition_list = *$4;

      DEBUG() << "Struct " << struct_name << " with no parent";
      DEBUG() << "STRUCT FIELD LIST SIZE: " << field_definition_list.size();
      auto struct_definition = new StructDef(std::move(struct_name), std::move(field_definition_list));
      struct_definition->AssignSizeFields();

      $$ = struct_definition;
      delete $2;
      delete $4;
    }
  | STRUCT IDENTIFIER ':' IDENTIFIER '{' field_definition_list '}'
    {
      auto&& struct_name = *$2;
      auto&& parent_struct_name = *$4;
      auto&& field_definition_list = *$6;

      DEBUG() << "Struct " << struct_name << " with parent " << parent_struct_name << "\n";
      DEBUG() << "STRUCT FIELD LIST SIZE: " << field_definition_list.size() << "\n";

      auto parent_struct = decls->GetTypeDef(parent_struct_name);
      if (parent_struct == nullptr) {
        ERRORLOC(LOC) << "Could not find struct " << parent_struct_name
                  << " used as parent for " << struct_name;
      }

      if (parent_struct->GetDefinitionType() != TypeDef::Type::STRUCT) {
        ERRORLOC(LOC) << parent_struct_name << " is not a struct";
      }
      auto struct_definition = new StructDef(std::move(struct_name), std::move(field_definition_list), (StructDef*)parent_struct);
      struct_definition->AssignSizeFields();

      $$ = struct_definition;
      delete $2;
      delete $4;
      delete $6;
    }
  | STRUCT IDENTIFIER ':' IDENTIFIER '(' constraint_list ')' '{' field_definition_list '}'
    {
      auto&& struct_name = *$2;
      auto&& parent_struct_name = *$4;
      auto&& constraints = *$6;
      auto&& field_definition_list = *$9;

      auto parent_struct = decls->GetTypeDef(parent_struct_name);
      if (parent_struct == nullptr) {
        ERRORLOC(LOC) << "Could not find struct " << parent_struct_name
                  << " used as parent for " << struct_name;
      }

      if (parent_struct->GetDefinitionType() != TypeDef::Type::STRUCT) {
        ERRORLOC(LOC) << parent_struct_name << " is not a struct";
      }

      auto struct_definition = new StructDef(std::move(struct_name), std::move(field_definition_list), (StructDef*)parent_struct);
      struct_definition->AssignSizeFields();

      for (const auto& constraint : constraints) {
        const auto& constraint_name = constraint.first;
        const auto& constraint_value = constraint.second;
        DEBUG() << "Parent constraint on " << constraint_name;
        struct_definition->AddParentConstraint(constraint_name, constraint_value);
      }

      $$ = struct_definition;

      delete $2;
      delete $4;
      delete $6;
      delete $9;
    }

packet_definition
  : PACKET IDENTIFIER '{' field_definition_list '}'  /* Packet with no parent */
    {
      auto&& packet_name = *$2;
      auto&& field_definition_list = *$4;

      DEBUG() << "Packet " << packet_name << " with no parent";
      DEBUG() << "PACKET FIELD LIST SIZE: " << field_definition_list.size();
      auto packet_definition = new PacketDef(std::move(packet_name), std::move(field_definition_list));
      packet_definition->AssignSizeFields();

      $$ = packet_definition;
      delete $2;
      delete $4;
    }
  | PACKET IDENTIFIER ':' IDENTIFIER '{' field_definition_list '}'
    {
      auto&& packet_name = *$2;
      auto&& parent_packet_name = *$4;
      auto&& field_definition_list = *$6;

      DEBUG() << "Packet " << packet_name << " with parent " << parent_packet_name << "\n";
      DEBUG() << "PACKET FIELD LIST SIZE: " << field_definition_list.size() << "\n";

      auto parent_packet = decls->GetPacketDef(parent_packet_name);
      if (parent_packet == nullptr) {
        ERRORLOC(LOC) << "Could not find packet " << parent_packet_name
                  << " used as parent for " << packet_name;
      }

      auto packet_definition = new PacketDef(std::move(packet_name), std::move(field_definition_list), parent_packet);
      packet_definition->AssignSizeFields();

      $$ = packet_definition;
      delete $2;
      delete $4;
      delete $6;
    }
  | PACKET IDENTIFIER ':' IDENTIFIER '(' constraint_list ')' '{' field_definition_list '}'
    {
      auto&& packet_name = *$2;
      auto&& parent_packet_name = *$4;
      auto&& constraints = *$6;
      auto&& field_definition_list = *$9;

      DEBUG() << "Packet " << packet_name << " with parent " << parent_packet_name << "\n";
      DEBUG() << "PACKET FIELD LIST SIZE: " << field_definition_list.size() << "\n";
      DEBUG() << "CONSTRAINT LIST SIZE: " << constraints.size() << "\n";

      auto parent_packet = decls->GetPacketDef(parent_packet_name);
      if (parent_packet == nullptr) {
        ERRORLOC(LOC) << "Could not find packet " << parent_packet_name
                  << " used as parent for " << packet_name;
      }

      auto packet_definition = new PacketDef(std::move(packet_name), std::move(field_definition_list), parent_packet);
      packet_definition->AssignSizeFields();

      for (const auto& constraint : constraints) {
        const auto& constraint_name = constraint.first;
        const auto& constraint_value = constraint.second;
        DEBUG() << "Parent constraint on " << constraint_name;
        packet_definition->AddParentConstraint(constraint_name, constraint_value);
      }

      $$ = packet_definition;

      delete $2;
      delete $4;
      delete $6;
      delete $9;
    }

field_definition_list
  : /* empty */
    {
      DEBUG() << "Empty Field definition\n";
      $$ = new FieldList();
    }
  | field_definition
    {
      DEBUG() << "Field definition\n";
      $$ = new FieldList();

      if ($1->GetFieldType() == GroupField::kFieldType) {
        auto group_fields = static_cast<GroupField*>($1)->GetFields();
	FieldList reversed_fields(group_fields->rbegin(), group_fields->rend());
        for (auto& field : reversed_fields) {
          $$->PrependField(field);
        }
	delete $1;
        break;
      }

      $$->PrependField($1);
    }
  | field_definition ',' field_definition_list
    {
      DEBUG() << "Field definition with list\n";
      $$ = $3;

      if ($1->GetFieldType() == GroupField::kFieldType) {
        auto group_fields = static_cast<GroupField*>($1)->GetFields();
	FieldList reversed_fields(group_fields->rbegin(), group_fields->rend());
        for (auto& field : reversed_fields) {
          $$->PrependField(field);
        }
	delete $1;
        break;
      }

      $$->PrependField($1);
    }

field_definition
  : group_field_definition
    {
      DEBUG() << "Group Field";
      $$ = $1;
    }
  | type_def_field_definition
    {
      DEBUG() << "Field with a pre-defined type\n";
      $$ = $1;
    }
  | scalar_field_definition
    {
      DEBUG() << "Scalar field\n";
      $$ = $1;
    }
  | checksum_start_field_definition
    {
      DEBUG() << "Checksum start field\n";
      $$ = $1;
    }
  | padding_field_definition
    {
      DEBUG() << "Padding field\n";
      $$ = $1;
    }
  | size_field_definition
    {
      DEBUG() << "Size field\n";
      $$ = $1;
    }
  | body_field_definition
    {
      DEBUG() << "Body field\n";
      $$ = $1;
    }
  | payload_field_definition
    {
      DEBUG() << "Payload field\n";
      $$ = $1;
    }
  | fixed_field_definition
    {
      DEBUG() << "Fixed field\n";
      $$ = $1;
    }
  | reserved_field_definition
    {
      DEBUG() << "Reserved field\n";
      $$ = $1;
    }
  | array_field_definition
    {
      DEBUG() << "ARRAY field\n";
      $$ = $1;
    }

group_field_definition
  : IDENTIFIER
    {
      auto group = decls->GetGroupDef(*$1);
      if (group == nullptr) {
        ERRORLOC(LOC) << "Could not find group with name " << *$1;
      }

      std::list<PacketField*>* expanded_fields;
      expanded_fields = new std::list<PacketField*>(group->begin(), group->end());
      $$ = new GroupField(LOC, expanded_fields);
      delete $1;
    }
  | IDENTIFIER '{' constraint_list '}'
    {
      DEBUG() << "Group with fixed field(s) " << *$1 << "\n";
      auto group = decls->GetGroupDef(*$1);
      if (group == nullptr) {
        ERRORLOC(LOC) << "Could not find group with name " << *$1;
      }

      std::list<PacketField*>* expanded_fields = new std::list<PacketField*>();
      for (const auto field : *group) {
        const auto constraint = $3->find(field->GetName());
        if (constraint != $3->end()) {
          if (field->GetFieldType() == ScalarField::kFieldType) {
            DEBUG() << "Fixing group scalar value\n";
            expanded_fields->push_back(new FixedScalarField(field->GetSize().bits(), std::get<int64_t>(constraint->second), LOC));
          } else if (field->GetFieldType() == EnumField::kFieldType) {
            DEBUG() << "Fixing group enum value\n";

            auto type_def = decls->GetTypeDef(field->GetDataType());
            EnumDef* enum_def = (type_def->GetDefinitionType() == TypeDef::Type::ENUM ? (EnumDef*)type_def : nullptr);
            if (enum_def == nullptr) {
              ERRORLOC(LOC) << "No enum found of type " << field->GetDataType();
            }
            if (!enum_def->HasEntry(std::get<std::string>(constraint->second))) {
              ERRORLOC(LOC) << "Enum " << field->GetDataType() << " has no enumeration " << std::get<std::string>(constraint->second);
            }

            expanded_fields->push_back(new FixedEnumField(enum_def, std::get<std::string>(constraint->second), LOC));
          } else {
            ERRORLOC(LOC) << "Unimplemented constraint of type " << field->GetFieldType();
          }
          $3->erase(constraint);
        } else {
          expanded_fields->push_back(field);
        }
      }
      if ($3->size() > 0) {
        ERRORLOC(LOC) << "Could not find member " << $3->begin()->first << " in group " << *$1;
      }

      $$ = new GroupField(LOC, expanded_fields);
      delete $1;
      delete $3;
    }

constraint_list
  : constraint ',' constraint_list
    {
      DEBUG() << "Group field value list\n";
      $3->insert(*$1);
      $$ = $3;
      delete($1);
    }
  | constraint
    {
      DEBUG() << "Group field value\n";
      $$ = new std::map<std::string, std::variant<int64_t, std::string>>();
      $$->insert(*$1);
      delete($1);
    }

constraint
  : IDENTIFIER '=' INTEGER
    {
      DEBUG() << "Group with a fixed integer value=" << $1 << " value=" << $3 << "\n";

      $$ = new std::pair(*$1, std::variant<int64_t,std::string>($3));
      delete $1;
    }
  | IDENTIFIER '=' IDENTIFIER
    {
      DEBUG() << "Group with a fixed enum field value=" << *$3 << " enum=" << *$1;

      $$ = new std::pair(*$1, std::variant<int64_t,std::string>(*$3));
      delete $1;
      delete $3;
    }

type_def_field_definition
  : IDENTIFIER ':' IDENTIFIER
    {
      DEBUG() << "Predefined type field " << *$1 << " : " << *$3 << "\n";
      if (auto type_def = decls->GetTypeDef(*$3)) {
        $$ = type_def->GetNewField(*$1, LOC);
      } else {
        ERRORLOC(LOC) << "No type with this name\n";
      }
      delete $1;
      delete $3;
    }

scalar_field_definition
  : IDENTIFIER ':' INTEGER
    {
      DEBUG() << "Scalar field " << *$1 << " : " << $3 << "\n";
      $$ = new ScalarField(*$1, $3, LOC);
      delete $1;
    }

body_field_definition
  : BODY
    {
      DEBUG() << "Body field\n";
      $$ = new BodyField(LOC);
    }

payload_field_definition
  : PAYLOAD ':' '[' SIZE_MODIFIER ']'
    {
      DEBUG() << "Payload field with modifier " << *$4 << "\n";
      $$ = new PayloadField(*$4, LOC);
      delete $4;
    }
  | PAYLOAD
    {
      DEBUG() << "Payload field\n";
      $$ = new PayloadField("", LOC);
    }

checksum_start_field_definition
  : CHECKSUM_START '(' IDENTIFIER ')'
    {
      DEBUG() << "ChecksumStart field defined\n";
      $$ = new ChecksumStartField(*$3, LOC);
      delete $3;
    }

padding_field_definition
  : PADDING '[' INTEGER ']'
    {
      DEBUG() << "Padding field defined\n";
      $$ = new PaddingField($3, LOC);
    }

size_field_definition
  : SIZE '(' IDENTIFIER ')' ':' INTEGER
    {
      DEBUG() << "Size field defined\n";
      $$ = new SizeField(*$3, $6, LOC);
      delete $3;
    }
  | SIZE '(' PAYLOAD ')' ':' INTEGER
    {
      DEBUG() << "Size for payload defined\n";
      $$ = new SizeField("payload", $6, LOC);
    }
  | COUNT '(' IDENTIFIER ')' ':' INTEGER
    {
      DEBUG() << "Count field defined\n";
      $$ = new CountField(*$3, $6, LOC);
      delete $3;
    }

fixed_field_definition
  : FIXED '=' INTEGER ':' INTEGER
    {
      DEBUG() << "Fixed field defined value=" << $3 << " size=" << $5 << "\n";
      $$ = new FixedScalarField($5, $3, LOC);
    }
  | FIXED '=' IDENTIFIER ':' IDENTIFIER
    {
      DEBUG() << "Fixed enum field defined value=" << *$3 << " enum=" << *$5;
      auto type_def = decls->GetTypeDef(*$5);
      if (type_def != nullptr) {
        EnumDef* enum_def = (type_def->GetDefinitionType() == TypeDef::Type::ENUM ? (EnumDef*)type_def : nullptr);
        if (!enum_def->HasEntry(*$3)) {
          ERRORLOC(LOC) << "Previously defined enum " << enum_def->GetTypeName() << " has no entry for " << *$3;
        }

        $$ = new FixedEnumField(enum_def, *$3, LOC);
      } else {
        ERRORLOC(LOC) << "No enum found with name " << *$5;
      }

      delete $3;
      delete $5;
    }

reserved_field_definition
  : RESERVED ':' INTEGER
    {
      DEBUG() << "Reserved field of size=" << $3 << "\n";
      $$ = new ReservedField($3, LOC);
    }

array_field_definition
  : IDENTIFIER ':' INTEGER '[' ']'
    {
      DEBUG() << "Vector field defined name=" << *$1 << " element_size=" << $3;
      $$ = new VectorField(*$1, $3, "", LOC);
      delete $1;
    }
  | IDENTIFIER ':' INTEGER '[' SIZE_MODIFIER ']'
    {
      DEBUG() << "Vector field defined name=" << *$1 << " element_size=" << $3
             << " size_modifier=" << *$5;
      $$ = new VectorField(*$1, $3, *$5, LOC);
      delete $1;
      delete $5;
    }
  | IDENTIFIER ':' INTEGER '[' INTEGER ']'
    {
      DEBUG() << "Array field defined name=" << *$1 << " element_size=" << $3
             << " fixed_size=" << $5;
      $$ = new ArrayField(*$1, $3, $5, LOC);
      delete $1;
    }
  | IDENTIFIER ':' IDENTIFIER '[' ']'
    {
      DEBUG() << "Vector field defined name=" << *$1 << " type=" << *$3;
      if (auto type_def = decls->GetTypeDef(*$3)) {
        $$ = new VectorField(*$1, type_def, "", LOC);
      } else {
        ERRORLOC(LOC) << "Can't find type used in array field.";
      }
      delete $1;
      delete $3;
    }
  | IDENTIFIER ':' IDENTIFIER '[' SIZE_MODIFIER ']'
    {
      DEBUG() << "Vector field defined name=" << *$1 << " type=" << *$3
             << " size_modifier=" << *$5;
      if (auto type_def = decls->GetTypeDef(*$3)) {
        $$ = new VectorField(*$1, type_def, *$5, LOC);
      } else {
        ERRORLOC(LOC) << "Can't find type used in array field.";
      }
      delete $1;
      delete $3;
      delete $5;
    }
  | IDENTIFIER ':' IDENTIFIER '[' INTEGER ']'
    {
      DEBUG() << "Array field defined name=" << *$1 << " type=" << *$3
             << " fixed_size=" << $5;
      if (auto type_def = decls->GetTypeDef(*$3)) {
        $$ = new ArrayField(*$1, type_def, $5, LOC);
      } else {
        ERRORLOC(LOC) << "Can't find type used in array field.";
      }
      delete $1;
      delete $3;
    }

%%


void yy::parser::error(const yy::parser::location_type& loc, const std::string& error) {
  ERROR() << error << " at location " << loc << "\n";
  abort();
}
