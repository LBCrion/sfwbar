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

struct _PagerItemPrivate
{
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *pager;
  gboolean visible;
  gboolean focused;
};

GType pager_item_get_type ( void );

GtkWidget *pager_item_new( gchar * );
gchar *pager_item_get_label ( GtkWidget *self );
void pager_item_update ( GtkWidget *self );
void pager_item_set_visible ( GtkWidget *, gboolean );
void pager_item_set_focused ( GtkWidget *, gboolean );

#endif
