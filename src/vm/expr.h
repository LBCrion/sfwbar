#ifndef __EXPR_H__
#define __EXPR_H__

#include <gtk/gtk.h>

typedef struct expr_cache {
  gchar *definition;
  gchar *cache;
  guint8 *code;
  gsize len;
  GtkWidget *widget;
  GdkEvent *event;
  gboolean eval;
  gint stack_depth;
  guint vstate;
  struct expr_cache *parent;
} expr_cache_t;

gboolean expr_cache_eval ( expr_cache_t *expr );
void expr_lib_init ( void );
expr_cache_t *expr_cache_new ( void );
void expr_cache_set ( expr_cache_t *expr, gchar *def );
void expr_cache_free ( expr_cache_t *expr );
void expr_dep_add ( gchar *ident, expr_cache_t *expr );
void expr_dep_remove ( expr_cache_t *expr );
void expr_dep_trigger ( gchar *ident );
void expr_dep_dump ( void );

#endif
