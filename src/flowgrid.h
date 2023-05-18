#ifndef __FLOWGRID_H__
#define __FLOWGRID_H__

#include <gtk/gtk.h>
#include "flowitem.h"

#define FLOW_GRID_TYPE            (flow_grid_get_type())
#define FLOW_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FLOW_GRID_TYPE, FlowGrid))
#define FLOW_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FLOW_GRID_TYPE, FlowGridClass))
#define IS_FLOW_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLOW_GRID_TYPE))
#define IS_FLOW_GRIDCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FLOW_GRID_TYPE))

typedef struct _FlowGrid FlowGrid;
typedef struct _FlowGridClass FlowGridClass;

struct _FlowGrid
{
  GtkGrid grid;
};

struct _FlowGridClass
{
  GtkGridClass parent_class;
};

typedef struct _FlowGridPrivate FlowGridPrivate;

struct _FlowGridPrivate
{
  gint cols,rows;
  gint primary_axis;
  gboolean limit;
  gboolean invalid;
  gboolean sort;
  GList *children;
  gint (*comp)( GtkWidget *, GtkWidget *, GtkWidget * );
  GtkTargetEntry *dnd_target;
};

GType flow_grid_get_type ( void );

GtkWidget *flow_grid_new( gboolean limit);
void flow_grid_set_rows ( GtkWidget *cgrid, gint rows );
void flow_grid_set_cols ( GtkWidget *cgrid, gint cols );
void flow_grid_set_primary ( GtkWidget *self, gint primary );
void flow_grid_add_child ( GtkWidget *self, GtkWidget *child );
void flow_grid_update ( GtkWidget *self );
void flow_grid_invalidate ( GtkWidget *self );
void flow_grid_delete_child ( GtkWidget *, void *parent );
guint flow_grid_n_children ( GtkWidget *self );
gpointer flow_grid_find_child ( GtkWidget *, gconstpointer parent );
void flow_grid_child_dnd_enable ( GtkWidget *, GtkWidget *, GtkWidget *);
void flow_grid_set_sort ( GtkWidget *cgrid, gboolean sort );
void flow_grid_copy_properties ( GtkWidget *dest, GtkWidget *src );

#endif
