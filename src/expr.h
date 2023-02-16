#ifndef __EXPR_H__
#define __EXPR_H__

#include <glib.h>

enum {
  G_TOKEN_IF      = G_TOKEN_LAST + 1,
  G_TOKEN_CACHED,
  G_TOKEN_LOOKUP,
  G_TOKEN_MAP
};

enum {
  EXPR_STRING,
  EXPR_NUMERIC,
  EXPR_VARIANT
};

typedef struct expr_cache {
  gchar *definition;
  gchar *cache;
  gboolean eval;
  guint vstate;
} ExprCache;

typedef struct expr_state {
  gint type;
  guint vstate;
  gboolean error;
  gboolean ignore;
} ExprState;

#define E_STATE(x) ((ExprState *)x->user_data)

char *expr_parse ( gchar *expr_str, guint * );
gboolean expr_cache_eval ( ExprCache *expr );
void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name );
gchar *expr_dtostr ( double num, gint dec );
void expr_lib_init ( void );
ExprCache *expr_cache_new ( void );
void expr_cache_free ( ExprCache *expr );
void expr_dep_add ( gchar *ident, ExprCache *expr );
void expr_dep_trigger ( gchar *ident );

#endif
