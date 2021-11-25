/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */


#include <gtk/gtk.h>

#define FLOW_GRID_TYPE            (flow_grid_get_type())
#define FLOW_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FLOW_GRID_TYPE, FlowGrid))
#define FLOW_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FLOW_GRID_TYPE, FlowGridClass))
#define IS_FLOW_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLOW_GRID_TYPE))
#define IS_FLOW_GRIDCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FLOW_GRID_TYPE))

static void flow_grid_get_preferred_width (GtkWidget *widget, gint *minimal, gint *natural);
static void flow_grid_get_preferred_height (GtkWidget *widget, gint *minimal, gint *natural);

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
  gint cols,rows,i;
};

G_DEFINE_TYPE_WITH_CODE (FlowGrid, flow_grid, GTK_TYPE_GRID, G_ADD_PRIVATE (FlowGrid));

static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
}

static void flow_grid_init (FlowGrid *cgrid )
{
  FlowGridPrivate *priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->rows = 1;
  priv->cols = 0;

  gtk_grid_set_row_homogeneous(GTK_GRID(cgrid),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(cgrid),TRUE);
}

GtkWidget *flow_grid_new()
{
  return GTK_WIDGET(g_object_new(flow_grid_get_type(), NULL));
}

static void flow_grid_get_preferred_width (GtkWidget *widget, gint *minimal, gint *natural)
{
  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  *minimal = 1;
  *natural = 1;
}

static void flow_grid_get_preferred_height (GtkWidget *widget, gint *minimal, gint *natural)
{
  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  *minimal = 1;
  *natural = 1;
}

void flow_grid_set_cols ( GtkWidget *cgrid, gint cols )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->cols = cols;
  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
  if((priv->rows>0)&&(priv->cols>0))
    priv->cols = -1;
}

void flow_grid_set_rows ( GtkWidget *cgrid, gint rows )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->rows = rows;
  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
  if((priv->rows>0)&&(priv->cols>0))
    priv->cols = -1;
}

void flow_grid_attach ( GtkWidget *cgrid, GtkWidget *w )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  if(!gtk_container_get_children(GTK_CONTAINER(cgrid)))
    priv->i=0;

  if(priv->rows>0)
    gtk_grid_attach(GTK_GRID(cgrid),w,
        (priv->i)/(priv->rows),(priv->i)%(priv->rows),1,1);
  else
    gtk_grid_attach(GTK_GRID(cgrid),w,
        (priv->i)%(priv->cols),(priv->i)/(priv->cols),1,1);
  priv->i++;
}

void flow_grid_pad ( GtkWidget *cgrid )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));
  
  if(priv->rows>0)
    for(;priv->i<priv->rows;priv->i++)
      gtk_grid_attach(GTK_GRID(cgrid),gtk_label_new(""),0,priv->i,1,1);
  else
    for(;priv->i<priv->cols;priv->i++)
      gtk_grid_attach(GTK_GRID(cgrid),gtk_label_new(""),priv->i,0,1,1);

}
