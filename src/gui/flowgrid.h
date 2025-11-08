#ifndef __FLOWGRID_H__
#define __FLOWGRID_H__

#include <gtk/gtk.h>
#include "flowitem.h"

#define FLOW_GRID_TYPE            (flow_grid_get_type())
#define FLOW_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FLOW_GRID_TYPE, FlowGrid))
#define FLOW_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FLOW_GRID_TYPE, FlowGridClass))
#define IS_FLOW_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLOW_GRID_TYPE))
#define IS_FLOW_GRIDCLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), FLOW_GRID_TYPE))
#define FLOW_GRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), FLOW_GRID_TYPE, FlowGridClass))

enum {
  /* drag_dest_add_(image|text|uri)_targets sets info to 0 */
  SFWB_DND_TARGET_GENERIC_GTK_API,
  SFWB_DND_TARGET_FLOW_ITEM
};

enum {
  FLOW_GRID_AXIS_DEFAULT = 0,
  FLOW_GRID_AXIS_ROWS,
  FLOW_GRID_AXIS_COLS,
};

typedef struct _FlowGrid FlowGrid;
typedef struct _FlowGridClass FlowGridClass;

struct _FlowGrid
{
  BaseWidget item;
};

struct _FlowGridClass
{
  BaseWidgetClass parent_class;

  gboolean limit;
};

typedef struct _FlowGridPrivate FlowGridPrivate;

struct _FlowGridPrivate
{
  gint cols,rows;
  gint primary_axis;
  gboolean icons, labels, tooltips;
  gint title_width;
  gboolean invalid;
  gboolean sort;
  GList *children;
  gint (*comp)( GtkWidget *, GtkWidget *, GtkWidget * );
  GtkTargetEntry *dnd_target;
  GtkWidget *parent;
  GtkWidget *grid;
};

GType flow_grid_get_type ( void );

void flow_grid_add_child ( GtkWidget *self, GtkWidget *child );
gboolean flow_grid_update ( GtkWidget *self );
void flow_grid_invalidate ( GtkWidget *self );
void flow_grid_delete_child ( GtkWidget *, void *parent );
guint flow_grid_n_children ( GtkWidget *self );
gpointer flow_grid_find_child ( GtkWidget *, gconstpointer parent );
void flow_grid_child_dnd_enable ( GtkWidget *, GtkWidget *, GtkWidget *);
GtkWidget *flow_grid_get_parent ( GtkWidget *self );
void flow_grid_set_parent ( GtkWidget *self, GtkWidget *parent );
void flow_grid_set_dnd_target ( GtkWidget *self, GtkTargetEntry *target );
GtkTargetEntry  *flow_grid_get_dnd_target ( GtkWidget *self );
void flow_grid_children_order ( GtkWidget *self, GtkWidget *ref,
    GtkWidget *child, gboolean after );

#endif
