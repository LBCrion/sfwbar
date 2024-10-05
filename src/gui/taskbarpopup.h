#ifndef __TASKBARPOPUP_H__
#define __TASKBARPOPUP_H__

#include "sfwbar.h" 
#include "flowitem.h"
#include "action.h"
#include "taskbar.h"

#define TASKBAR_POPUP_TYPE            (taskbar_popup_get_type())
#define TASKBAR_POPUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_POPUP_TYPE, TaskbarPopup))
#define TASKBAR_POPUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_POPUP_TYPE, TaskbarPopupClass))
#define IS_TASKBAR_POPUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_POPUP_TYPE))
#define IS_TASKBAR_POPUPCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_POPUP_TYPE))

typedef struct _TaskbarPopup TaskbarPopup;
typedef struct _TaskbarPopupClass TaskbarPopupClass;

struct _TaskbarPopup
{
  FlowItem item;
};

struct _TaskbarPopupClass
{
  FlowItemClass parent_class;
};

typedef struct _TaskbarPopupPrivate TaskbarPopupPrivate;

struct _TaskbarPopupPrivate
{
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *shell;
  GtkWidget *tgroup;
  GtkWidget *popover;
  gchar *appid;
  gboolean invalid;
  gboolean single;
};

GType taskbar_popup_get_type ( void );

GtkWidget *taskbar_popup_new( const gchar *appid, GtkWidget *taskbar );
void taskbar_popup_pop_child ( GtkWidget *self, GtkWidget *child );
GtkWidget *taskbar_popup_get_taskbar ( GtkWidget *, window_t *, gboolean );

#endif
