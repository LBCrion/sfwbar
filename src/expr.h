#ifndef __EXPR_H__
#define __EXPR_H__

#include <gtk/gtk.h>

enum {
  G_TOKEN_IF      = G_TOKEN_LAST + 1,
  G_TOKEN_CACHED,
  G_TOKEN_LOOKUP,
  G_TOKEN_REPLACEALL,
  G_TOKEN_MAP,
  G_TOKEN_IDENT
};

enum {
  EXPR_STRING,
  EXPR_NUMERIC,
  EXPR_VARIANT
};

typedef struct expr_cache {
  gchar *definition;
  gchar *cache;
  GtkWidget *widget;
  GdkEvent *event;
  gboolean eval;
  guint vstate;
  struct expr_cache *parent;
} ExprCache;

typedef struct expr_state {
  gint type;
  gboolean error;
  gboolean ignore;
  ExprCache *expr;
} ExprState;

#define E_STATE(x) ((ExprState *)x->user_data)

gboolean expr_cache_eval ( ExprCache *expr );
void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name );
gchar *expr_dtostr ( double num, gint dec );
void expr_lib_init ( void );
ExprCache *expr_cache_new ( void );
void expr_cache_free ( ExprCache *expr );
void expr_dep_add ( gchar *ident, ExprCache *expr );
void expr_dep_remove ( ExprCache *expr );
void expr_dep_trigger ( gchar *ident );
void expr_dep_dump ( void );

#endif
