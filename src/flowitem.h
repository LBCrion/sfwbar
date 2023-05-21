#ifndef __FLOWITEM_H__
#define __FLOWITEM_H__

#include <gtk/gtk.h>
#include "basewidget.h"

#define FLOW_ITEM_TYPE            (flow_item_get_type())
#define FLOW_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FLOW_ITEM_TYPE, FlowItemClass))
#define FLOW_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FLOW_ITEM_TYPE, FlowItem))
#define IS_FLOW_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLOW_ITEM_TYPE))
#define IS_FLOW_TEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FLOW_ITEM_TYPE))
#define FLOW_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), FLOW_ITEM_TYPE, FlowItemClass))

typedef struct _FlowItem FlowItem;
typedef struct _FlowItemClass FlowItemClass;

struct _FlowItem
{
  BaseWidget widget;
};

struct _FlowItemClass
{
  BaseWidgetClass parent_class;

  void (*update) ( GtkWidget *self );
  void (*invalidate) ( GtkWidget *self );
  void* (*get_parent) ( GtkWidget *self );
  gint (*compare) (GtkWidget *, GtkWidget *, GtkWidget *);
  GCompareFunc comp_parent;
};

typedef struct _FlowItemPrivate FlowItemPrivate;

struct _FlowItemPrivate
{
  gboolean active;
  GtkWidget *parent;
};

GType flow_item_get_type ( void );

void flow_item_update ( GtkWidget *self );
void flow_item_invalidate ( GtkWidget *self );
void *flow_item_get_parent ( GtkWidget *self );
void flow_item_set_parent ( GtkWidget *self, GtkWidget *parent );
void flow_item_set_active ( GtkWidget *self, gboolean );
gboolean flow_item_get_active ( GtkWidget *self );
gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent );

#endif
