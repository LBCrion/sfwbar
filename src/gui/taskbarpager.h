#ifndef __TASKBARDESK_H__
#define __TASKBARDESK_H__

#include "flowitem.h"
#include "workspace.h"
#include "action.h"
#include "taskbar.h"

#define TASKBAR_PAGER_TYPE            (taskbar_pager_get_type())
#define TASKBAR_PAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TASKBAR_PAGER_TYPE, TaskbarPager))
#define TASKBAR_PAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TASKBAR_PAGER_TYPE, TaskbarPagerClass))
#define IS_TASKBAR_PAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TASKBAR_PAGER_TYPE))
#define IS_TASKBAR_PAGERCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TASKBAR_PAGER_TYPE))

typedef struct _TaskbarPager TaskbarPager;
typedef struct _TaskbarPagerClass TaskbarPagerClass;

struct _TaskbarPager
{
  FlowItem item;
};

struct _TaskbarPagerClass
{
  FlowItemClass parent_class;
};

typedef struct _TaskbarPagerPrivate TaskbarPagerPrivate;

struct _TaskbarPagerPrivate
{
  GtkWidget *button;
  GtkWidget *grid;
  GtkWidget *shell;
  GtkWidget *taskbar;
  workspace_t *ws;
  gboolean invalid;
  gboolean single;
  GList *holds;
};

GType taskbar_pager_get_type ( void );

GtkWidget *taskbar_pager_new( workspace_t *ws, GtkWidget *taskbar );
GtkWidget *taskbar_pager_get_taskbar ( GtkWidget *, window_t *, gboolean );

#endif
