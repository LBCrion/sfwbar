/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "menu.h"
#include "bar.h"
#include "taskbargroup.h"

static GHashTable *menus;

GtkWidget *menu_from_name ( gchar *name )
{
  if(!menus || !name)
    return NULL;
  return g_hash_table_lookup(menus, name);
}

void menu_remove ( gchar *name )
{
  if(name)
    g_hash_table_remove(menus,name);
}

void menu_add ( gchar *name, GtkWidget *menu )
{
  if(!menus)
    menus = g_hash_table_new_full((GHashFunc)str_nhash,(GEqualFunc)str_nequal,
        g_free,g_object_unref);

  if(!g_hash_table_lookup_extended(menus, name, NULL, NULL))
  {
    g_object_ref_sink(menu);
    g_hash_table_insert(menus, name, menu);
  }
  else
    g_free(name);
}

void menu_popup_get_gravity ( GtkWidget *widget, GdkGravity *wanchor,
    GdkGravity *manchor )
{
  switch(bar_get_toplevel_dir(widget))
  {
    case GTK_POS_TOP:
      *wanchor = GDK_GRAVITY_SOUTH_WEST;
      *manchor = GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_LEFT:
      *wanchor = GDK_GRAVITY_NORTH_EAST;
      *manchor = GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_RIGHT:
      *wanchor = GDK_GRAVITY_NORTH_WEST;
      *manchor = GDK_GRAVITY_NORTH_EAST;
      break;
    default:
      *wanchor = GDK_GRAVITY_NORTH_WEST;
      *manchor = GDK_GRAVITY_SOUTH_WEST;
      break;
  }
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

  menu_popup_get_gravity(widget,&wanchor,&manchor);
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
  box = gtk_grid_new();
  if(icon)
  {
    img = gtk_image_new_from_icon_name(icon+1,GTK_ICON_SIZE_MENU);
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

  g_signal_connect(G_OBJECT(item),"activate",
      G_CALLBACK(menu_action_cb),action);
  g_object_weak_ref(G_OBJECT(item),(GWeakNotify)action_free,action);

  return item;
}
