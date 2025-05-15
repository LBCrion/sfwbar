#ifndef __TASKBAR_H__
#define __TASKBAR_H__

#include "flowgrid.h"

#define TASKBAR_TYPE            (taskbar_get_type())
#define TASKBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_TYPE, Taskbar))
#define TASKBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_TYPE, TaskbarClass))
#define IS_TASKBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_TYPE))
#define IS_TASKBARCLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_TYPE))
#define TASKBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), TASKBAR_TYPE, TaskbarClass))

typedef struct _Taskbar Taskbar;
typedef struct _TaskbarClass TaskbarClass;

struct _Taskbar
{
  FlowGrid item;
};

struct _TaskbarClass
{
  FlowGridClass parent_class;
};

typedef struct _TaskbarPrivate TaskbarPrivate;

struct _TaskbarPrivate
{
  GtkWidget *parent;
};

GType taskbar_get_type ( void );

GtkWidget *taskbar_new( GtkWidget *parent );
GtkWidget *taskbar_get_parent ( GtkWidget *self );
GtkWidget *taskbar_get_taskbar ( GtkWidget *, window_t *, gboolean );

#endif
