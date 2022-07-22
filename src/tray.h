#ifndef __TRAY_H__
#define __TRAY_H__

#include "basewidget.h"
#include "flowgrid.h"

#define TRAY_TYPE            (tray_get_type())
#define TRAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TRAY_TYPE, Tray))
#define TRAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TRAY_TYPE, TrayClass))
#define IS_TRAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TRAY_TYPE))
#define IS_TRAYCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TRAY_TYPE))

typedef struct _Tray Tray;
typedef struct _TrayClass TrayClass;

struct _Tray
{
  BaseWidget item;
};

struct _TrayClass
{
  BaseWidgetClass parent_class;
};

typedef struct _TrayPrivate TrayPrivate;

struct _TrayPrivate
{
  GtkWidget *tray;
};

GType tray_get_type ( void );

GtkWidget *tray_new();
void tray_item_init_for_all ( SniItem *sni );
void tray_item_destroy ( SniItem *sni );
void tray_update ( void );
void tray_invalidate_all ( SniItem *sni );

#endif
