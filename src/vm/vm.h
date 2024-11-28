#ifndef __EXPRN_H__
#define __EXPRN_H__

#include <glib.h>
#include "vm/expr.h"

enum expr_instruction_t {
  EXPR_OP_IMMEDIATE,
  EXPR_OP_JZ,
  EXPR_OP_JMP,
  EXPR_OP_CACHED,
  EXPR_OP_VARIABLE,
  EXPR_OP_FUNCTION
};

enum expr_type_t {
  EXPR_TYPE_NUMERIC,
  EXPR_TYPE_STRING,
  EXPR_TYPE_BOOLEAN, // Not in use
  EXPR_TYPE_NA
};

typedef struct {
  enum expr_type_t type;
  union {
    gboolean boolean;
    gdouble numeric;
    gchar *string;
  } value;
} value_t;

typedef struct {
  guint8 *ip;
  GArray *stack;
  gint max_stack;
  gboolean use_cached;
  expr_cache_t *cache;
} vm_t;

typedef value_t (*vm_func_t)(vm_t *vm, value_t params[], gint np);

typedef struct {
  vm_func_t function;
  gboolean deterministic;
} vm_function_t;

extern const value_t value_na;

GByteArray *parser_expr_compile ( gchar *expr );
value_t vm_run ( expr_cache_t *cache );
gchar *expr_vm_result_to_string ( vm_t *vm );
gint expr_vm_get_func_params ( vm_t *vm, value_t *params[] );

void vm_func_init ( void );
void vm_func_add ( gchar *name, vm_func_t func, gboolean deterministic );
vm_function_t *vm_func_lookup ( gchar *name );
gboolean vm_func_remove ( gchar *name );

#endif
