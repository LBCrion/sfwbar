#ifndef __PAGER_H__
#define __PAGER_H__

#include "basewidget.h"
#include "flowgrid.h"
#include "workspace.h"

#define PAGER_TYPE            (pager_get_type())
#define PAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PAGER_TYPE, Pager))
#define PAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PAGER_TYPE, PagerClass))
#define IS_PAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PAGER_TYPE))
#define IS_PAGERCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PAGER_TYPE))

typedef struct _Pager Pager;
typedef struct _PagerClass PagerClass;

struct _Pager
{
  BaseWidget item;
};

struct _PagerClass
{
  BaseWidgetClass parent_class;
};

typedef struct _PagerPrivate PagerPrivate;

struct _PagerPrivate
{
  GtkWidget *pager;
};

GType pager_get_type ( void );


GtkWidget *pager_new();
void pager_invalidate_all ( workspace_t *ws );
void pager_add_pin ( GtkWidget *pager, gchar *pin );
void pager_item_add ( workspace_t *ws );
void pager_item_delete ( workspace_t *ws );
void pager_update_all ( void );
gpointer pager_get_focused ( void );

#endif
