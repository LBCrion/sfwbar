#ifndef __SWITCHERITEM_H__
#define __SWITCHERITEM_H__

#include "sfwbar.h" 
#include "flowitem.h"

#define SWITCHER_ITEM_TYPE            (switcher_item_get_type())
#define SWITCHER_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SWITCHER_ITEM_TYPE, SwitcherItem))
#define SWITCHER_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SWITCHER_ITEM_TYPE, SwitcherItemClass))
#define IS_SWITCHER_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SWITCHER_ITEM_TYPE))
#define IS_SWITCHER_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SWITCHER_ITEM_TYPE))

typedef struct _SwitcherItem SwitcherItem;
typedef struct _SwitcherItemClass SwitcherItemClass;

struct _SwitcherItem
{
  FlowItem item;
};

struct _SwitcherItemClass
{
  FlowItemClass parent_class;
};

typedef struct _SwitcherItemPrivate SwitcherItemPrivate;

struct _SwitcherItemPrivate
{
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *grid;
  GtkWidget *switcher;
  window_t *win;
  action_t **actions;
  gboolean invalid;
};

GType switcher_item_get_type ( void );

GtkWidget *switcher_item_new( window_t *win, GtkWidget *switcher );
window_t *switcher_item_get_window ( GtkWidget *self );
void switcher_item_update ( GtkWidget *self );
gint switcher_item_compare ( GtkWidget *, GtkWidget *, GtkWidget * );
void switcher_item_invalidate ( GtkWidget *self );

#endif
