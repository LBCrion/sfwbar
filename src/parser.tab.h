/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Skeleton interface for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_PARSER_TAB_H_INCLUDED
# define YY_YY_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    COMMA = 258,                   /* COMMA  */
    INCLUDE = 259,                 /* INCLUDE  */
    SEMICOLON = 260,               /* SEMICOLON  */
    QUOTE = 261,                   /* QUOTE  */
    STRING = 262,                  /* STRING  */
    IDENTIFIER = 263,              /* IDENTIFIER  */
    RPAREN = 264,                  /* RPAREN  */
    LPAREN = 265,                  /* LPAREN  */
    NUMBER = 266,                  /* NUMBER  */
    ADD = 267,                     /* ADD  */
    PRODUCT = 268,                 /* PRODUCT  */
    REPLACE = 269,                 /* REPLACE  */
    EOL = 270,                     /* EOL  */
    MINUS = 271,                   /* MINUS  */
    PLUS = 272,                    /* PLUS  */
    MULT = 273,                    /* MULT  */
    DIVIDE = 274,                  /* DIVIDE  */
    PERCENT = 275,                 /* PERCENT  */
    EQUAL = 276,                   /* EQUAL  */
    UPDSTR = 277,                  /* UPDSTR  */
    TIME = 278,                    /* TIME  */
    DF = 279,                      /* DF  */
    STRW = 280,                    /* STRW  */
    MIDW = 281,                    /* MIDW  */
    EXTRACT = 282,                 /* EXTRACT  */
    SNAME = 283                    /* SNAME  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 19 "parser.y"

  double num;
  void *ptr;

#line 92 "parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (struct context *context);

#endif /* !YY_YY_PARSER_TAB_H_INCLUDED  */
