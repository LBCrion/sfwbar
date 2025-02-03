#ifndef __MENUITEM_H__
#define __MENUITEM_H__

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include "vm/expr.h"

typedef struct _MenuItemPrivate {
  gchar *id;
  gint sort_index;
  guint16 flags;
  gchar *desktop_file;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *menu;
  GBytes *action;
  expr_cache_t *label_expr;
} MenuItemPrivate;

enum {
  MI_LABEL    = 1,
  MI_ICON     = 2,
  MI_TOOLTIP  = 4,
  MI_ACTION   = 8
};

void menu_item_init( GtkWidget *self );
GtkWidget *menu_item_get ( gchar *id, gboolean create );
void menu_item_update_from_desktop ( GtkWidget *self, const gchar *desktop_id);
void menu_item_update_from_app ( GtkWidget *self, GDesktopAppInfo *app);
void menu_item_set_id ( GtkWidget *self, gchar *id );
gchar *menu_item_get_id ( GtkWidget *self );
void menu_item_set_label ( GtkWidget *self, const gchar *label );
void menu_item_set_label_expr ( GtkWidget *self, GBytes *code );
void menu_item_set_icon ( GtkWidget *self, const gchar *icon );
void menu_item_set_action ( GtkWidget *self, GBytes *action );
void menu_item_set_tooltip ( GtkWidget *self, gchar *tooltip );
void menu_item_set_sort_index ( GtkWidget *self, gint index );
gint menu_item_get_sort_index ( GtkWidget *self );
void menu_item_set_submenu ( GtkWidget *self, gchar *subname );
gint menu_item_compare ( GtkWidget *i1, GtkWidget *i2 );
void menu_item_label_update ( GtkWidget *self );
void menu_item_insert ( GtkWidget *menu, GtkWidget *item );

#endif
