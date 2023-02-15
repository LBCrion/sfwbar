#ifndef __EXPR_H__
#define __EXPR_H__

#include <glib.h>

typedef struct expr_cache {
  gchar *definition;
  gchar *cache;
  gboolean eval;
  guint vcount;
} ExprCache;

char *expr_parse ( gchar *expr_str, guint * );
gboolean expr_cache_eval ( ExprCache *expr );
void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name );
gchar *expr_dtostr ( double num, gint dec );
void expr_lib_init ( void );
ExprCache *expr_cache_new ( void );
void expr_cache_free ( ExprCache *expr );

#endif
