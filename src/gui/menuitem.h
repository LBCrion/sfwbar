#ifndef __MENUITEM_H__
#define __MENUITEM_H__

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include "vm/expr.h"

#define DECLARE_MENU_ITEM(T_c, T_u, T_s, T_e, T_p) \
  G_DECLARE_FINAL_TYPE(T_c, T_u, T_s, T_e, T_p) \
  typedef struct _##T_c { T_p item; } T_c;

#define DEFINE_MENU_ITEM(T_c, T_u, T_s, T_e, T_p) \
  static void T_u##_iface_init ( void *a, void *b ) {} \
  static void T_u##_init ( T_c *a) { menu_item_init_common(GTK_WIDGET(a)); } \
  static void T_u##_class_init ( T_c##Class *a) { menu_iface_class_init(G_OBJECT_CLASS(a)); } \
  GtkWidget *T_u##_new ( void ) { return g_object_new(T_s##_TYPE_##T_e, NULL); } \
  G_DEFINE_TYPE_WITH_CODE(T_c, T_u, T_p, \
    G_IMPLEMENT_INTERFACE(MENU_TYPE_IFACE, T_u##_iface_init))

G_BEGIN_DECLS

#define MENU_TYPE_IFACE menu_iface_get_type ()
G_DECLARE_INTERFACE(MenuIface, menu_iface, MENU, IFACE, GObject)

typedef struct _MenuIfaceInterface { GTypeInterface parent;} MenuIfaceInterface;

#define MENU_TYPE_ITEM menu_item_get_type ()
DECLARE_MENU_ITEM(MenuItem, menu_item, MENU, ITEM, GtkMenuItem)

#define MENU_TYPE_ITEM_CHECK  menu_item_check_get_type ()
DECLARE_MENU_ITEM(MenuItemCheck, menu_item_check, MENU, ITEM_CHECK, GtkCheckMenuItem)

#define MENU_TYPE_ITEM_RADIO  menu_item_radio_get_type ()
DECLARE_MENU_ITEM(MenuItemRadio, menu_item_radio, MENU, ITEM_RADIO, GtkRadioMenuItem)

#define MENU_TYPE_ITEM_SEPARATOR menu_item_separator_get_type ()
DECLARE_MENU_ITEM(MenuItemSeparator, menu_item_separator, MENU, ITEM_SEPARATOR, GtkSeparatorMenuItem)

G_END_DECLS

typedef struct _MenuItemPrivate {
  gchar *id;
  gint sort_index;
  guint16 flags;
  gchar *desktop_file;
  gchar *css;
  GtkCssProvider *provider;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *menu;
  GBytes *action;
  expr_cache_t *label_expr;
  expr_cache_t *style_expr;
  expr_cache_t *tooltip_expr;
} MenuItemPrivate;

enum {
  MI_LABEL    = 1,
  MI_ICON     = 2,
  MI_TOOLTIP  = 4,
  MI_ACTION   = 8
};

GtkWidget *menu_item_new();
GtkWidget *menu_item_check_new();
GtkWidget *menu_item_radio_new();
GtkWidget *menu_item_separator_new();
GtkWidget *menu_item_get ( gchar *id, gboolean create );
gboolean menu_item_remove ( gchar *id );
void menu_item_update_from_desktop ( GtkWidget *self, const gchar *desktop_id);
void menu_item_update_from_app ( GtkWidget *self, GDesktopAppInfo *app);
void menu_item_set_id ( GtkWidget *self, gchar *id );
gchar *menu_item_get_id ( GtkWidget *self );
void menu_item_set_label ( GtkWidget *self, const gchar *label );
void menu_item_set_label_expr ( GtkWidget *self, GBytes *code );
void menu_item_set_icon ( GtkWidget *self, const gchar *icon );
void menu_item_set_action ( GtkWidget *self, GBytes *action );
void menu_item_set_tooltip ( GtkWidget *self, GBytes *tooltip );
void menu_item_set_sort_index ( GtkWidget *self, gint index );
gint menu_item_get_sort_index ( GtkWidget *self );
void menu_item_set_submenu ( GtkWidget *self, gchar *subname );
void menu_item_update ( GtkWidget *self );
gint menu_item_compare ( GtkWidget *i1, GtkWidget *i2 );
void menu_item_insert ( GtkWidget *menu, GtkWidget *item );

#endif
