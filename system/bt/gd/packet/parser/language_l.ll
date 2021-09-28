%{

#include <string>
#include <map>
#include <iostream>

#include "declarations.h"
#include "language_y.h"

using token = yy::parser::token;

#define YY_USER_ACTION yylloc->step(); yylloc->columns(yyleng);

%}

%option debug

%option yylineno
%option noyywrap
%option nounput
%option noinput
%option reentrant
%option bison-bridge
%option bison-locations

identifier [a-zA-Z][_a-zA-Z0-9]*
size_modifier [+*/-][ +*/\-0-9]*
intvalue (0|[1-9][0-9]*)
hexvalue 0[x|X][0-9a-fA-F]+
string_literal \".*\"

%x COMMENT_STATE

%%
  /* NOTE:
   * Rule ordering is important in order to establist priority. Some
   * rules are a superset of other rules and will cause the sub rules to
   * never match. Ex. Keywords must always go before identifiers, otherwise
   * all keywords will be treated as an identifier.
   */

  /* Block Comment */
"/*"                    { BEGIN(COMMENT_STATE); }
<COMMENT_STATE>"*/"     { BEGIN(INITIAL); }
<COMMENT_STATE>[\n]+    { yylloc->lines(yyleng); }
<COMMENT_STATE>.        { /* do nothing */ }

  /* Line Comment */
"//"[^\r\n]*            { /* do nothing */ }

  /* Begin reserved keyword definitions */
  /* Fields */
"_body_"                { return(token::BODY); }
"_payload_"             { return(token::PAYLOAD); }
"_size_"                { return(token::SIZE); }
"_count_"               { return(token::COUNT); }
"_fixed_"               { return(token::FIXED); }
"_reserved_"            { return(token::RESERVED); }
"_checksum_start_"      { return(token::CHECKSUM_START); }
"_padding_"             { return(token::PADDING); }
  /* Types */
"checksum"              { return(token::CHECKSUM); }
"custom_field"          { return(token::CUSTOM_FIELD); }
"enum"                  { return(token::ENUM); }
"group"                 { return(token::GROUP); }
"packet"                { return(token::PACKET); }
"struct"                { return(token::STRUCT); }
"little_endian_packets" {
                          yylval->integer = 1;
                          return token::IS_LITTLE_ENDIAN;
                        }
"big_endian_packets"    {
                          yylval->integer = 0;
                          return token::IS_LITTLE_ENDIAN;
                        }

  /* Begin identifier definitions */
{string_literal}        {
                          std::string with_quotes = std::string(yytext);
                          yylval->string = new std::string(with_quotes.begin() + 1, with_quotes.end() - 1);
                          return token::STRING;
                        }

{size_modifier}         {
                          yylval->string = new std::string(yytext);
                          return token::SIZE_MODIFIER;
                        }

{identifier}            {
                          yylval->string = new std::string(yytext);
                          return token::IDENTIFIER;
                        }

{intvalue}              {
                          yylval->integer = std::stoi(std::string(yytext), nullptr, 10);
                          return token::INTEGER;
                        }

{hexvalue}              {
                          yylval->integer = std::stoi(std::string(yytext), nullptr, 16);
                          return token::INTEGER;
                        }

  /* Begin token definitions */
":"            { return(':'); }
"{"            { return('{'); }
"}"            { return('}'); }
"["            { return('['); }
"]"            { return(']'); }
"("            { return('('); }
")"            { return(')'); }
"<"            { return('<'); }
">"            { return('>'); }
"="            { return('='); }
","            { return(','); }

(\n|\r\n)+     { yylloc->lines(yyleng); }
[ \t\f\v]+     { /* Ignore all other whitespace */ }

%%

