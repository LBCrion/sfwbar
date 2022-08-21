#ifndef __TASKBAR_H__
#define __TASKBAR_H__

#include "basewidget.h"

#define TASKBAR_TYPE            (taskbar_get_type())
#define TASKBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_TYPE, Taskbar))
#define TASKBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_TYPE, TaskbarClass))
#define IS_TASKBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_TYPE))
#define IS_TASKBARCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_TYPE))

typedef struct _Taskbar Taskbar;
typedef struct _TaskbarClass TaskbarClass;

struct _Taskbar
{
  BaseWidget item;
};

struct _TaskbarClass
{
  BaseWidgetClass parent_class;
};

typedef struct _TaskbarPrivate TaskbarPrivate;

struct _TaskbarPrivate
{
  GtkWidget *taskbar;
};

GType taskbar_get_type ( void );

GtkWidget *taskbar_new( gboolean );
void taskbar_populate ( void );
void taskbar_invalidate_all ( window_t *win );
void taskbar_update_all ( void );
void taskbar_init_item ( window_t *win );
void taskbar_destroy_item ( window_t *win );
void taskbar_reparent_item ( window_t *win, const gchar *new_appid );

#endif
