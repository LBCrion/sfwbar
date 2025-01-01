#ifndef __EXPRN_H__
#define __EXPRN_H__

#include <gtk/gtk.h>
#include "wintree.h"
#include "vm/value.h"
#include "vm/expr.h"

enum expr_instruction_t {
  EXPR_OP_IMMEDIATE,
  EXPR_OP_JZ,
  EXPR_OP_JMP,
  EXPR_OP_CACHED,
  EXPR_OP_VARIABLE,
  EXPR_OP_FUNCTION,
  EXPR_OP_DISCARD,
  EXPR_OP_LOCAL,
  EXPR_OP_ASSIGN,
  EXPR_OP_RETURN
};

enum vm_func_flags_t {
  VM_FUNC_DETERMINISTIC,
  VM_FUNC_USERDEFINED
};

typedef struct {
  guint8 *ip;
  guint8 *code;
  gsize len;
  gsize fp;
  GArray *stack;
  GPtrArray *pstack;
  gint max_stack;
  gboolean use_cached;
  guint16 wstate;
  GtkWidget *widget;
  GdkEvent *event;
  window_t *win;
  expr_cache_t *expr;
} vm_t;

typedef value_t (*vm_func_t)(vm_t *vm, value_t params[], gint np);

typedef struct {
  gchar *name;
  guint8 flags;
  union {
    vm_func_t function;
    GBytes *code;
  } ptr;
} vm_function_t;

#define vm_param_check_np(vm, np, n, fname) { if(np!=n) { return value_na; } }
#define vm_param_check_np_range(vm, np, min, max, fname) { if(np<min || np>max) { return value_na; } }
#define vm_param_check_string(vm, p, n, fname) { if(!value_like_string(p[n])) { return value_na; } }
#define vm_param_check_numeric(vm, p, n, fname) { if(!value_like_numeric(p[n])) { return value_na; } }
#define vm_param_check_array(vm, p, n, fname) { if(!value_is_array(p[n])) { return value_na; } }

GBytes *parser_expr_compile ( gchar *expr );
gboolean parser_block_parse ( GScanner *scanner, GByteArray *, gboolean vars );
gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code );
gboolean parser_macro_add ( GScanner *scanner );
gboolean parser_function_parse( GScanner *scanner );
const gchar *parser_identifier_lookup ( gchar *identifier );

value_t vm_expr_eval ( expr_cache_t *expr );
value_t vm_function_call ( vm_t *vm, GBytes *code, guint8 np );
void vm_run_action ( GBytes *code, GtkWidget *w, GdkEvent *e, window_t *win,
    guint16 *s);
void vm_run_user_defined ( gchar *action, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state );

void vm_func_init ( void );
void vm_func_add ( gchar *name, vm_func_t func, gboolean deterministic );
void vm_func_add_user ( gchar *name, GBytes *code );
vm_function_t *vm_func_lookup ( gchar *name );
void vm_func_remove ( gchar *name );

#endif
