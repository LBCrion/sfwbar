#ifndef __SNIITEM_H__
#define __SNIITEM_H__

#include "sfwbar.h" 
#include "flowitem.h"
#include "sni.h"

#define TRAY_ITEM_TYPE            (tray_item_get_type())
#define TRAY_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TRAY_ITEM_TYPE, TrayItem))
#define TRAY_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), TRAY_ITEM_TYPE, TrayItemClass))
#define IS_TRAY_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), TRAY_ITEM_TYPE))
#define IS_TRAY_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TRAY_ITEM_TYPE))

typedef struct _TrayItem TrayItem;
typedef struct _TrayItemClass TrayItemClass;

struct _TrayItem
{
  FlowItem item;
};

struct _TrayItemClass
{
  FlowItemClass parent_class;
};

typedef struct _TrayItemPrivate TrayItemPrivate;

struct _TrayItemPrivate
{
  SniItem *sni;
  GtkWidget *button;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *tray;
  gint icon_idx, pix_idx, old_icon, old_pix;
  gboolean invalid;
  GdkEvent *last_event;
};

GType tray_item_get_type ( void );

GtkWidget *tray_item_new( SniItem *win, GtkWidget *tray );
SniItem*tray_item_get_sni ( GtkWidget *self );
void tray_item_update ( GtkWidget *self );
gint tray_item_compare ( GtkWidget *, GtkWidget *, GtkWidget * );
void tray_item_invalidate ( GtkWidget *self );

#endif
