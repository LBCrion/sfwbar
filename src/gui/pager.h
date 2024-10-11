#ifndef __PAGER_H__
#define __PAGER_H__

#include "flowgrid.h"
#include "workspace.h"

#define PAGER_TYPE            (pager_get_type())
#define PAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PAGER_TYPE, Pager))
#define PAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PAGER_TYPE, PagerClass))
#define IS_PAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PAGER_TYPE))
#define IS_PAGERCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PAGER_TYPE))

typedef struct _Pager Pager;
typedef struct _PagerClass PagerClass;
typedef struct _PagerPrivate PagerPrivate;

struct _Pager
{
  BaseWidget item;
};

struct _PagerClass
{
  BaseWidgetClass parent_class;
};

struct _PagerPrivate
{
  GList *pins;
};

GType pager_get_type ( void );

void pager_invalidate_all ( workspace_t *ws );
void pager_add_pins ( GtkWidget *self, GList *pins );
gboolean pager_check_pins ( GtkWidget *self, gchar *pin );
void pager_item_add ( workspace_t *ws );
void pager_item_delete ( workspace_t *ws );
void pager_update_all ( void );

#endif
