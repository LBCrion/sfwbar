#ifndef __SNI_H__
#define __SNI_H__

#define SNI_MAX_STRING 9

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_THEME = 8,
  SNI_PROP_ICONPIX = 9,
  SNI_PROP_OVLAYPIX = 10,
  SNI_PROP_ATTNPIX = 11,
  SNI_PROP_WINDOWID = 12,
  SNI_PROP_TOOLTIP = 13,
  SNI_PROP_ISMENU = 14,
  SNI_PROP_MENU = 15
};

typedef struct sni_host {
  gchar *iface;
  gchar *watcher;
  gchar *item_iface;
  GList *items;
} SniHost;

typedef struct sni_watcher {
  guint regid;
  gboolean watcher_registered;
  gchar *iface;
  GList *items;
  GDBusNodeInfo *idata;
  SniHost *host;
} SniWatcher;

typedef struct sni_item {
  gchar *uid;
  gchar *udest;
  gchar *dest;
  gchar *path;
  gchar *string[SNI_MAX_STRING];
  gchar *menu_path;
  GdkPixbuf *pixbuf[3];
  gboolean menu;
  gboolean dirty;
  gint ref;
  guint signal;
  GCancellable *cancel;
  GtkWidget *image;
  GtkWidget *box;
  SniHost *host;
} SniItem;

void sni_init ( void );
void sni_update ( void );
GDBusConnection *sni_get_connection ( void );
void sni_item_set_icon ( SniItem *sni, gint icon, gint pix );
void sni_get_menu ( SniItem *sni, GdkEvent *event );
SniItem *sni_item_new (GDBusConnection *, SniHost *, const gchar *);
void sni_item_free ( SniItem *sni );

#endif
