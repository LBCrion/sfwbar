#ifndef __SNI_H__
#define __SNI_H__

#define SNI_MAX_STRING (SNI_PROP_ATTNPIX+1)

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_ICONACC = 8,
  SNI_PROP_ATTNACC = 9,
  SNI_PROP_LABEL = 10,
  SNI_PROP_LGUIDE = 11,
  SNI_PROP_THEME = 12,
  SNI_PROP_ICONPIX = 13,
  SNI_PROP_OVLAYPIX = 14,
  SNI_PROP_ATTNPIX = 15,
  SNI_PROP_WINDOWID = 16,
  SNI_PROP_TOOLTIP = 17,
  SNI_PROP_ISMENU = 18,
  SNI_PROP_MENU = 19,
  SNI_PROP_ORDER = 20,
  SNI_PROP_MAX = 21
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
  gchar *tooltip;
  gboolean menu;
  gint ref;
  guint signal;
  guint32 order;
  GCancellable *cancel;
  SniHost *host;
} SniItem;

void sni_init ( void );
void sni_update ( void );
GDBusConnection *sni_get_connection ( void );
void sni_get_menu ( GtkWidget *widget, GdkEvent *event );
SniItem *sni_item_new (GDBusConnection *, SniHost *, const gchar *);
void sni_item_free ( SniItem *sni );
GList *sni_item_get_list ( void );
gchar *sni_item_tooltip ( SniItem *item );
gchar *sni_item_icon ( SniItem *item );

#endif
