#ifndef __SNIITEM_H__
#define __SNIITEM_H__

#include "sfwbar.h" 

#define SNI_ITEM_TYPE            (sni_item_get_type())
#define SNI_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SNI_ITEM_TYPE, SniItem))
#define SNI_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SNI_ITEM_TYPE, SniItemClass))
#define IS_SNI_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SNI_ITEM_TYPE))
#define IS_SNI_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SNI_ITEM_TYPE))

typedef struct _SniItem SniItem;
typedef struct _SniItemClass SniItemClass;

struct _SniItem
{
  FlowItem item;
};

struct _SniItemClass
{
  FlowItemClass parent_class;
};

typedef struct _SniItemPrivate SniItemPrivate;

struct _SniItemPrivate
{
  gchar *uid;
  gchar *udest;
  gchar *dest;
  gchar *path;
  gchar *iface;
  gchar *string[MAX_STRING];
  gchar *menu_path;
  GdkPixbuf *pixbuf[3];
  gboolean menu;
  gboolean dirty;
  gint ref;
  guint signal;
  GCancellable *cancel;
  GtkWidget *image;
};

GType sni_item_get_type ( void );

GtkWidget *sni_item_new (GDBusConnection *con, struct sni_iface *iface,
    const gchar *uid);
void sni_item_set_label ( GtkWidget *self );
window_t *sni_item_get_window ( GtkWidget *self );
GtkWidget *sni_item_get_sni ( GtkWidget *self );
void sni_item_update ( GtkWidget *self );

#endif
