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
};

GType flow_item_get_type ( void );

#endif
