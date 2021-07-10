/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Skeleton implementation for Bison GLR parsers in C

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

/* C GLR parser skeleton written by Paul Hilfinger.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30706

/* Bison version string.  */
#define YYBISON_VERSION "3.7.6"

/* Skeleton name.  */
#define YYSKELETON_NAME "glr.c"

/* Pure parsers.  */
#define YYPURE 1






/* First part of user prologue.  */
#line 1 "parser.y"

#include <stdio.h>
#include <time.h>
#include "sfwbar.h"
#include "parser.tab.h"
// #define YYPARSE_PARAM config
// #define YYLEX_PARAM config
int yylex (YYSTYPE * yylval_param, struct context *context );
int yyerror(struct context *x,const char *y);

#line 69 "parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"

/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_COMMA = 3,                      /* COMMA  */
  YYSYMBOL_INCLUDE = 4,                    /* INCLUDE  */
  YYSYMBOL_SEMICOLON = 5,                  /* SEMICOLON  */
  YYSYMBOL_QUOTE = 6,                      /* QUOTE  */
  YYSYMBOL_STRING = 7,                     /* STRING  */
  YYSYMBOL_IDENTIFIER = 8,                 /* IDENTIFIER  */
  YYSYMBOL_RPAREN = 9,                     /* RPAREN  */
  YYSYMBOL_LPAREN = 10,                    /* LPAREN  */
  YYSYMBOL_NUMBER = 11,                    /* NUMBER  */
  YYSYMBOL_ADD = 12,                       /* ADD  */
  YYSYMBOL_PRODUCT = 13,                   /* PRODUCT  */
  YYSYMBOL_REPLACE = 14,                   /* REPLACE  */
  YYSYMBOL_EOL = 15,                       /* EOL  */
  YYSYMBOL_MINUS = 16,                     /* MINUS  */
  YYSYMBOL_PLUS = 17,                      /* PLUS  */
  YYSYMBOL_MULT = 18,                      /* MULT  */
  YYSYMBOL_DIVIDE = 19,                    /* DIVIDE  */
  YYSYMBOL_EQUAL = 20,                     /* EQUAL  */
  YYSYMBOL_UPDSTR = 21,                    /* UPDSTR  */
  YYSYMBOL_TIME = 22,                      /* TIME  */
  YYSYMBOL_STRW = 23,                      /* STRW  */
  YYSYMBOL_MIDW = 24,                      /* MIDW  */
  YYSYMBOL_EXTRACT = 25,                   /* EXTRACT  */
  YYSYMBOL_SNAME = 26,                     /* SNAME  */
  YYSYMBOL_YYACCEPT = 27,                  /* $accept  */
  YYSYMBOL_ROOT = 28,                      /* ROOT  */
  YYSYMBOL_str = 29,                       /* str  */
  YYSYMBOL_string2 = 30,                   /* string2  */
  YYSYMBOL_num = 31,                       /* num  */
  YYSYMBOL_expr2 = 32,                     /* expr2  */
  YYSYMBOL_expr3 = 33                      /* expr3  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Default (constant) value used for initialization for null
   right-hand sides.  Unlike the standard yacc.c template, here we set
   the default value of $$ to a zeroed-out value.  Since the default
   value is undefined, this behavior is technically correct.  */
static YYSTYPE yyval_default;



#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif
#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YYFREE
# define YYFREE free
#endif
#ifndef YYMALLOC
# define YYMALLOC malloc
#endif
#ifndef YYREALLOC
# define YYREALLOC realloc
#endif

#ifdef __cplusplus
  typedef bool yybool;
# define yytrue true
# define yyfalse false
#else
  /* When we move to stdbool, get rid of the various casts to yybool.  */
  typedef signed char yybool;
# define yytrue 1
# define yyfalse 0
#endif

#ifndef YYSETJMP
# include <setjmp.h>
# define YYJMP_BUF jmp_buf
# define YYSETJMP(Env) setjmp (Env)
/* Pacify Clang and ICC.  */
# define YYLONGJMP(Env, Val)                    \
 do {                                           \
   longjmp (Env, Val);                          \
   YY_ASSERT (0);                               \
 } while (yyfalse)
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)))
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) \
            || (!defined __STRICT_ANSI__ \
                && (__4 < __GNUC__ + (7 <= __GNUC_MINOR__) \
                    || (defined __apple_build_version__ \
                        ? 6000000 <= __apple_build_version__ \
                        : 3 < __clang_major__ + (5 <= __clang_minor__))))))
   /* _Noreturn works as-is.  */
# elif (2 < __GNUC__ + (8 <= __GNUC_MINOR__) || defined __clang__ \
        || 0x5110 <= __SUNPRO_C)
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   78

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  27
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  7
/* YYNRULES -- Number of rules.  */
#define YYNRULES  23
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  54
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 8
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   281

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int8 yyrline[] =
{
       0,    61,    61,    62,    66,    67,    71,    72,    73,    74,
      75,    76,    77,    81,    82,    83,    87,    88,    89,    93,
      94,    95,    96,    97
};
#endif

#define YYPACT_NINF (-19)
#define YYTABLE_NINF (-1)

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
       6,   -19,   -19,    55,   -19,    55,    55,     5,    29,    36,
      54,   -19,    43,    52,   -19,    -8,    59,   -19,    -5,   -19,
     -19,    11,    55,    31,    31,   -19,    31,    55,    55,    55,
      55,   -19,   -19,    51,    42,     4,     7,   -19,    59,    59,
     -19,   -19,   -19,    55,    55,    31,    33,    45,    58,   -19,
      55,   -19,    35,   -19
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     6,    23,     0,    22,     0,     0,     0,     0,     0,
       0,     7,     0,     2,     4,     3,    13,    16,     0,    20,
      19,     0,     0,     0,     0,     1,     0,     0,     0,     0,
       0,    21,    11,     0,     0,     0,     0,     5,    14,    15,
      17,    18,    12,     0,     0,     0,     0,     0,     0,     8,
       0,    10,     0,     9
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -19,   -19,   -18,    44,    -3,    46,    -4
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,    12,    13,    14,    15,    16,    17
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      18,    19,    20,    33,    31,    35,    36,    44,    27,    28,
      45,    27,    28,     1,     2,    21,     3,     4,     1,    34,
      32,    26,     5,     6,    26,    40,    41,    48,     7,     8,
       9,    10,    11,     7,     8,     9,    10,    11,     1,    22,
      46,    47,    49,    25,    53,    43,    23,    52,    50,    27,
      28,    27,    28,     7,     8,     9,    10,    11,    27,    28,
      42,    27,    28,     2,    24,     3,     4,    51,    26,    26,
      37,     5,     6,    38,    39,    26,     0,    29,    30
};

static const yytype_int8 yycheck[] =
{
       3,     5,     6,    21,     9,    23,    24,     3,    16,    17,
       3,    16,    17,     7,     8,    10,    10,    11,     7,    22,
       9,    17,    16,    17,    17,    29,    30,    45,    22,    23,
      24,    25,    26,    22,    23,    24,    25,    26,     7,    10,
      43,    44,     9,     0,     9,     3,    10,    50,     3,    16,
      17,    16,    17,    22,    23,    24,    25,    26,    16,    17,
       9,    16,    17,     8,    10,    10,    11,     9,    17,    17,
      26,    16,    17,    27,    28,    17,    -1,    18,    19
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     7,     8,    10,    11,    16,    17,    22,    23,    24,
      25,    26,    28,    29,    30,    31,    32,    33,    31,    33,
      33,    10,    10,    10,    10,     0,    17,    16,    17,    18,
      19,     9,     9,    29,    31,    29,    29,    30,    32,    32,
      33,    33,     9,     3,     3,     3,    31,    31,    29,     9,
       3,     9,    31,     9
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    27,    28,    28,    29,    29,    30,    30,    30,    30,
      30,    30,    30,    31,    31,    31,    32,    32,    32,    33,
      33,    33,    33,    33
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     1,     3,     1,     1,     6,     8,
       6,     3,     4,     1,     3,     3,     1,     3,     3,     2,
       2,     3,     1,     1
};


/* YYDPREC[RULE-NUM] -- Dynamic precedence of rule #RULE-NUM (0 if none).  */
static const yytype_int8 yydprec[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0
};

/* YYMERGER[RULE-NUM] -- Index of merging function for rule #RULE-NUM.  */
static const yytype_int8 yymerger[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0
};

/* YYIMMEDIATE[RULE-NUM] -- True iff rule #RULE-NUM is not to be deferred, as
   in the case of predicates.  */
static const yybool yyimmediate[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0
};




#undef yynerrs
#define yynerrs (yystackp->yyerrcnt)
#undef yychar
#define yychar (yystackp->yyrawchar)
#undef yylval
#define yylval (yystackp->yyval)
#undef yylloc
#define yylloc (yystackp->yyloc)


enum { YYENOMEM = -2 };

typedef enum { yyok, yyaccept, yyabort, yyerr } YYRESULTTAG;

