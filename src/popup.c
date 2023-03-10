/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "grid.h"
#include "config.h"
#include "basewidget.h"
#include "popup.h"
#include "bar.h"

static GHashTable *popup_list;

void popup_get_gravity ( GtkWidget *widget, GdkGravity *wanchor,
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

void popup_show ( GtkWidget *parent, GtkWidget *popup )
{
  GdkRectangle rect;
  GdkWindow *gparent, *gpopup;
  GdkGravity wanchor, panchor;

  gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(popup)));
  gtk_widget_unrealize(popup);
  gtk_widget_realize(popup);
  gparent = gtk_widget_get_window(parent);
  gpopup = gtk_widget_get_window(
      gtk_widget_get_ancestor(popup,GTK_TYPE_WINDOW));
  rect.x = 0;
  rect.y = 0;
  rect.width = gdk_window_get_width(gparent);
  rect.height = gdk_window_get_height(gparent);
  gdk_window_set_transient_for(gpopup, gparent);
  popup_get_gravity(parent,&wanchor,&panchor);
  gdk_window_move_to_rect(gpopup, &rect,wanchor,panchor,
      GDK_ANCHOR_FLIP_X | GDK_ANCHOR_FLIP_Y,0,0);
  css_widget_cascade(popup,NULL);
}

void popup_trigger ( GtkWidget *parent, gchar *name )
{
  GtkWidget *popup;

  popup = popup_from_name(name);
  if(!popup || !parent)
    return;

  if(gtk_widget_get_visible(popup))
    gtk_widget_hide(popup);
  else
    popup_show(parent, popup);
}

GtkWidget *popup_new ( gchar *name )
{
  GtkWidget *win, *grid;

  if(!popup_list)
    popup_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  win = popup_from_name(name);
  if(win)
    return win;

  win = gtk_window_new(GTK_WINDOW_POPUP);
  grid = grid_new();
  gtk_container_add(GTK_CONTAINER(win),grid);
  gtk_widget_set_name(win,name);
  gtk_widget_set_name(grid,name);
  g_hash_table_insert(popup_list,g_strdup(name),win);
  return win;
}

GtkWidget *popup_from_name ( gchar *name )
{
  if(!popup_list || !name)
    return NULL;
  return g_hash_table_lookup(popup_list,name);
}
