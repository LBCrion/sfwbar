#ifndef __GRID_H__
#define __GRID_H__

#include "basewidget.h"

#define GRID_TYPE            (grid_get_type())
#define GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GRID_TYPE, Grid))
#define GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GRID_TYPE, GridClass))
#define IS_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GRID_TYPE))
#define IS_GRIDCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GRID_TYPE))

typedef struct _Grid Grid;
typedef struct _GridClass GridClass;

struct _Grid
{
  BaseWidget item;
};

struct _GridClass
{
  BaseWidgetClass parent_class;
};

typedef struct _GridPrivate GridPrivate;

struct _GridPrivate
{
  GtkWidget *grid;
  GList *last;
  GList *children;
  gint dir;
};

GType grid_get_type ( void );

GtkWidget *grid_new();
gboolean grid_attach ( GtkWidget *self, GtkWidget *child );
void grid_detach( GtkWidget *child, GtkWidget *self );

#endif
