#ifndef __EXPR_H__
#define __EXPR_H__

#include <gtk/gtk.h>

typedef struct _expr_cache {
  gchar *definition;
  gchar *cache;
  GBytes *code;
  GtkWidget *widget;
  GdkEvent *event;
  gboolean eval;
  gint stack_depth;
  guint vstate;
  struct _expr_cache *parent;
  void *store;
} expr_cache_t;

typedef struct _expr_dep {
  GList *list;
  GMutex mutex;
} expr_dep_t;

void expr_lib_init ( void );
expr_cache_t *expr_cache_new ( void );
expr_cache_t *expr_cache_new_with_code ( GBytes *code );
void expr_update ( expr_cache_t **expr, GBytes *code );
void expr_cache_set ( expr_cache_t *expr, gchar *def );
void expr_cache_free ( expr_cache_t *expr );
void expr_dep_add ( GQuark quark, expr_cache_t *expr );
void expr_dep_remove ( expr_cache_t *expr );
void expr_dep_trigger ( GQuark quark );
void expr_dep_dump ( void );

#endif
