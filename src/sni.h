#ifndef __SNI_H__
#define __SNI_H__

#define SNI_MAX_STRING 16
#define SNI_MAX_PROP 19

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_LABEL = 8,
  SNI_PROP_LGUIDE = 9,
  SNI_PROP_THEME = 10,
  SNI_PROP_ICONPIX = 11,
  SNI_PROP_OVLAYPIX = 12,
  SNI_PROP_ATTNPIX = 13,
  SNI_PROP_WINDOWID = 14,
  SNI_PROP_TOOLTIP = 15,
  SNI_PROP_ISMENU = 16,
  SNI_PROP_MENU = 17,
  SNI_PROP_ORDER = 18
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

#endif
