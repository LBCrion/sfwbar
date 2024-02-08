#ifndef __TASKBARITEM_H__
#define __TASKBARITEM_H__

#include "sfwbar.h" 
#include "flowitem.h"
#include "action.h"

#define TASKBAR_ITEM_TYPE            (taskbar_item_get_type())
#define TASKBAR_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_ITEM_TYPE, TaskbarItem))
#define TASKBAR_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_ITEM_TYPE, TaskbarItemClass))
#define IS_TASKBAR_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_ITEM_TYPE))
#define IS_TASKBAR_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_ITEM_TYPE))

typedef struct _TaskbarItem TaskbarItem;
typedef struct _TaskbarItemClass TaskbarItemClass;

struct _TaskbarItem
{
  FlowItem item;
};

struct _TaskbarItemClass
{
  FlowItemClass parent_class;
};

typedef struct _TaskbarItemPrivate TaskbarItemPrivate;

struct _TaskbarItemPrivate
{
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *taskbar;
  window_t *win;
  action_t **actions;
  gboolean invalid;
  GdkModifierType saved_modifiers;
};

GType taskbar_item_get_type ( void );

GtkWidget *taskbar_item_new( window_t *win, GtkWidget *taskbar );
void taskbar_item_set_image ( GtkWidget *icon, gchar *appid );

#endif
