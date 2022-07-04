#ifndef __PAGERITEM_H__
#define __PAGERITEM_H__

#include "sfwbar.h" 

#define PAGER_ITEM_TYPE            (pager_item_get_type())
#define PAGER_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PAGER_ITEM_TYPE, PagerItem))
#define PAGER_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PAGER_ITEM_TYPE, PagerItemClass))
#define IS_PAGER_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PAGER_ITEM_TYPE))
#define IS_PAGER_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PAGER_ITEM_TYPE))

typedef struct _PagerItem PagerItem;
typedef struct _PagerItemClass PagerItemClass;

struct _PagerItem
{
  FlowItem item;
};

struct _PagerItemClass
{
  FlowItemClass parent_class;
};

typedef struct _PagerItemPrivate PagerItemPrivate;

typedef struct workspace_s {
  gpointer id;
  gchar *name;
  gboolean visible;
  gboolean focused;
  GtkWidget *pager;
} workspace_t;

struct _PagerItemPrivate
{
  GtkWidget *button;
  GtkWidget *pager;
  workspace_t *ws;
};

GType pager_item_get_type ( void );

GtkWidget *pager_item_new( GtkWidget *pager, workspace_t *ws );
workspace_t *pager_item_get_workspace ( GtkWidget *self );
void pager_item_update ( GtkWidget *self );
gint pager_item_compare ( GtkWidget *, GtkWidget *, GtkWidget * );

#endif
