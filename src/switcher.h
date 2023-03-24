#ifndef __SWITCHER_H__
#define __SWITCHER_H__

#include "basewidget.h"
#include "flowgrid.h"

#define SWITCHER_TYPE            (switcher_get_type())
#define SWITCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SWITCHER_TYPE, Switcher))
#define SWITCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SWITCHER_TYPE, SwitcherClass))
#define IS_SWITCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SWITCHER_TYPE))
#define IS_SWITCHERCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SWITCHER_TYPE))

typedef struct _Switcher Switcher;
typedef struct _SwitcherClass SwitcherClass;

struct _Switcher
{
  BaseWidget item;
};

struct _SwitcherClass
{
  BaseWidgetClass parent_class;
};

typedef struct _SwitcherPrivate SwitcherPrivate;

struct _SwitcherPrivate
{
  GtkWidget *switcher;
  gint filter;
};

GType switcher_get_type ( void );
void switcher_window_delete ( window_t *win );

GtkWidget *switcher_new();
gboolean switcher_state ( void );
gboolean switcher_event ( gpointer );
void switcher_invalidate ( window_t *win );
void switcher_update ( void );
void switcher_window_init ( window_t *win);
void switcher_populate ( void );
void switcher_set_filter ( GtkWidget *self, gint filter );
gint switcher_get_filter ( GtkWidget *self );

#endif
