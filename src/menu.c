/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include "sfwbar.h"
#include "menu.h"
#include "bar.h"
#include "taskbargroup.h"
#include "scaleimage.h"
#include "popup.h"


static GHashTable *menus;

GtkWidget *menu_from_name ( gchar *name )
{
  if(!menus || !name)
    return NULL;
  return g_hash_table_lookup(menus, name);
}

void menu_remove ( gchar *name )
{
  GtkWidget *menu;
  GList *items, *iter;

  if(!menus || !name)
    return;
  menu = menu_from_name(name);
  if(!menu)
    return;
  items = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=items;iter;iter=g_list_next(iter))
    if(gtk_menu_item_get_submenu(iter->data))
      gtk_menu_item_set_submenu(iter->data,NULL);
  g_list_free(items);

  g_hash_table_remove(menus,name);
}

void menu_clamp_size ( GtkMenu *menu )
{
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkWindow *gdk_win;
  GtkWindow *toplevel;
  GdkRectangle workarea;
  gint w,h;

  toplevel = GTK_WINDOW(gtk_widget_get_ancestor(GTK_WIDGET(menu),GTK_TYPE_WINDOW));
  gdk_win = gtk_widget_get_window(GTK_WIDGET(toplevel));
  w = gdk_window_get_width(gdk_win);
  h = gdk_window_get_height(gdk_win);

  display = gdk_window_get_display(gdk_win);
  monitor = gdk_display_get_monitor_at_window(display,gdk_win);
  gdk_monitor_get_workarea(monitor,&workarea);

  gdk_window_resize(gdk_win,MIN(w,workarea.width),MIN(h,workarea.height));
}

GtkWidget *menu_new ( gchar *name )
{
  GtkWidget *menu;

  if(name)
  {
    menu = menu_from_name(name);
    if(menu)
      return menu;
  }

  menu = gtk_menu_new();
  g_signal_connect(menu,"popped-up",G_CALLBACK(menu_clamp_size),NULL);
  gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

  if(name)
  {
    g_object_ref_sink(G_OBJECT(menu));
    if(!menus)
      menus = g_hash_table_new_full((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal, g_free,g_object_unref);
    g_hash_table_insert(menus, g_strdup(name), menu);
  }
  return menu;
}

void menu_popup( GtkWidget *widget, GtkWidget *menu, GdkEvent *event,
    gpointer wid, guint16 *state )
{
  GdkGravity wanchor, manchor;
  GtkWidget *window;

  if(!menu || !widget)
    return;

  if(state)
    g_object_set_data( G_OBJECT(menu), "state", GUINT_TO_POINTER(*state) );
  g_object_set_data( G_OBJECT(menu), "wid", wid );
  g_object_set_data( G_OBJECT(menu), "caller", widget );

  window = gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW);
  if(gtk_window_get_window_type(GTK_WINDOW(window)) == GTK_WINDOW_POPUP)
    taskbar_group_pop_child(window, menu);

  popup_get_gravity(widget,&wanchor,&manchor);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_widget(GTK_MENU(menu),widget,wanchor,manchor,event);
}

gboolean menu_action_cb ( GtkWidget *w ,action_t *action )
{
  GtkWidget *parent = gtk_widget_get_ancestor(w,GTK_TYPE_MENU);
  gpointer wid;
  guint16 state;
  GtkWidget *widget;

  if(parent)
  {
    wid = g_object_get_data ( G_OBJECT(parent), "wid" );
    state = GPOINTER_TO_UINT(g_object_get_data ( G_OBJECT(parent), "state" ));
    widget = g_object_get_data ( G_OBJECT(parent),"caller" );
  }
  else
  {
    wid = NULL;
    state = 0;
    widget = NULL;
  }

  if(!wid)
    wid = wintree_get_focus();

  action_exec(widget,action,NULL,wintree_from_id(wid),&state);
  return TRUE;
}

GtkWidget *menu_item_new ( gchar *label, action_t *action )
{
  GtkWidget *item,*box,*wlabel,*img;
  gchar *text, *icon;

  icon = strchr(label,'%');
  if(icon)
    text = g_strndup(label,icon-label);
  else
    text = g_strdup(label);

  item = gtk_menu_item_new();
  gtk_widget_set_name(item,"menu_item");
  box = gtk_grid_new();
  if(icon)
  {
    img = scale_image_new();
    scale_image_set_image(img,icon+1,NULL);
    if(img)
      gtk_grid_attach(GTK_GRID(box),img,1,1,1,1);
  }
  if(text)
  {
    wlabel = gtk_label_new_with_mnemonic(text);
    gtk_grid_attach(GTK_GRID(box),wlabel,2,1,1,1);
    g_free(text);
  }
  gtk_container_add(GTK_CONTAINER(item),box);

  if(action)
  {
    g_signal_connect(G_OBJECT(item),"activate",
        G_CALLBACK(menu_action_cb),action);
    g_object_weak_ref(G_OBJECT(item),(GWeakNotify)action_free,action);
  }

  return item;
}
