#ifndef __SWITCHER_H__
#define __SWITCHER_H__

#include "flowgrid.h"

#define SWITCHER_TYPE            (switcher_get_type())
#define SWITCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SWITCHER_TYPE, Switcher))
#define SWITCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SWITCHER_TYPE, SwitcherClass))
#define IS_SWITCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SWITCHER_TYPE))
#define IS_SWITCHERCLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), SWITCHER_TYPE))

typedef struct _Switcher Switcher;
typedef struct _SwitcherClass SwitcherClass;

struct _Switcher
{
  FlowGrid item;
};

struct _SwitcherClass
{
  FlowGridClass parent_class;
};

typedef struct _SwitcherPrivate SwitcherPrivate;

struct _SwitcherPrivate
{
  GtkWidget *switcher;
  gint filter;
};

GType switcher_get_type ( void );

GtkWidget *switcher_new();
gboolean switcher_is_focused ( gpointer uid );

#endif
