grammar gn;


/*
 * Lexer Rules
 */

r : statementlist EOF;



//string     : '"' ( NLETTER | DIGIT | LETTER | Escape | expansion)* '"' ;

//Escape     : '\\' ( '$' | '"' | NLETTER | DIGIT | LETTER) ;

//bracketexpansion : '{' ( Identifier | arrayaccess | scopeaccess ) '}' ;

//expansion        : '$' ( Identifier | bracketexpansion | HEX ) ;

statement     : assignment | call | condition ;
lvalue        : Identifier | arrayaccess | scopeaccess ;
assignment    : lvalue AssignOp expr ;
call          : Identifier '('  exprlist? ')' block? ;
condition     : 'if' '(' expr ')' block
               (elsec ( condition | block ))? ;
block         : '{' statementlist '}' ;
statementlist  :  ( statement | comment )* ;
arrayaccess   : Identifier '[' expr ']' ;
scopeaccess   : Identifier '.' Identifier ;
expr          : unaryexpr | expr BinaryOp expr ;
unaryexpr     : primaryexpr | UnaryOp unaryexpr ;
primaryexpr   : Identifier | Integer | String | call
              | arrayaccess | scopeaccess | block
              | '(' expr ')'
              | '[' ( exprlist  ','? )? ']' ;
exprlist      : expr ( ',' expr )* ;
elsec         : 'else' ;
comment       : COMMENT ;


AssignOp : '=' | '+=' | '-=' ;
UnaryOp  : '!' ;
BinaryOp : '+' | '-'
         | '<' | '<=' | '>' | '>='
         | '==' | '!='
         | '&&'
         | '||' ;

Identifier : LETTER ( DIGIT | LETTER )* ;
Integer    : '-'? DIGIT+ ;
String     : '"' ('\\"'|~'"')* '"' ;

fragment DIGIT  : [0-9] ;
fragment LETTER : ([a-z] | [A-Z] | '_') ;
COMMENT : '#' ~[\r\n]* '\r'? '\n' -> skip ;
WS     : [ \r\n] -> skip ;

//NLETTER   : ~[\r\n$"0-9a-zA-Z_] ;
//HEX    : '0x' [0-9A-Fa-f]+ ;

