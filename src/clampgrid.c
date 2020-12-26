
#include <gtk/gtk.h>


#define CLAMP_GRID_TYPE            (clamp_grid_get_type())
#define CLAMP_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), CLAMP_GRID_TYPE, ClampGrid))
#define CLAMP_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CLAMP_GRID_TYPE, ClampGridClass))
#define IS_CLAMP_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CLAMP_GRID_TYPE))
#define IS_CLAMP_GRIDCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CLAMP_GRID_TYPE))

static void clamp_grid_get_preferred_width (GtkWidget *widget, int *minimal, int *natural);
static void clamp_grid_get_preferred_height (GtkWidget *widget, int *minimal, int *natural);

typedef struct _ClampGrid ClampGrid;
typedef struct _ClampGridClass ClampGridClass;

struct _ClampGrid
{
  GtkGrid grid;
  gint x,y;
};

struct _ClampGridClass
{
  GtkGridClass parent_class;
};

typedef struct _ClampGridPrivate ClampGridPrivate;

struct _ClampGridPrivate
{
        gint x,y;
};

// G_DEFINE_TYPE(ClampGrid, clamp_grid, GTK_TYPE_GRID);
G_DEFINE_TYPE_WITH_CODE (ClampGrid, clamp_grid, GTK_TYPE_GRID, G_ADD_PRIVATE (ClampGrid));

static void clamp_grid_class_init ( ClampGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = clamp_grid_get_preferred_width;
  widget_class->get_preferred_height = clamp_grid_get_preferred_height;
}

static void clamp_grid_init (ClampGrid *cgrid )
{
  ClampGridPrivate *priv = clamp_grid_get_instance_private(CLAMP_GRID(cgrid));
  priv->x=700;
  priv->y=-1;
}

GtkWidget *clamp_grid_new()
{
  return GTK_WIDGET(g_object_new(clamp_grid_get_type(), NULL));
}

void clamp_grid_set_clamp ( ClampGrid *cgrid, int x, int y )
{
  ClampGridPrivate *priv = clamp_grid_get_instance_private(CLAMP_GRID(cgrid));
  priv->x=x;
  priv->y=y;
}

static void clamp_grid_get_preferred_width (GtkWidget *widget, int *minimal, int *natural)
{
  ClampGridPrivate *priv;
  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_CLAMP_GRID(widget));

  priv = clamp_grid_get_instance_private(CLAMP_GRID(widget));

  GTK_WIDGET_CLASS(clamp_grid_parent_class)->get_preferred_width(widget,minimal,natural);
  if(priv->x>0)
  {
    if(priv->x<(*minimal))
      *minimal = priv->x;
    if(priv->x<(*natural))
      *natural = priv->x;
  }
}

static void clamp_grid_get_preferred_height (GtkWidget *widget, int *minimal, int *natural)
{
  ClampGridPrivate *priv;
  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_CLAMP_GRID(widget));

  priv = clamp_grid_get_instance_private(CLAMP_GRID(widget));

  GTK_WIDGET_CLASS(clamp_grid_parent_class)->get_preferred_height(widget,minimal,natural);
  if(priv->y>0)
  {
    if(priv->y<(*minimal))
      *minimal = priv->y;
    if(priv->y<(*natural))
      *natural = priv->y;
  }
}

