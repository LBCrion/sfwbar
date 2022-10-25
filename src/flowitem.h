#ifndef __FLOWITEM_H__
#define __FLOWITEM_H__

#include <gtk/gtk.h>
#include "basewidget.h"

#define FLOW_ITEM_TYPE            (flow_item_get_type())

G_DECLARE_DERIVABLE_TYPE (FlowItem, flow_item, FLOW, ITEM, BaseWidget);

typedef struct _FlowItemClass FlowItemClass;

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
};

GType flow_item_get_type ( void );

void flow_item_update ( GtkWidget *self );
void flow_item_invalidate ( GtkWidget *self );
void *flow_item_get_parent ( GtkWidget *self );
void flow_item_set_active ( GtkWidget *self, gboolean );
gboolean flow_item_get_active ( GtkWidget *self );
gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent );

#endif
