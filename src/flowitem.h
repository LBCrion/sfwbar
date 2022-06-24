#ifndef __FLOWITEM_H__
#define __FLOWITEM_H__

#include <gtk/gtk.h>

#define FLOW_ITEM_TYPE            (flow_item_get_type())

G_DECLARE_DERIVABLE_TYPE (FlowItem, flow_item, FLOW, ITEM, GtkEventBox);

typedef struct _FlowItemClass FlowItemClass;

struct _FlowItemClass
{
  GtkEventBoxClass parent_class;

  void (*update) ( GtkWidget *self );
  void* (*get_parent) ( GtkWidget *self );
  gint (*compare) (GtkWidget *, GtkWidget *, GtkWidget *);
};

typedef struct _FlowItemPrivate FlowItemPrivate;

struct _FlowItemPrivate
{
  gboolean active;
};

GType flow_item_get_type ( void );

void flow_item_update ( GtkWidget *self );
void *flow_item_get_parent ( GtkWidget *self );
void flow_item_set_active ( GtkWidget *self, gboolean );
gboolean flow_item_get_active ( GtkWidget *self );
gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent );

#endif
