#ifndef __TASKBARGROUPITEM_H__
#define __TASKBARGROUPITEM_H__

#include "sfwbar.h" 
#include "flowitem.h"
#include "action.h"

#define TASKBAR_GROUP_TYPE            (taskbar_group_get_type())
#define TASKBAR_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_GROUP_TYPE, TaskbarGroup))
#define TASKBAR_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_GROUP_TYPE, TaskbarGroupClass))
#define IS_TASKBAR_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_GROUP_TYPE))
#define IS_TASKBAR_GROUPCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_GROUP_TYPE))

typedef struct _TaskbarGroup TaskbarGroup;
typedef struct _TaskbarGroupClass TaskbarGroupClass;

struct _TaskbarGroup
{
  FlowItem item;
};

struct _TaskbarGroupClass
{
  FlowItemClass parent_class;
};

typedef struct _TaskbarGroupPrivate TaskbarGroupPrivate;

struct _TaskbarGroupPrivate
{
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *taskbar;
  GtkWidget *tgroup;
  GtkWidget *popover;
  gchar *appid;
  gboolean invalid;
  GList *holds;
};

GType taskbar_group_get_type ( void );

GtkWidget *taskbar_group_new( const gchar *appid, GtkWidget *taskbar );
void taskbar_group_pop_child ( GtkWidget *self, GtkWidget *child );

#endif