#define YYCHK(YYE)                              \
  do {                                          \
    YYRESULTTAG yychk_flag = YYE;               \
    if (yychk_flag != yyok)                     \
      return yychk_flag;                        \
  } while (0)

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYMAXDEPTH * sizeof (GLRStackItem)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Minimum number of free items on the stack allowed after an
   allocation.  This is to allow allocation and initialization
   to be completed by functions that call yyexpandGLRStack before the
   stack is expanded, thus insuring that all necessary pointers get
   properly redirected to new data.  */
#define YYHEADROOM 2

#ifndef YYSTACKEXPANDABLE
#  define YYSTACKEXPANDABLE 1
#endif

#if YYSTACKEXPANDABLE
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyexpandGLRStack (Yystack);                       \
  } while (0)
#else
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyMemoryExhausted (Yystack);                      \
  } while (0)
#endif

/** State numbers. */
typedef int yy_state_t;

/** Rule numbers. */
typedef int yyRuleNum;

/** Item references. */
typedef short yyItemNum;

typedef struct yyGLRState yyGLRState;
typedef struct yyGLRStateSet yyGLRStateSet;
typedef struct yySemanticOption yySemanticOption;
typedef union yyGLRStackItem yyGLRStackItem;
typedef struct yyGLRStack yyGLRStack;

struct yyGLRState {
  /** Type tag: always true.  */
  yybool yyisState;
  /** Type tag for yysemantics.  If true, yysval applies, otherwise
   *  yyfirstVal applies.  */
  yybool yyresolved;
  /** Number of corresponding LALR(1) machine state.  */
  yy_state_t yylrState;
  /** Preceding state in this stack */
  yyGLRState* yypred;
  /** Source position of the last token produced by my symbol */
  YYPTRDIFF_T yyposn;
  union {
    /** First in a chain of alternative reductions producing the
     *  nonterminal corresponding to this state, threaded through
     *  yynext.  */
    yySemanticOption* yyfirstVal;
    /** Semantic value for this state.  */
    YYSTYPE yysval;
  } yysemantics;
};

struct yyGLRStateSet {
  yyGLRState** yystates;
  /** During nondeterministic operation, yylookaheadNeeds tracks which
   *  stacks have actually needed the current lookahead.  During deterministic
   *  operation, yylookaheadNeeds[0] is not maintained since it would merely
   *  duplicate yychar != YYEMPTY.  */
  yybool* yylookaheadNeeds;
  YYPTRDIFF_T yysize;
  YYPTRDIFF_T yycapacity;
};

struct yySemanticOption {
  /** Type tag: always false.  */
  yybool yyisState;
  /** Rule number for this reduction */
  yyRuleNum yyrule;
  /** The last RHS state in the list of states to be reduced.  */
  yyGLRState* yystate;
  /** The lookahead for this reduction.  */
  int yyrawchar;
  YYSTYPE yyval;
  /** Next sibling in chain of options.  To facilitate merging,
   *  options are chained in decreasing order by address.  */
  yySemanticOption* yynext;
};

/** Type of the items in the GLR stack.  The yyisState field
 *  indicates which item of the union is valid.  */
union yyGLRStackItem {
  yyGLRState yystate;
  yySemanticOption yyoption;
};

struct yyGLRStack {
  int yyerrState;


  int yyerrcnt;
  int yyrawchar;
  YYSTYPE yyval;

  YYJMP_BUF yyexception_buffer;
  yyGLRStackItem* yyitems;
  yyGLRStackItem* yynextFree;
  YYPTRDIFF_T yyspaceLeft;
  yyGLRState* yysplitPoint;
  yyGLRState* yylastDeleted;
  yyGLRStateSet yytops;
};

#if YYSTACKEXPANDABLE
static void yyexpandGLRStack (yyGLRStack* yystackp);
#endif

_Noreturn static void
yyFail (yyGLRStack* yystackp, struct context *context, const char* yymsg)
{
  if (yymsg != YY_NULLPTR)
    yyerror (context, yymsg);
  YYLONGJMP (yystackp->yyexception_buffer, 1);
}

_Noreturn static void
yyMemoryExhausted (yyGLRStack* yystackp)
{
  YYLONGJMP (yystackp->yyexception_buffer, 2);
}

/** Accessing symbol of state YYSTATE.  */
static inline yysymbol_kind_t
yy_accessing_symbol (yy_state_t yystate)
{
  return YY_CAST (yysymbol_kind_t, yystos[yystate]);
}

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "COMMA", "INCLUDE",
  "SEMICOLON", "QUOTE", "STRING", "IDENTIFIER", "RPAREN", "LPAREN",
  "NUMBER", "ADD", "PRODUCT", "REPLACE", "EOL", "MINUS", "PLUS", "MULT",
  "DIVIDE", "EQUAL", "UPDSTR", "TIME", "STRW", "MIDW", "EXTRACT", "SNAME",
  "$accept", "ROOT", "str", "string2", "num", "expr2", "expr3", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#if YYDEBUG

# ifndef YYFPRINTF
#  define YYFPRINTF fprintf
# endif

# define YY_FPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_FPRINTF_

# define YY_FPRINTF_(Args)                      \
  do {                                          \
    YYFPRINTF Args;                             \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

# define YY_DPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_DPRINTF_

# define YY_DPRINTF_(Args)                      \
  do {                                          \
    if (yydebug)                                \
      YYFPRINTF Args;                           \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

/* This macro is provided for backward compatibility. */
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif



/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct context *context)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (context);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct context *context)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, context);
  YYFPRINTF (yyo, ")");
}

# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                  \
  do {                                                                  \
    if (yydebug)                                                        \
      {                                                                 \
        YY_FPRINTF ((stderr, "%s ", Title));                            \
        yy_symbol_print (stderr, Kind, Value, context);        \
        YY_FPRINTF ((stderr, "\n"));                                    \
      }                                                                 \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;

static void yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
  YY_ATTRIBUTE_UNUSED;
static void yypdumpstack (yyGLRStack* yystackp)
  YY_ATTRIBUTE_UNUSED;

#else /* !YYDEBUG */

# define YY_DPRINTF(Args) do {} while (yyfalse)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)

#endif /* !YYDEBUG */



/** Fill in YYVSP[YYLOW1 .. YYLOW0-1] from the chain of states starting
 *  at YYVSP[YYLOW0].yystate.yypred.  Leaves YYVSP[YYLOW1].yystate.yypred
 *  containing the pointer to the next state in the chain.  */
static void yyfillin (yyGLRStackItem *, int, int) YY_ATTRIBUTE_UNUSED;
static void
yyfillin (yyGLRStackItem *yyvsp, int yylow0, int yylow1)
{
  int i;
  yyGLRState *s = yyvsp[yylow0].yystate.yypred;
  for (i = yylow0-1; i >= yylow1; i -= 1)
    {
#if YYDEBUG
      yyvsp[i].yystate.yylrState = s->yylrState;
#endif
      yyvsp[i].yystate.yyresolved = s->yyresolved;
      if (s->yyresolved)
        yyvsp[i].yystate.yysemantics.yysval = s->yysemantics.yysval;
      else
        /* The effect of using yysval or yyloc (in an immediate rule) is
         * undefined.  */
        yyvsp[i].yystate.yysemantics.yyfirstVal = YY_NULLPTR;
      s = yyvsp[i].yystate.yypred = s->yypred;
    }
}


/** If yychar is empty, fetch the next token.  */
static inline yysymbol_kind_t
yygetToken (int *yycharp, yyGLRStack* yystackp, struct context *context)
{
  yysymbol_kind_t yytoken;
  YY_USE (context);
  if (*yycharp == YYEMPTY)
    {
      YY_DPRINTF ((stderr, "Reading a token\n"));
      *yycharp = yylex (&yylval, context);
    }
  if (*yycharp <= YYEOF)
    {
      *yycharp = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YY_DPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (*yycharp);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }
  return yytoken;
}

/* Do nothing if YYNORMAL or if *YYLOW <= YYLOW1.  Otherwise, fill in
 * YYVSP[YYLOW1 .. *YYLOW-1] as in yyfillin and set *YYLOW = YYLOW1.
 * For convenience, always return YYLOW1.  */
static inline int yyfill (yyGLRStackItem *, int *, int, yybool)
     YY_ATTRIBUTE_UNUSED;
static inline int
yyfill (yyGLRStackItem *yyvsp, int *yylow, int yylow1, yybool yynormal)
{
  if (!yynormal && yylow1 < *yylow)
    {
      yyfillin (yyvsp, *yylow, yylow1);
      *yylow = yylow1;
    }
  return yylow1;
}

/** Perform user action for rule number YYN, with RHS length YYRHSLEN,
 *  and top stack item YYVSP.  YYLVALP points to place to put semantic
 *  value ($$), and yylocp points to place for location information
 *  (@$).  Returns yyok for normal return, yyaccept for YYACCEPT,
 *  yyerr for YYERROR, yyabort for YYABORT.  */
static YYRESULTTAG
yyuserAction (yyRuleNum yyn, int yyrhslen, yyGLRStackItem* yyvsp,
              yyGLRStack* yystackp,
              YYSTYPE* yyvalp, struct context *context)
{
  yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  int yylow;
  YY_USE (yyvalp);
  YY_USE (context);
  YY_USE (yyrhslen);
# undef yyerrok
# define yyerrok (yystackp->yyerrState = 0)
# undef YYACCEPT
# define YYACCEPT return yyaccept
# undef YYABORT
# define YYABORT return yyabort
# undef YYERROR
# define YYERROR return yyerrok, yyerr
# undef YYRECOVERING
# define YYRECOVERING() (yystackp->yyerrState != 0)
# undef yyclearin
# define yyclearin (yychar = YYEMPTY)
# undef YYFILL
# define YYFILL(N) yyfill (yyvsp, &yylow, (N), yynormal)
# undef YYBACKUP
# define YYBACKUP(Token, Value)                                              \
  return yyerror (context, YY_("syntax error: cannot back up")),     \
         yyerrok, yyerr

  yylow = 1;
  if (yyrhslen == 0)
    *yyvalp = yyval_default;
  else
    *yyvalp = yyvsp[YYFILL (1-yyrhslen)].yystate.yysemantics.yysval;
  switch (yyn)
    {
  case 2: /* ROOT: str  */
#line 61 "parser.y"
       { context->ret_val = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr); }
#line 1008 "parser.tab.c"
    break;

  case 3: /* ROOT: num  */
#line 62 "parser.y"
       { context->ret_val = numeric_to_str((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num),context->default_dec); }
#line 1014 "parser.tab.c"
    break;

  case 4: /* str: string2  */
#line 66 "parser.y"
           { ((*yyvalp).ptr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr); }
#line 1020 "parser.tab.c"
    break;

  case 5: /* str: str PLUS string2  */
#line 67 "parser.y"
                    { ((*yyvalp).ptr) = g_strconcat ( (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.ptr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr), NULL ); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr)); }
