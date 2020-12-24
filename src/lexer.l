%x incl

%{
#include <string.h>
#include "sfwbar.h"
#include "parser.tab.h"

extern int yyerror(struct context *x,const char *y);
extern char *config_str_clean ( char *str );
extern int config_line_num;
#define MAX_INCLUDE_DEPTH 10
YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
FILE *file_stack[MAX_INCLUDE_DEPTH];
char *fname_stack[MAX_INCLUDE_DEPTH];
int loc_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;

#define YY_DECL int yylex (YYSTYPE * yylval_param, struct context *context )
YY_DECL;
%}

%option bison-bridge
/* %option noyywrap */
%option noinput
%option nounput

%%

"-"			{ return MINUS; }
"+"			{ return PLUS; }
"*"			{ return MULT; }
"/"			{ return DIVIDE; }
","			{ return COMMA; }
"="		        { return EQUAL; }
"Time"			{ return TIME; }
"Mid"			{ return MIDW; }
"Extract"		{ return EXTRACT; }
"Str"			{ return STRW; }
[0-9.]+			{ yylval->num = g_ascii_strtod(yytext,NULL); return NUMBER; }
$[a-zA-Z0-9._]+		{ yylval->ptr = g_strdup(yytext); return SNAME; }
")"			{ return RPAREN; }
"("			{ return LPAREN; }
^#.*\n			{ (context->line_num)++; }
[ \t]+			;
\n			{ (context->line_num)++; }
\"[^\"\n]+\"		{ yylval->ptr=g_strndup(yytext+1,strlen(yytext)-2); return STRING; }
\'[^\']*\'	        { yylval->ptr=g_strndup(yytext+1,strlen(yytext)-2); return STRING; }
[A-Za-z0-9._]+          { yylval->ptr = g_strdup(yytext); return IDENTIFIER; }
.			{ yyerror(context,"illegal token"); }
%%
#ifndef yywrap
int yywrap() { return 1; }
#endif
