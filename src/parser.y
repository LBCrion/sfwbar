%{
#include <stdio.h>
#include <time.h>
#include "sfwbar.h"
#include "parser.tab.h"
// #define YYPARSE_PARAM config
// #define YYLEX_PARAM config
int yylex (YYSTYPE * yylval_param, struct context *context );
int yyerror(struct context *x,const char *y);
%}

%glr-parser
%define api.pure
%parse-param {struct context *context}
%lex-param {struct context *context}
%start ROOT

%union
{
  double num;
  void *ptr;
}

%token COMMA
%token INCLUDE
%token <ptr> SEMICOLON
%token QUOTE
%token <ptr> STRING
%token <ptr> IDENTIFIER
%token RPAREN
%token LPAREN
%token <num> NUMBER
%token <num> ADD
%token <num> PRODUCT
%token <num> REPLACE
%token EOL

%type <ptr> str
%type <num> num

%token MINUS
%token PLUS
%token MULT
%token DIVIDE
%token EQUAL
%token UPDSTR
%token <ptr> TIME
%token <ptr> STRW
%token <ptr> MIDW
%token <ptr> EXTRACT
%token <ptr> SNAME

%type <ptr> ROOT
%type <num> expr2
%type <num> expr3
%type <ptr> string2

%%

ROOT:
   str { context->ret_val = $1; }
 | num { context->ret_val = numeric_to_str($1,context->default_dec); }
;
  
str:
   string2 { $$ = $1; }
 | str PLUS string2 { $$ = g_strconcat ( $1, $3, NULL ); g_free($1); g_free($3); }
;

string2:
   STRING { $$ = $1; }
 | SNAME { $$ = string_from_name(context,$1); g_free($1); }
 | STRW LPAREN num COMMA num RPAREN { $$ = numeric_to_str ( $3, $5 ); }
 | MIDW LPAREN str COMMA num COMMA num RPAREN { $$ = str_mid($3,$5,$7); g_free($3); }
 | EXTRACT LPAREN str COMMA str RPAREN { $$ = extract_str($3,$5); g_free($3); g_free($5); }
 | TIME LPAREN RPAREN { $$ = time_str(NULL); }
 | TIME LPAREN str RPAREN { $$ = time_str($3); g_free($3); }
;

num:
   expr2 { $$ = $1; }
 | num MINUS expr2 { $$ = $1 - $3; }
 | num PLUS expr2 { $$ = $1 + $3; }
;

expr2:
   expr3 { $$ = $1; }
 | expr2 MULT expr3 { $$ = $1 * $3; }
 | expr2 DIVIDE expr3 { $$ = $1 / $3; }
;

expr3:
   PLUS expr3 { $$ = $2; }
 | MINUS expr3 { $$ = -$2; }
 | LPAREN num RPAREN { $$ = $2; }
 | NUMBER { $$ = $1; }
 | IDENTIFIER { $$ = numeric_from_name(context,$1); g_free($1); }
;