#line 1026 "parser.tab.c"
    break;

  case 6: /* string2: STRING  */
#line 71 "parser.y"
          { ((*yyvalp).ptr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr); }
#line 1032 "parser.tab.c"
    break;

  case 7: /* string2: SNAME  */
#line 72 "parser.y"
         { ((*yyvalp).ptr) = string_from_name(context,(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr)); }
#line 1038 "parser.tab.c"
    break;

  case 8: /* string2: STRW LPAREN num COMMA num RPAREN  */
#line 73 "parser.y"
                                    { ((*yyvalp).ptr) = numeric_to_str ( (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval.num), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.num) ); }
#line 1044 "parser.tab.c"
    break;

  case 9: /* string2: MIDW LPAREN str COMMA num COMMA num RPAREN  */
#line 74 "parser.y"
                                              { ((*yyvalp).ptr) = str_mid((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yysval.ptr),(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval.num),(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.num)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yysval.ptr)); }
#line 1050 "parser.tab.c"
    break;

  case 10: /* string2: EXTRACT LPAREN str COMMA str RPAREN  */
#line 75 "parser.y"
                                       { ((*yyvalp).ptr) = extract_str((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval.ptr),(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.ptr)); }
#line 1056 "parser.tab.c"
    break;

  case 11: /* string2: TIME LPAREN RPAREN  */
#line 76 "parser.y"
                      { ((*yyvalp).ptr) = time_str(NULL); }
#line 1062 "parser.tab.c"
    break;

  case 12: /* string2: TIME LPAREN str RPAREN  */
#line 77 "parser.y"
                          { ((*yyvalp).ptr) = time_str((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.ptr)); }
#line 1068 "parser.tab.c"
    break;

  case 13: /* num: expr2  */
#line 81 "parser.y"
         { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1074 "parser.tab.c"
    break;

  case 14: /* num: num MINUS expr2  */
#line 82 "parser.y"
                   { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.num) - (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1080 "parser.tab.c"
    break;

  case 15: /* num: num PLUS expr2  */
#line 83 "parser.y"
                  { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.num) + (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1086 "parser.tab.c"
    break;

  case 16: /* expr2: expr3  */
#line 87 "parser.y"
         { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1092 "parser.tab.c"
    break;

  case 17: /* expr2: expr2 MULT expr3  */
#line 88 "parser.y"
                    { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.num) * (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1098 "parser.tab.c"
    break;

  case 18: /* expr2: expr2 DIVIDE expr3  */
#line 89 "parser.y"
                      { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yysval.num) / (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1104 "parser.tab.c"
    break;

  case 19: /* expr3: PLUS expr3  */
#line 93 "parser.y"
              { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1110 "parser.tab.c"
    break;

  case 20: /* expr3: MINUS expr3  */
#line 94 "parser.y"
               { ((*yyvalp).num) = -(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1116 "parser.tab.c"
    break;

  case 21: /* expr3: LPAREN num RPAREN  */
#line 95 "parser.y"
                     { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yysval.num); }
#line 1122 "parser.tab.c"
    break;

  case 22: /* expr3: NUMBER  */
#line 96 "parser.y"
          { ((*yyvalp).num) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.num); }
#line 1128 "parser.tab.c"
    break;

  case 23: /* expr3: IDENTIFIER  */
#line 97 "parser.y"
              { ((*yyvalp).num) = numeric_from_name(context,(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr)); g_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yysval.ptr)); }
#line 1134 "parser.tab.c"
    break;


#line 1138 "parser.tab.c"

      default: break;
    }

  return yyok;
# undef yyerrok
# undef YYABORT
# undef YYACCEPT
# undef YYERROR
# undef YYBACKUP
# undef yyclearin
# undef YYRECOVERING
}


static void
yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1)
{
  YY_USE (yy0);
  YY_USE (yy1);

  switch (yyn)
    {

      default: break;
    }
}

                              /* Bison grammar-table manipulation.  */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, struct context *context)
{
  YY_USE (yyvaluep);
  YY_USE (context);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/** Number of symbols composing the right hand side of rule #RULE.  */
static inline int
yyrhsLength (yyRuleNum yyrule)
{
  return yyr2[yyrule];
}

static void
yydestroyGLRState (char const *yymsg, yyGLRState *yys, struct context *context)
{
  if (yys->yyresolved)
    yydestruct (yymsg, yy_accessing_symbol (yys->yylrState),
                &yys->yysemantics.yysval, context);
  else
    {
#if YYDEBUG
      if (yydebug)
        {
          if (yys->yysemantics.yyfirstVal)
            YY_FPRINTF ((stderr, "%s unresolved", yymsg));
          else
            YY_FPRINTF ((stderr, "%s incomplete", yymsg));
          YY_SYMBOL_PRINT ("", yy_accessing_symbol (yys->yylrState), YY_NULLPTR, &yys->yyloc);
        }
#endif

      if (yys->yysemantics.yyfirstVal)
        {
          yySemanticOption *yyoption = yys->yysemantics.yyfirstVal;
          yyGLRState *yyrh;
          int yyn;
          for (yyrh = yyoption->yystate, yyn = yyrhsLength (yyoption->yyrule);
               yyn > 0;
               yyrh = yyrh->yypred, yyn -= 1)
            yydestroyGLRState (yymsg, yyrh, context);
        }
    }
}

/** Left-hand-side symbol for rule #YYRULE.  */
static inline yysymbol_kind_t
yylhsNonterm (yyRuleNum yyrule)
{
  return YY_CAST (yysymbol_kind_t, yyr1[yyrule]);
}

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

/** True iff LR state YYSTATE has only a default reduction (regardless
 *  of token).  */
static inline yybool
yyisDefaultedState (yy_state_t yystate)
{
  return yypact_value_is_default (yypact[yystate]);
}

/** The default reduction for YYSTATE, assuming it has one.  */
static inline yyRuleNum
yydefaultAction (yy_state_t yystate)
{
  return yydefact[yystate];
}

#define yytable_value_is_error(Yyn) \
  0

/** The action to take in YYSTATE on seeing YYTOKEN.
 *  Result R means
 *    R < 0:  Reduce on rule -R.
 *    R = 0:  Error.
 *    R > 0:  Shift to state R.
 *  Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list
 *  of conflicting reductions.
 */
static inline int
yygetLRActions (yy_state_t yystate, yysymbol_kind_t yytoken, const short** yyconflicts)
{
  int yyindex = yypact[yystate] + yytoken;
  if (yytoken == YYSYMBOL_YYerror)
    {
      // This is the error token.
      *yyconflicts = yyconfl;
      return 0;
    }
  else if (yyisDefaultedState (yystate)
           || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != yytoken)
    {
      *yyconflicts = yyconfl;
      return -yydefact[yystate];
    }
  else if (! yytable_value_is_error (yytable[yyindex]))
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return yytable[yyindex];
    }
  else
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return 0;
    }
}

/** Compute post-reduction state.
 * \param yystate   the current state
 * \param yysym     the nonterminal to push on the stack
 */
static inline yy_state_t
yyLRgotoState (yy_state_t yystate, yysymbol_kind_t yysym)
{
  int yyr = yypgoto[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == yystate)
    return yytable[yyr];
  else
    return yydefgoto[yysym - YYNTOKENS];
}

static inline yybool
yyisShiftAction (int yyaction)
{
  return 0 < yyaction;
}

