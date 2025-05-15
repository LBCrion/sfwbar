#ifndef __EXPRN_H__
#define __EXPRN_H__

#include <gtk/gtk.h>
#include "wintree.h"
#include "util/datalist.h"
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
  EXPR_OP_LOCAL_ASSIGN,
  EXPR_OP_HEAP,
  EXPR_OP_HEAP_ASSIGN,
  EXPR_OP_RETURN
};

enum vm_func_flags_t {
  VM_FUNC_DETERMINISTIC = 1,
  VM_FUNC_USERDEFINED = 2,
};

typedef struct _vm_store_t vm_store_t;
struct _vm_store_t {
  datalist_t *vars;
  GHashTable *widget_map;
  gboolean transient;
  vm_store_t *parent;
};

typedef struct _vm_var {
  value_t value;
  gchar *name;
  GQuark quark;
} vm_var_t;

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
  vm_store_t *store;
  vm_store_t *globals;
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

typedef struct {
  GBytes *code;
  vm_store_t *store;
} vm_closure_t;

#define vm_param_check_np(vm, np, n, fname) { if(np!=n) { return value_na; } }
#define vm_param_check_np_range(vm, np, min, max, fname) { if(np<min || np>max) { return value_na; } }
#define vm_param_check_string(vm, p, n, fname) { if(!value_like_string(p[n])) { return value_na; } }
#define vm_param_check_numeric(vm, p, n, fname) { if(!value_like_numeric(p[n])) { return value_na; } }
#define vm_param_check_array(vm, p, n, fname) { if(!value_is_array(p[n])) { return value_na; } }

void parser_init ( void );
GBytes *parser_expr_compile ( gchar *expr );
gboolean parser_block_parse ( GScanner *scanner, GByteArray * );
gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code );
gboolean parser_macro_add ( GScanner *scanner );
gboolean parser_function_parse( GScanner *scanner );
const gchar *parser_identifier_lookup ( gchar *identifier );
GBytes *parser_exec_build ( gchar *cmd );
GBytes *parser_string_build ( gchar *str );

value_t vm_code_eval ( GBytes *code, GtkWidget *widget );
value_t vm_expr_eval ( expr_cache_t *expr );
value_t vm_function_call ( vm_t *vm, GBytes *code, guint8 np );
void vm_run_action ( GBytes *code, GtkWidget *w, GdkEvent *e, window_t *win,
    guint16 *s, vm_store_t *store);
void vm_run_user_defined ( gchar *action, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state, vm_store_t *store );

void vm_func_init ( void );
void vm_func_add ( gchar *name, vm_func_t func, gboolean deterministic );
void vm_func_add_user ( gchar *name, GBytes *code );
vm_function_t *vm_func_lookup ( gchar *name );
void vm_func_remove ( gchar *name );

vm_var_t *vm_var_new ( gchar *name );
void vm_var_free ( vm_var_t *var );
vm_store_t *vm_store_new ( vm_store_t *parent, gboolean transient );
vm_store_t *vm_store_dup ( vm_store_t *src );
void vm_store_free ( vm_store_t *store );
vm_var_t *vm_store_lookup ( vm_store_t *store, GQuark id );
vm_var_t *vm_store_lookup_string ( vm_store_t *store, gchar *string );
gboolean vm_store_insert ( vm_store_t *store, vm_var_t *var );
gboolean vm_store_insert_full ( vm_store_t *store, gchar *name, value_t v );
vm_closure_t *vm_closure_new ( GBytes *code, vm_store_t *store );
void vm_closure_free ( vm_closure_t *closure );

#endif