static inline yybool
yyisErrorAction (int yyaction)
{
  return yyaction == 0;
}

                                /* GLRStates */

/** Return a fresh GLRStackItem in YYSTACKP.  The item is an LR state
 *  if YYISSTATE, and otherwise a semantic option.  Callers should call
 *  YY_RESERVE_GLRSTACK afterwards to make sure there is sufficient
 *  headroom.  */

static inline yyGLRStackItem*
yynewGLRStackItem (yyGLRStack* yystackp, yybool yyisState)
{
  yyGLRStackItem* yynewItem = yystackp->yynextFree;
  yystackp->yyspaceLeft -= 1;
  yystackp->yynextFree += 1;
  yynewItem->yystate.yyisState = yyisState;
  return yynewItem;
}

/** Add a new semantic action that will execute the action for rule
 *  YYRULE on the semantic values in YYRHS to the list of
 *  alternative actions for YYSTATE.  Assumes that YYRHS comes from
 *  stack #YYK of *YYSTACKP. */
static void
yyaddDeferredAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyGLRState* yystate,
                     yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yySemanticOption* yynewOption =
    &yynewGLRStackItem (yystackp, yyfalse)->yyoption;
  YY_ASSERT (!yynewOption->yyisState);
  yynewOption->yystate = yyrhs;
  yynewOption->yyrule = yyrule;
  if (yystackp->yytops.yylookaheadNeeds[yyk])
    {
      yynewOption->yyrawchar = yychar;
      yynewOption->yyval = yylval;
    }
  else
    yynewOption->yyrawchar = YYEMPTY;
  yynewOption->yynext = yystate->yysemantics.yyfirstVal;
  yystate->yysemantics.yyfirstVal = yynewOption;

  YY_RESERVE_GLRSTACK (yystackp);
}

                                /* GLRStacks */

/** Initialize YYSET to a singleton set containing an empty stack.  */
static yybool
yyinitStateSet (yyGLRStateSet* yyset)
{
  yyset->yysize = 1;
  yyset->yycapacity = 16;
  yyset->yystates
    = YY_CAST (yyGLRState**,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yystates[0]));
  if (! yyset->yystates)
    return yyfalse;
  yyset->yystates[0] = YY_NULLPTR;
  yyset->yylookaheadNeeds
    = YY_CAST (yybool*,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yylookaheadNeeds[0]));
  if (! yyset->yylookaheadNeeds)
    {
      YYFREE (yyset->yystates);
      return yyfalse;
    }
  memset (yyset->yylookaheadNeeds,
          0,
          YY_CAST (YYSIZE_T, yyset->yycapacity) * sizeof yyset->yylookaheadNeeds[0]);
  return yytrue;
}

static void yyfreeStateSet (yyGLRStateSet* yyset)
{
  YYFREE (yyset->yystates);
  YYFREE (yyset->yylookaheadNeeds);
}

/** Initialize *YYSTACKP to a single empty stack, with total maximum
 *  capacity for all stacks of YYSIZE.  */
static yybool
yyinitGLRStack (yyGLRStack* yystackp, YYPTRDIFF_T yysize)
{
  yystackp->yyerrState = 0;
  yynerrs = 0;
  yystackp->yyspaceLeft = yysize;
  yystackp->yyitems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yysize)
                         * sizeof yystackp->yynextFree[0]));
  if (!yystackp->yyitems)
    return yyfalse;
  yystackp->yynextFree = yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;
  return yyinitStateSet (&yystackp->yytops);
}


#if YYSTACKEXPANDABLE
# define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE)                   \
  &((YYTOITEMS)                                                         \
    - ((YYFROMITEMS) - YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX))))->YYTYPE

/** If *YYSTACKP is expandable, extend it.  WARNING: Pointers into the
    stack from outside should be considered invalid after this call.
    We always expand when there are 1 or fewer items left AFTER an
    allocation, so that we can avoid having external pointers exist
    across an allocation.  */
static void
yyexpandGLRStack (yyGLRStack* yystackp)
{
  yyGLRStackItem* yynewItems;
  yyGLRStackItem* yyp0, *yyp1;
  YYPTRDIFF_T yynewSize;
  YYPTRDIFF_T yyn;
  YYPTRDIFF_T yysize = yystackp->yynextFree - yystackp->yyitems;
  if (YYMAXDEPTH - YYHEADROOM < yysize)
    yyMemoryExhausted (yystackp);
  yynewSize = 2*yysize;
  if (YYMAXDEPTH < yynewSize)
    yynewSize = YYMAXDEPTH;
  yynewItems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yynewSize)
                         * sizeof yynewItems[0]));
  if (! yynewItems)
    yyMemoryExhausted (yystackp);
  for (yyp0 = yystackp->yyitems, yyp1 = yynewItems, yyn = yysize;
       0 < yyn;
       yyn -= 1, yyp0 += 1, yyp1 += 1)
    {
      *yyp1 = *yyp0;
      if (*YY_REINTERPRET_CAST (yybool *, yyp0))
        {
          yyGLRState* yys0 = &yyp0->yystate;
          yyGLRState* yys1 = &yyp1->yystate;
          if (yys0->yypred != YY_NULLPTR)
            yys1->yypred =
              YYRELOC (yyp0, yyp1, yys0->yypred, yystate);
          if (! yys0->yyresolved && yys0->yysemantics.yyfirstVal != YY_NULLPTR)
            yys1->yysemantics.yyfirstVal =
              YYRELOC (yyp0, yyp1, yys0->yysemantics.yyfirstVal, yyoption);
        }
      else
        {
          yySemanticOption* yyv0 = &yyp0->yyoption;
          yySemanticOption* yyv1 = &yyp1->yyoption;
          if (yyv0->yystate != YY_NULLPTR)
            yyv1->yystate = YYRELOC (yyp0, yyp1, yyv0->yystate, yystate);
          if (yyv0->yynext != YY_NULLPTR)
            yyv1->yynext = YYRELOC (yyp0, yyp1, yyv0->yynext, yyoption);
        }
    }
  if (yystackp->yysplitPoint != YY_NULLPTR)
    yystackp->yysplitPoint = YYRELOC (yystackp->yyitems, yynewItems,
                                      yystackp->yysplitPoint, yystate);

  for (yyn = 0; yyn < yystackp->yytops.yysize; yyn += 1)
    if (yystackp->yytops.yystates[yyn] != YY_NULLPTR)
      yystackp->yytops.yystates[yyn] =
        YYRELOC (yystackp->yyitems, yynewItems,
                 yystackp->yytops.yystates[yyn], yystate);
  YYFREE (yystackp->yyitems);
  yystackp->yyitems = yynewItems;
  yystackp->yynextFree = yynewItems + yysize;
  yystackp->yyspaceLeft = yynewSize - yysize;
}
#endif

static void
yyfreeGLRStack (yyGLRStack* yystackp)
{
  YYFREE (yystackp->yyitems);
  yyfreeStateSet (&yystackp->yytops);
}

/** Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the
 *  splitpoint of *YYSTACKP, if needed, so that it is at least as deep as
 *  YYS.  */
static inline void
yyupdateSplit (yyGLRStack* yystackp, yyGLRState* yys)
{
  if (yystackp->yysplitPoint != YY_NULLPTR && yystackp->yysplitPoint > yys)
    yystackp->yysplitPoint = yys;
}

/** Invalidate stack #YYK in *YYSTACKP.  */
static inline void
yymarkStackDeleted (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    yystackp->yylastDeleted = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yystates[yyk] = YY_NULLPTR;
}

/** Undelete the last stack in *YYSTACKP that was marked as deleted.  Can
    only be done once after a deletion, and only when all other stacks have
    been deleted.  */
static void
yyundeleteLastStack (yyGLRStack* yystackp)
{
  if (yystackp->yylastDeleted == YY_NULLPTR || yystackp->yytops.yysize != 0)
    return;
  yystackp->yytops.yystates[0] = yystackp->yylastDeleted;
  yystackp->yytops.yysize = 1;
  YY_DPRINTF ((stderr, "Restoring last deleted stack as stack #0.\n"));
  yystackp->yylastDeleted = YY_NULLPTR;
}

static inline void
yyremoveDeletes (yyGLRStack* yystackp)
{
  YYPTRDIFF_T yyi, yyj;
  yyi = yyj = 0;
  while (yyj < yystackp->yytops.yysize)
    {
      if (yystackp->yytops.yystates[yyi] == YY_NULLPTR)
        {
          if (yyi == yyj)
            YY_DPRINTF ((stderr, "Removing dead stacks.\n"));
          yystackp->yytops.yysize -= 1;
        }
      else
        {
          yystackp->yytops.yystates[yyj] = yystackp->yytops.yystates[yyi];
          /* In the current implementation, it's unnecessary to copy
             yystackp->yytops.yylookaheadNeeds[yyi] since, after
             yyremoveDeletes returns, the parser immediately either enters
             deterministic operation or shifts a token.  However, it doesn't
             hurt, and the code might evolve to need it.  */
          yystackp->yytops.yylookaheadNeeds[yyj] =
            yystackp->yytops.yylookaheadNeeds[yyi];
          if (yyj != yyi)
            YY_DPRINTF ((stderr, "Rename stack %ld -> %ld.\n",
                        YY_CAST (long, yyi), YY_CAST (long, yyj)));
          yyj += 1;
        }
      yyi += 1;
    }
}

/** Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR
 * state YYLRSTATE, at input position YYPOSN, with (resolved) semantic
 * value *YYVALP and source location *YYLOCP.  */
static inline void
yyglrShift (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
            YYPTRDIFF_T yyposn,
            YYSTYPE* yyvalp)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yytrue;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yysval = *yyvalp;
  yystackp->yytops.yystates[yyk] = yynewState;

  YY_RESERVE_GLRSTACK (yystackp);
}

/** Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR
 *  state YYLRSTATE, at input position YYPOSN, with the (unresolved)
 *  semantic value of YYRHS under the action for YYRULE.  */
static inline void
yyglrShiftDefer (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
                 YYPTRDIFF_T yyposn, yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;
  YY_ASSERT (yynewState->yyisState);

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yyfalse;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyfirstVal = YY_NULLPTR;
  yystackp->yytops.yystates[yyk] = yynewState;

  /* Invokes YY_RESERVE_GLRSTACK.  */
  yyaddDeferredAction (yystackp, yyk, yynewState, yyrhs, yyrule);
}

#if !YYDEBUG
# define YY_REDUCE_PRINT(Args)
#else
# define YY_REDUCE_PRINT(Args)          \
  do {                                  \
    if (yydebug)                        \
      yy_reduce_print Args;             \
  } while (0)

/*----------------------------------------------------------------------.
| Report that stack #YYK of *YYSTACKP is going to be reduced by YYRULE. |
`----------------------------------------------------------------------*/

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule, struct context *context)
{
  int yynrhs = yyrhsLength (yyrule);
  int yyi;
  YY_FPRINTF ((stderr, "Reducing stack %ld by rule %d (line %d):\n",
               YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
  if (! yynormal)
    yyfillin (yyvsp, 1, -yynrhs);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YY_FPRINTF ((stderr, "   $%d = ", yyi + 1));
      yy_symbol_print (stderr,
                       yy_accessing_symbol (yyvsp[yyi - yynrhs + 1].yystate.yylrState),
                       &yyvsp[yyi - yynrhs + 1].yystate.yysemantics.yysval                       , context);
      if (!yyvsp[yyi - yynrhs + 1].yystate.yyresolved)
        YY_FPRINTF ((stderr, " (unresolved)"));
      YY_FPRINTF ((stderr, "\n"));
    }
}
#endif

/** Pop the symbols consumed by reduction #YYRULE from the top of stack
 *  #YYK of *YYSTACKP, and perform the appropriate semantic action on their
 *  semantic values.  Assumes that all ambiguities in semantic values
 *  have been previously resolved.  Set *YYVALP to the resulting value,
 *  and *YYLOCP to the computed location (if any).  Return value is as
 *  for userAction.  */
static inline YYRESULTTAG
yydoAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
            YYSTYPE* yyvalp, struct context *context)
{
  int yynrhs = yyrhsLength (yyrule);

  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      /* Standard special case: single stack.  */
      yyGLRStackItem* yyrhs
        = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yytops.yystates[yyk]);
      YY_ASSERT (yyk == 0);
      yystackp->yynextFree -= yynrhs;
      yystackp->yyspaceLeft += yynrhs;
      yystackp->yytops.yystates[0] = & yystackp->yynextFree[-1].yystate;
      YY_REDUCE_PRINT ((yytrue, yyrhs, yyk, yyrule, context));
      return yyuserAction (yyrule, yynrhs, yyrhs, yystackp,
                           yyvalp, context);
    }
  else
    {
      yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
      yyGLRState* yys = yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred
        = yystackp->yytops.yystates[yyk];
      int yyi;
      for (yyi = 0; yyi < yynrhs; yyi += 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yystackp->yytops.yystates[yyk] = yys;
      YY_REDUCE_PRINT ((yyfalse, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1, yyk, yyrule, context));
      return yyuserAction (yyrule, yynrhs, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyvalp, context);
    }
}

/** Pop items off stack #YYK of *YYSTACKP according to grammar rule YYRULE,
 *  and push back on the resulting nonterminal symbol.  Perform the
 *  semantic action associated with YYRULE and store its value with the
 *  newly pushed state, if YYFORCEEVAL or if *YYSTACKP is currently
 *  unambiguous.  Otherwise, store the deferred semantic action with
 *  the new state.  If the new state would have an identical input
 *  position, LR state, and predecessor to an existing state on the stack,
 *  it is identified with that existing state, eliminating stack #YYK from
 *  *YYSTACKP.  In this case, the semantic value is
 *  added to the options for the existing state's semantic value.
 */
static inline YYRESULTTAG
yyglrReduce (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
             yybool yyforceEval, struct context *context)
{
  YYPTRDIFF_T yyposn = yystackp->yytops.yystates[yyk]->yyposn;

  if (yyforceEval || yystackp->yysplitPoint == YY_NULLPTR)
    {
      YYSTYPE yysval;

      YYRESULTTAG yyflag = yydoAction (yystackp, yyk, yyrule, &yysval, context);
      if (yyflag == yyerr && yystackp->yysplitPoint != YY_NULLPTR)
        YY_DPRINTF ((stderr,
                     "Parse on stack %ld rejected by rule %d (line %d).\n",
                     YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule - 1]));
      if (yyflag != yyok)
        return yyflag;
      YY_SYMBOL_PRINT ("-> $$ =", yylhsNonterm (yyrule), &yysval, &yyloc);
      yyglrShift (yystackp, yyk,
                  yyLRgotoState (yystackp->yytops.yystates[yyk]->yylrState,
                                 yylhsNonterm (yyrule)),
                  yyposn, &yysval);
    }
  else
    {
      YYPTRDIFF_T yyi;
      int yyn;
      yyGLRState* yys, *yys0 = yystackp->yytops.yystates[yyk];
      yy_state_t yynewLRState;

      for (yys = yystackp->yytops.yystates[yyk], yyn = yyrhsLength (yyrule);
           0 < yyn; yyn -= 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yynewLRState = yyLRgotoState (yys->yylrState, yylhsNonterm (yyrule));
      YY_DPRINTF ((stderr,
                   "Reduced stack %ld by rule %d (line %d); action deferred.  "
                   "Now in state %d.\n",
                   YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule - 1],
                   yynewLRState));
      for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
        if (yyi != yyk && yystackp->yytops.yystates[yyi] != YY_NULLPTR)
          {
            yyGLRState *yysplit = yystackp->yysplitPoint;
            yyGLRState *yyp = yystackp->yytops.yystates[yyi];
            while (yyp != yys && yyp != yysplit && yyp->yyposn >= yyposn)
              {
                if (yyp->yylrState == yynewLRState && yyp->yypred == yys)
                  {
                    yyaddDeferredAction (yystackp, yyk, yyp, yys0, yyrule);
                    yymarkStackDeleted (yystackp, yyk);
                    YY_DPRINTF ((stderr, "Merging stack %ld into stack %ld.\n",
                                 YY_CAST (long, yyk), YY_CAST (long, yyi)));
                    return yyok;
                  }
                yyp = yyp->yypred;
              }
          }
      yystackp->yytops.yystates[yyk] = yys;
      yyglrShiftDefer (yystackp, yyk, yynewLRState, yyposn, yys0, yyrule);
    }
  return yyok;
}

static YYPTRDIFF_T
yysplitStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      YY_ASSERT (yyk == 0);
      yystackp->yysplitPoint = yystackp->yytops.yystates[yyk];
    }
  if (yystackp->yytops.yycapacity <= yystackp->yytops.yysize)
    {
      YYPTRDIFF_T state_size = YYSIZEOF (yystackp->yytops.yystates[0]);
      YYPTRDIFF_T half_max_capacity = YYSIZE_MAXIMUM / 2 / state_size;
      if (half_max_capacity < yystackp->yytops.yycapacity)
        yyMemoryExhausted (yystackp);
      yystackp->yytops.yycapacity *= 2;

      {
        yyGLRState** yynewStates
          = YY_CAST (yyGLRState**,
                     YYREALLOC (yystackp->yytops.yystates,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewStates[0])));
        if (yynewStates == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yystates = yynewStates;
      }

      {
        yybool* yynewLookaheadNeeds
          = YY_CAST (yybool*,
                     YYREALLOC (yystackp->yytops.yylookaheadNeeds,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewLookaheadNeeds[0])));
        if (yynewLookaheadNeeds == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yylookaheadNeeds = yynewLookaheadNeeds;
      }
    }
  yystackp->yytops.yystates[yystackp->yytops.yysize]
    = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yylookaheadNeeds[yystackp->yytops.yysize]
    = yystackp->yytops.yylookaheadNeeds[yyk];
  yystackp->yytops.yysize += 1;
  return yystackp->yytops.yysize - 1;
}

/** True iff YYY0 and YYY1 represent identical options at the top level.
 *  That is, they represent the same rule applied to RHS symbols
 *  that produce the same terminal symbols.  */
static yybool
yyidenticalOptions (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  if (yyy0->yyrule == yyy1->yyrule)
    {
      yyGLRState *yys0, *yys1;
      int yyn;
      for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
           yyn = yyrhsLength (yyy0->yyrule);
           yyn > 0;
           yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
        if (yys0->yyposn != yys1->yyposn)
          return yyfalse;
      return yytrue;
    }
  else
    return yyfalse;
}

/** Assuming identicalOptions (YYY0,YYY1), destructively merge the
 *  alternative semantic values for the RHS-symbols of YYY1 and YYY0.  */
static void
yymergeOptionSets (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  yyGLRState *yys0, *yys1;
  int yyn;
  for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
       yyn = yyrhsLength (yyy0->yyrule);
       0 < yyn;
       yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
    {
      if (yys0 == yys1)
        break;
      else if (yys0->yyresolved)
        {
          yys1->yyresolved = yytrue;
          yys1->yysemantics.yysval = yys0->yysemantics.yysval;
        }
      else if (yys1->yyresolved)
        {
          yys0->yyresolved = yytrue;
          yys0->yysemantics.yysval = yys1->yysemantics.yysval;
        }
      else
        {
          yySemanticOption** yyz0p = &yys0->yysemantics.yyfirstVal;
          yySemanticOption* yyz1 = yys1->yysemantics.yyfirstVal;
          while (yytrue)
            {
              if (yyz1 == *yyz0p || yyz1 == YY_NULLPTR)
                break;
              else if (*yyz0p == YY_NULLPTR)
                {
                  *yyz0p = yyz1;
                  break;
                }
              else if (*yyz0p < yyz1)
                {
                  yySemanticOption* yyz = *yyz0p;
                  *yyz0p = yyz1;
                  yyz1 = yyz1->yynext;
                  (*yyz0p)->yynext = yyz;
                }
              yyz0p = &(*yyz0p)->yynext;
            }
          yys1->yysemantics.yyfirstVal = yys0->yysemantics.yyfirstVal;
        }
    }
}

/** Y0 and Y1 represent two possible actions to take in a given
 *  parsing state; return 0 if no combination is possible,
 *  1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.  */
static int
yypreference (yySemanticOption* y0, yySemanticOption* y1)
{
  yyRuleNum r0 = y0->yyrule, r1 = y1->yyrule;
  int p0 = yydprec[r0], p1 = yydprec[r1];

  if (p0 == p1)
    {
      if (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1])
        return 0;
      else
        return 1;
    }
  if (p0 == 0 || p1 == 0)
    return 0;
  if (p0 < p1)
    return 3;
  if (p1 < p0)
    return 2;
  return 0;
}

static YYRESULTTAG yyresolveValue (yyGLRState* yys,
                                   yyGLRStack* yystackp, struct context *context);


/** Resolve the previous YYN states starting at and including state YYS
 *  on *YYSTACKP. If result != yyok, some states may have been left
 *  unresolved possibly with empty semantic option chains.  Regardless
 *  of whether result = yyok, each state has been left with consistent
 *  data so that yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveStates (yyGLRState* yys, int yyn,
                 yyGLRStack* yystackp, struct context *context)
{
  if (0 < yyn)
    {
      YY_ASSERT (yys->yypred);
      YYCHK (yyresolveStates (yys->yypred, yyn-1, yystackp, context));
      if (! yys->yyresolved)
        YYCHK (yyresolveValue (yys, yystackp, context));
    }
  return yyok;
}

/** Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its
 *  user action, and return the semantic value and location in *YYVALP
 *  and *YYLOCP.  Regardless of whether result = yyok, all RHS states
 *  have been destroyed (assuming the user action destroys all RHS
 *  semantic values if invoked).  */
static YYRESULTTAG
yyresolveAction (yySemanticOption* yyopt, yyGLRStack* yystackp,
                 YYSTYPE* yyvalp, struct context *context)
{
  yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
  int yynrhs = yyrhsLength (yyopt->yyrule);
  YYRESULTTAG yyflag =
    yyresolveStates (yyopt->yystate, yynrhs, yystackp, context);
  if (yyflag != yyok)
    {
      yyGLRState *yys;
      for (yys = yyopt->yystate; yynrhs > 0; yys = yys->yypred, yynrhs -= 1)
        yydestroyGLRState ("Cleanup: popping", yys, context);
      return yyflag;
    }

  yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred = yyopt->yystate;
  {
    int yychar_current = yychar;
    YYSTYPE yylval_current = yylval;
    yychar = yyopt->yyrawchar;
    yylval = yyopt->yyval;
    yyflag = yyuserAction (yyopt->yyrule, yynrhs,
                           yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyvalp, context);
    yychar = yychar_current;
    yylval = yylval_current;
  }
  return yyflag;
}

#if YYDEBUG
static void
yyreportTree (yySemanticOption* yyx, int yyindent)
{
  int yynrhs = yyrhsLength (yyx->yyrule);
  int yyi;
  yyGLRState* yys;
  yyGLRState* yystates[1 + YYMAXRHS];
  yyGLRState yyleftmost_state;

  for (yyi = yynrhs, yys = yyx->yystate; 0 < yyi; yyi -= 1, yys = yys->yypred)
    yystates[yyi] = yys;
  if (yys == YY_NULLPTR)
    {
      yyleftmost_state.yyposn = 0;
      yystates[0] = &yyleftmost_state;
    }
  else
    yystates[0] = yys;

  if (yyx->yystate->yyposn < yys->yyposn + 1)
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, empty>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1));
  else
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, tokens %ld .. %ld>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1, YY_CAST (long, yys->yyposn + 1),
                 YY_CAST (long, yyx->yystate->yyposn)));
  for (yyi = 1; yyi <= yynrhs; yyi += 1)
    {
      if (yystates[yyi]->yyresolved)
        {
          if (yystates[yyi-1]->yyposn+1 > yystates[yyi]->yyposn)
            YY_FPRINTF ((stderr, "%*s%s <empty>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState))));
          else
            YY_FPRINTF ((stderr, "%*s%s <tokens %ld .. %ld>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState)),
                         YY_CAST (long, yystates[yyi-1]->yyposn + 1),
                         YY_CAST (long, yystates[yyi]->yyposn)));
        }
      else
        yyreportTree (yystates[yyi]->yysemantics.yyfirstVal, yyindent+2);
    }
}
#endif

static YYRESULTTAG
yyreportAmbiguity (yySemanticOption* yyx0,
                   yySemanticOption* yyx1, struct context *context)
{
  YY_USE (yyx0);
  YY_USE (yyx1);

#if YYDEBUG
  YY_FPRINTF ((stderr, "Ambiguity detected.\n"));
  YY_FPRINTF ((stderr, "Option 1,\n"));
  yyreportTree (yyx0, 2);
  YY_FPRINTF ((stderr, "\nOption 2,\n"));
  yyreportTree (yyx1, 2);
  YY_FPRINTF ((stderr, "\n"));
#endif

  yyerror (context, YY_("syntax is ambiguous"));
  return yyabort;
}

/** Resolve the ambiguity represented in state YYS in *YYSTACKP,
 *  perform the indicated actions, and set the semantic value of YYS.
 *  If result != yyok, the chain of semantic options in YYS has been
 *  cleared instead or it has been left unmodified except that
 *  redundant options may have been removed.  Regardless of whether
 *  result = yyok, YYS has been left with consistent data so that
 *  yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp, struct context *context)
{
  yySemanticOption* yyoptionList = yys->yysemantics.yyfirstVal;
  yySemanticOption* yybest = yyoptionList;
  yySemanticOption** yypp;
  yybool yymerge = yyfalse;
  YYSTYPE yysval;
  YYRESULTTAG yyflag;

  for (yypp = &yyoptionList->yynext; *yypp != YY_NULLPTR; )
    {
      yySemanticOption* yyp = *yypp;

      if (yyidenticalOptions (yybest, yyp))
        {
          yymergeOptionSets (yybest, yyp);
          *yypp = yyp->yynext;
        }
      else
        {
          switch (yypreference (yybest, yyp))
            {
            case 0:
              return yyreportAmbiguity (yybest, yyp, context);
              break;
            case 1:
              yymerge = yytrue;
              break;
            case 2:
              break;
            case 3:
              yybest = yyp;
              yymerge = yyfalse;
              break;
            default:
              /* This cannot happen so it is not worth a YY_ASSERT (yyfalse),
                 but some compilers complain if the default case is
                 omitted.  */
              break;
            }
          yypp = &yyp->yynext;
        }
    }

  if (yymerge)
    {
      yySemanticOption* yyp;
      int yyprec = yydprec[yybest->yyrule];
      yyflag = yyresolveAction (yybest, yystackp, &yysval, context);
      if (yyflag == yyok)
        for (yyp = yybest->yynext; yyp != YY_NULLPTR; yyp = yyp->yynext)
          {
            if (yyprec == yydprec[yyp->yyrule])
              {
                YYSTYPE yysval_other;
                yyflag = yyresolveAction (yyp, yystackp, &yysval_other, context);
                if (yyflag != yyok)
                  {
                    yydestruct ("Cleanup: discarding incompletely merged value for",
                                yy_accessing_symbol (yys->yylrState),
                                &yysval, context);
                    break;
                  }
                yyuserMerge (yymerger[yyp->yyrule], &yysval, &yysval_other);
              }
          }
    }
  else
    yyflag = yyresolveAction (yybest, yystackp, &yysval, context);

  if (yyflag == yyok)
    {
      yys->yyresolved = yytrue;
      yys->yysemantics.yysval = yysval;
    }
  else
    yys->yysemantics.yyfirstVal = YY_NULLPTR;
  return yyflag;
}

static YYRESULTTAG
yyresolveStack (yyGLRStack* yystackp, struct context *context)
{
  if (yystackp->yysplitPoint != YY_NULLPTR)
    {
      yyGLRState* yys;
      int yyn;

      for (yyn = 0, yys = yystackp->yytops.yystates[0];
           yys != yystackp->yysplitPoint;
           yys = yys->yypred, yyn += 1)
        continue;
      YYCHK (yyresolveStates (yystackp->yytops.yystates[0], yyn, yystackp
                             , context));
    }
  return yyok;
}

static void
yycompressStack (yyGLRStack* yystackp)
{
  yyGLRState* yyp, *yyq, *yyr;

  if (yystackp->yytops.yysize != 1 || yystackp->yysplitPoint == YY_NULLPTR)
    return;

  for (yyp = yystackp->yytops.yystates[0], yyq = yyp->yypred, yyr = YY_NULLPTR;
       yyp != yystackp->yysplitPoint;
       yyr = yyp, yyp = yyq, yyq = yyp->yypred)
    yyp->yypred = yyr;

  yystackp->yyspaceLeft += yystackp->yynextFree - yystackp->yyitems;
  yystackp->yynextFree = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yysplitPoint) + 1;
  yystackp->yyspaceLeft -= yystackp->yynextFree - yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;

  while (yyr != YY_NULLPTR)
    {
      yystackp->yynextFree->yystate = *yyr;
      yyr = yyr->yypred;
      yystackp->yynextFree->yystate.yypred = &yystackp->yynextFree[-1].yystate;
      yystackp->yytops.yystates[0] = &yystackp->yynextFree->yystate;
      yystackp->yynextFree += 1;
      yystackp->yyspaceLeft -= 1;
    }
}

static YYRESULTTAG
yyprocessOneStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk,
                   YYPTRDIFF_T yyposn, struct context *context)
{
  while (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    {
      yy_state_t yystate = yystackp->yytops.yystates[yyk]->yylrState;
      YY_DPRINTF ((stderr, "Stack %ld Entering state %d\n",
                   YY_CAST (long, yyk), yystate));

      YY_ASSERT (yystate != YYFINAL);

      if (yyisDefaultedState (yystate))
        {
          YYRESULTTAG yyflag;
          yyRuleNum yyrule = yydefaultAction (yystate);
          if (yyrule == 0)
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          yyflag = yyglrReduce (yystackp, yyk, yyrule, yyimmediate[yyrule], context);
          if (yyflag == yyerr)
            {
              YY_DPRINTF ((stderr,
                           "Stack %ld dies "
                           "(predicate failure or explicit user error).\n",
                           YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          if (yyflag != yyok)
            return yyflag;
        }
      else
        {
          yysymbol_kind_t yytoken = yygetToken (&yychar, yystackp, context);
          const short* yyconflicts;
          const int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
          yystackp->yytops.yylookaheadNeeds[yyk] = yytrue;

          for (/* nothing */; *yyconflicts; yyconflicts += 1)
            {
              YYRESULTTAG yyflag;
              YYPTRDIFF_T yynewStack = yysplitStack (yystackp, yyk);
              YY_DPRINTF ((stderr, "Splitting off stack %ld from %ld.\n",
                           YY_CAST (long, yynewStack), YY_CAST (long, yyk)));
              yyflag = yyglrReduce (yystackp, yynewStack,
                                    *yyconflicts,
                                    yyimmediate[*yyconflicts], context);
              if (yyflag == yyok)
                YYCHK (yyprocessOneStack (yystackp, yynewStack,
                                          yyposn, context));
              else if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yynewStack)));
                  yymarkStackDeleted (yystackp, yynewStack);
                }
              else
                return yyflag;
            }

          if (yyisShiftAction (yyaction))
            break;
          else if (yyisErrorAction (yyaction))
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              break;
            }
          else
            {
              YYRESULTTAG yyflag = yyglrReduce (yystackp, yyk, -yyaction,
                                                yyimmediate[-yyaction], context);
              if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr,
                               "Stack %ld dies "
                               "(predicate failure or explicit user error).\n",
                               YY_CAST (long, yyk)));
                  yymarkStackDeleted (yystackp, yyk);
                  break;
                }
              else if (yyflag != yyok)
                return yyflag;
            }
        }
    }
  return yyok;
}






static void
yyreportSyntaxError (yyGLRStack* yystackp, struct context *context)
{
  if (yystackp->yyerrState != 0)
    return;
  yyerror (context, YY_("syntax error"));
  yynerrs += 1;
}

/* Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP,
   yylval, and yylloc are the syntactic category, semantic value, and location
   of the lookahead.  */
static void
yyrecoverSyntaxError (yyGLRStack* yystackp, struct context *context)
{
  if (yystackp->yyerrState == 3)
    /* We just shifted the error token and (perhaps) took some
       reductions.  Skip tokens until we can proceed.  */
    while (yytrue)
      {
        yysymbol_kind_t yytoken;
        int yyj;
        if (yychar == YYEOF)
          yyFail (yystackp, context, YY_NULLPTR);
        if (yychar != YYEMPTY)
          {
            yytoken = YYTRANSLATE (yychar);
            yydestruct ("Error: discarding",
                        yytoken, &yylval, context);
            yychar = YYEMPTY;
          }
        yytoken = yygetToken (&yychar, yystackp, context);
        yyj = yypact[yystackp->yytops.yystates[0]->yylrState];
        if (yypact_value_is_default (yyj))
          return;
        yyj += yytoken;
        if (yyj < 0 || YYLAST < yyj || yycheck[yyj] != yytoken)
          {
            if (yydefact[yystackp->yytops.yystates[0]->yylrState] != 0)
              return;
          }
        else if (! yytable_value_is_error (yytable[yyj]))
          return;
      }

  /* Reduce to one stack.  */
  {
    YYPTRDIFF_T yyk;
    for (yyk = 0; yyk < yystackp->yytops.yysize; yyk += 1)
      if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
        break;
    if (yyk >= yystackp->yytops.yysize)
      yyFail (yystackp, context, YY_NULLPTR);
    for (yyk += 1; yyk < yystackp->yytops.yysize; yyk += 1)
      yymarkStackDeleted (yystackp, yyk);
    yyremoveDeletes (yystackp);
    yycompressStack (yystackp);
  }

  /* Pop stack until we find a state that shifts the error token.  */
  yystackp->yyerrState = 3;
  while (yystackp->yytops.yystates[0] != YY_NULLPTR)
    {
      yyGLRState *yys = yystackp->yytops.yystates[0];
      int yyj = yypact[yys->yylrState];
      if (! yypact_value_is_default (yyj))
        {
          yyj += YYSYMBOL_YYerror;
          if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYSYMBOL_YYerror
              && yyisShiftAction (yytable[yyj]))
            {
              /* Shift the error token.  */
              int yyaction = yytable[yyj];
              YY_SYMBOL_PRINT ("Shifting", yy_accessing_symbol (yyaction),
                               &yylval, &yyerrloc);
              yyglrShift (yystackp, 0, yyaction,
                          yys->yyposn, &yylval);
              yys = yystackp->yytops.yystates[0];
              break;
            }
        }
      if (yys->yypred != YY_NULLPTR)
        yydestroyGLRState ("Error: popping", yys, context);
      yystackp->yytops.yystates[0] = yys->yypred;
      yystackp->yynextFree -= 1;
      yystackp->yyspaceLeft += 1;
    }
  if (yystackp->yytops.yystates[0] == YY_NULLPTR)
    yyFail (yystackp, context, YY_NULLPTR);
}

#define YYCHK1(YYE)                                                          \
  do {                                                                       \
    switch (YYE) {                                                           \
    case yyok:                                                               \
      break;                                                                 \
    case yyabort:                                                            \
      goto yyabortlab;                                                       \
    case yyaccept:                                                           \
      goto yyacceptlab;                                                      \
    case yyerr:                                                              \
      goto yyuser_error;                                                     \
    default:                                                                 \
      goto yybuglab;                                                         \
    }                                                                        \
  } while (0)

/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct context *context)
{
  int yyresult;
  yyGLRStack yystack;
  yyGLRStack* const yystackp = &yystack;
  YYPTRDIFF_T yyposn;

  YY_DPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;
  yylval = yyval_default;

  if (! yyinitGLRStack (yystackp, YYINITDEPTH))
    goto yyexhaustedlab;
  switch (YYSETJMP (yystack.yyexception_buffer))
    {
    case 0: break;
    case 1: goto yyabortlab;
    case 2: goto yyexhaustedlab;
    default: goto yybuglab;
    }
  yyglrShift (&yystack, 0, 0, 0, &yylval);
  yyposn = 0;

  while (yytrue)
    {
      /* For efficiency, we have two loops, the first of which is
         specialized to deterministic operation (single stack, no
         potential ambiguity).  */
      /* Standard mode. */
      while (yytrue)
        {
          yy_state_t yystate = yystack.yytops.yystates[0]->yylrState;
          YY_DPRINTF ((stderr, "Entering state %d\n", yystate));
          if (yystate == YYFINAL)
            goto yyacceptlab;
          if (yyisDefaultedState (yystate))
            {
              yyRuleNum yyrule = yydefaultAction (yystate);
              if (yyrule == 0)
                {
                  yyreportSyntaxError (&yystack, context);
                  goto yyuser_error;
                }
              YYCHK1 (yyglrReduce (&yystack, 0, yyrule, yytrue, context));
            }
          else
            {
              yysymbol_kind_t yytoken = yygetToken (&yychar, yystackp, context);
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
              if (*yyconflicts)
                /* Enter nondeterministic mode.  */
                break;
              if (yyisShiftAction (yyaction))
                {
                  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
                  yychar = YYEMPTY;
                  yyposn += 1;
                  yyglrShift (&yystack, 0, yyaction, yyposn, &yylval);
                  if (0 < yystack.yyerrState)
                    yystack.yyerrState -= 1;
                }
              else if (yyisErrorAction (yyaction))
                {
                  /* Issue an error message unless the scanner already
                     did. */
                  if (yychar != YYerror)
                    yyreportSyntaxError (&yystack, context);
                  goto yyuser_error;
                }
              else
                YYCHK1 (yyglrReduce (&yystack, 0, -yyaction, yytrue, context));
            }
        }

      /* Nondeterministic mode. */
      while (yytrue)
        {
          yysymbol_kind_t yytoken_to_shift;
          YYPTRDIFF_T yys;

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            yystackp->yytops.yylookaheadNeeds[yys] = yychar != YYEMPTY;

          /* yyprocessOneStack returns one of three things:

              - An error flag.  If the caller is yyprocessOneStack, it
                immediately returns as well.  When the caller is finally
                yyparse, it jumps to an error label via YYCHK1.

              - yyok, but yyprocessOneStack has invoked yymarkStackDeleted
                (&yystack, yys), which sets the top state of yys to NULL.  Thus,
                yyparse's following invocation of yyremoveDeletes will remove
                the stack.

              - yyok, when ready to shift a token.

             Except in the first case, yyparse will invoke yyremoveDeletes and
             then shift the next token onto all remaining stacks.  This
             synchronization of the shift (that is, after all preceding
             reductions on all stacks) helps prevent double destructor calls
             on yylval in the event of memory exhaustion.  */

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            YYCHK1 (yyprocessOneStack (&yystack, yys, yyposn, context));
          yyremoveDeletes (&yystack);
          if (yystack.yytops.yysize == 0)
            {
              yyundeleteLastStack (&yystack);
              if (yystack.yytops.yysize == 0)
                yyFail (&yystack, context, YY_("syntax error"));
              YYCHK1 (yyresolveStack (&yystack, context));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yyreportSyntaxError (&yystack, context);
              goto yyuser_error;
            }

          /* If any yyglrShift call fails, it will fail after shifting.  Thus,
             a copy of yylval will already be on stack 0 in the event of a
             failure in the following loop.  Thus, yychar is set to YYEMPTY
             before the loop to make sure the user destructor for yylval isn't
             called twice.  */
          yytoken_to_shift = YYTRANSLATE (yychar);
          yychar = YYEMPTY;
          yyposn += 1;
          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            {
              yy_state_t yystate = yystack.yytops.yystates[yys]->yylrState;
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken_to_shift,
                              &yyconflicts);
              /* Note that yyconflicts were handled by yyprocessOneStack.  */
              YY_DPRINTF ((stderr, "On stack %ld, ", YY_CAST (long, yys)));
              YY_SYMBOL_PRINT ("shifting", yytoken_to_shift, &yylval, &yylloc);
              yyglrShift (&yystack, yys, yyaction, yyposn,
                          &yylval);
              YY_DPRINTF ((stderr, "Stack %ld now in state #%d\n",
                           YY_CAST (long, yys),
                           yystack.yytops.yystates[yys]->yylrState));
            }

          if (yystack.yytops.yysize == 1)
            {
              YYCHK1 (yyresolveStack (&yystack, context));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yycompressStack (&yystack);
              break;
            }
        }
      continue;
    yyuser_error:
      yyrecoverSyntaxError (&yystack, context);
      yyposn = yystack.yytops.yystates[0]->yyposn;
    }

 yyacceptlab:
  yyresult = 0;
  goto yyreturn;

 yybuglab:
  YY_ASSERT (yyfalse);
  goto yyabortlab;

 yyabortlab:
  yyresult = 1;
  goto yyreturn;

 yyexhaustedlab:
  yyerror (context, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;

 yyreturn:
  if (yychar != YYEMPTY)
    yydestruct ("Cleanup: discarding lookahead",
                YYTRANSLATE (yychar), &yylval, context);

  /* If the stack is well-formed, pop the stack until it is empty,
     destroying its entries as we go.  But free the stack regardless
     of whether it is well-formed.  */
  if (yystack.yyitems)
    {
      yyGLRState** yystates = yystack.yytops.yystates;
      if (yystates)
        {
          YYPTRDIFF_T yysize = yystack.yytops.yysize;
          YYPTRDIFF_T yyk;
          for (yyk = 0; yyk < yysize; yyk += 1)
            if (yystates[yyk])
              {
                while (yystates[yyk])
                  {
                    yyGLRState *yys = yystates[yyk];
                    if (yys->yypred != YY_NULLPTR)
                      yydestroyGLRState ("Cleanup: popping", yys, context);
                    yystates[yyk] = yys->yypred;
                    yystack.yynextFree -= 1;
                    yystack.yyspaceLeft += 1;
                  }
                break;
              }
        }
      yyfreeGLRStack (&yystack);
    }

  return yyresult;
}

/* DEBUGGING ONLY */
#if YYDEBUG
static void
yy_yypstack (yyGLRState* yys)
{
  if (yys->yypred)
    {
      yy_yypstack (yys->yypred);
      YY_FPRINTF ((stderr, " -> "));
    }
  YY_FPRINTF ((stderr, "%d@%ld", yys->yylrState, YY_CAST (long, yys->yyposn)));
}

static void
yypstates (yyGLRState* yyst)
{
  if (yyst == YY_NULLPTR)
    YY_FPRINTF ((stderr, "<null>"));
  else
    yy_yypstack (yyst);
  YY_FPRINTF ((stderr, "\n"));
}

static void
yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  yypstates (yystackp->yytops.yystates[yyk]);
}

static void
yypdumpstack (yyGLRStack* yystackp)
{
#define YYINDEX(YYX)                                                    \
  YY_CAST (long,                                                        \
           ((YYX)                                                       \
            ? YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX)) - yystackp->yyitems \
            : -1))

  yyGLRStackItem* yyp;
  for (yyp = yystackp->yyitems; yyp < yystackp->yynextFree; yyp += 1)
    {
      YY_FPRINTF ((stderr, "%3ld. ",
                   YY_CAST (long, yyp - yystackp->yyitems)));
      if (*YY_REINTERPRET_CAST (yybool *, yyp))
        {
          YY_ASSERT (yyp->yystate.yyisState);
          YY_ASSERT (yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Res: %d, LR State: %d, posn: %ld, pred: %ld",
                       yyp->yystate.yyresolved, yyp->yystate.yylrState,
                       YY_CAST (long, yyp->yystate.yyposn),
                       YYINDEX (yyp->yystate.yypred)));
          if (! yyp->yystate.yyresolved)
            YY_FPRINTF ((stderr, ", firstVal: %ld",
                         YYINDEX (yyp->yystate.yysemantics.yyfirstVal)));
        }
      else
        {
          YY_ASSERT (!yyp->yystate.yyisState);
          YY_ASSERT (!yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Option. rule: %d, state: %ld, next: %ld",
                       yyp->yyoption.yyrule - 1,
                       YYINDEX (yyp->yyoption.yystate),
                       YYINDEX (yyp->yyoption.yynext)));
        }
      YY_FPRINTF ((stderr, "\n"));
    }

  YY_FPRINTF ((stderr, "Tops:"));
  {
    YYPTRDIFF_T yyi;
    for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
      YY_FPRINTF ((stderr, "%ld: %ld; ", YY_CAST (long, yyi),
                   YYINDEX (yystackp->yytops.yystates[yyi])));
    YY_FPRINTF ((stderr, "\n"));
  }
#undef YYINDEX
}
#endif

#undef yylval
#undef yychar
#undef yynerrs




