/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"
#include "bar.h"
#include "wayland.h"
#include "grid.h"
#include "config.h"

static GHashTable *bar_list;

GtkWindow *bar_from_name ( gchar *name )
{
  if(!bar_list)
    return NULL;

  return g_hash_table_lookup(bar_list,name?name:"sfwbar");
}

GtkWidget *bar_grid_from_name ( gchar *addr )
{
  GtkWidget *bar, *box;
  GtkWidget *widget = NULL;
  gchar *grid, *ptr, *name;

  if(!addr)
    addr = "sfwbar";

  ptr = strchr(addr,':');

  if(ptr)
  {
    grid = ptr + 1;
    if(ptr == addr)
      name = g_strdup("sfwbar");
    else
      name = g_strndup(addr,ptr-addr);
  }
  else
  {
    grid = NULL;
    name = g_strdup(addr);
  }

  bar = GTK_WIDGET(bar_from_name(name));
  if(!bar)
    bar = GTK_WIDGET(bar_new(name));
  g_free(name);

  if(grid && !g_ascii_strcasecmp(grid,"center"))
    widget = g_object_get_data(G_OBJECT(bar),"center");
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
    widget = g_object_get_data(G_OBJECT(bar),"end");
  else
    widget = g_object_get_data(G_OBJECT(bar),"start");

  if(widget)
    return widget;

  widget = grid_new();
  gtk_widget_set_name(base_widget_get_child(widget),"layout");

  box = gtk_bin_get_child(GTK_BIN(bar));
  if(grid && !g_ascii_strcasecmp(grid,"center"))
  {
    gtk_box_set_center_widget(GTK_BOX(box),widget);
    g_object_set_data(G_OBJECT(bar),"center",widget);
  }
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
  {
    gtk_box_pack_end(GTK_BOX(box),widget,TRUE,TRUE,0);
    g_object_set_data(G_OBJECT(bar),"end",widget);
  }
  else
  {
    gtk_box_pack_start(GTK_BOX(box),widget,TRUE,TRUE,0);
    g_object_set_data(G_OBJECT(bar),"start",widget);
  }
  return widget;
}

gboolean bar_hide_event ( const gchar *mode )
{
  gchar state;
  void *key,*bar;
  static gchar pstate = 's';
  GHashTableIter iter;

  state = pstate;
  if(mode)        // if NULL, return to persistent state
    switch(*mode)
    {
      case 'd':
      case 's':   // show
        pstate = state = 's';
        break;
      case 'h':   // hide
        pstate = state = 'h';
        break;
      case 'v':   // visible by modfier
        state ='s';
        break;
      case 't':    // toggle
        if( pstate == 's' )
          pstate = state = 'h';
        else
          pstate = state = 's';
        break;
    }

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    if(state=='h')
      gtk_widget_hide(GTK_WIDGET(bar));
    else
      if(!gtk_widget_is_visible(GTK_WIDGET(bar)))
      {
        bar_update_monitor(GTK_WINDOW(bar));
        gtk_widget_show_now(GTK_WIDGET(bar));
      }
  return TRUE;
}

gint bar_get_toplevel_dir ( GtkWidget *widget )
{
  GtkWidget *toplevel;
  gint toplevel_dir;

  if(!widget)
    return GTK_POS_RIGHT;

  toplevel = gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW);

  if(!toplevel)
    return GTK_POS_RIGHT;

  gtk_widget_style_get(toplevel, "direction",&toplevel_dir,NULL);
  return toplevel_dir;
}

void bar_set_layer ( gchar *layer_str, GtkWindow *bar )
{
  GtkLayerShellLayer layer;

  if(!bar || !layer_str)
    return;

  if(!g_ascii_strcasecmp(layer_str,"background"))
    layer = GTK_LAYER_SHELL_LAYER_BACKGROUND;
  else if(!g_ascii_strcasecmp(layer_str,"bottom"))
    layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
  else if(!g_ascii_strcasecmp(layer_str,"overlay"))
    layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
  else
    layer = GTK_LAYER_SHELL_LAYER_TOP;

#if GTK_LAYER_VER_MINOR > 5 || GTK_LAYER_VER_MAJOR > 0
  if(layer == gtk_layer_get_layer(bar))
    return;
#endif

  gtk_layer_set_layer(bar, layer);
  if(gtk_widget_is_visible(GTK_WIDGET(bar)))
  {
    gtk_widget_hide(GTK_WIDGET(bar));
    gtk_widget_show_now(GTK_WIDGET(bar));
  }
}

void bar_set_exclusive_zone ( gchar *zone, GtkWindow *bar )
{
  if(!bar || !zone)
    return;

  if(!g_ascii_strcasecmp(zone,"auto"))
    gtk_layer_auto_exclusive_zone_enable (bar);
  else
    gtk_layer_set_exclusive_zone (bar,MAX(-1,g_ascii_strtoll(zone,NULL,10)));
}

gchar *bar_get_output ( GtkWidget *widget )
{
  GdkWindow *win;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW);
  win = gtk_widget_get_window(toplevel);
  return g_object_get_data( G_OBJECT(gdk_display_get_monitor_at_window(
        gdk_window_get_display(win),win)), "xdg_name" );
}

gboolean bar_update_monitor ( GtkWindow *win )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
  gint nmon,i;
  gchar *name, *monitor;
  gboolean visible;

  if(!win)
    return FALSE;

  monitor = g_object_get_data(G_OBJECT(win),"monitor");

  gdisp = gdk_display_get_default();
  match = gdk_display_get_primary_monitor(gdisp);
  if(!match)
    match = gdk_display_get_monitor(gdisp,0);

  if(monitor)
  {
    nmon = gdk_display_get_n_monitors(gdisp);
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      name = g_object_get_data(G_OBJECT(gmon),"xdg_name");
      if(name && !g_strcmp0(name,monitor))
        match = gmon;
    }
  }

  visible = gtk_widget_get_visible(GTK_WIDGET(win));
  gtk_widget_hide(GTK_WIDGET(win));
  if(!match)
  {
    if(visible)
      g_object_set_data(G_OBJECT(bar),"visible",GINT_TO_POINTER(TRUE));
    return FALSE;
  }
  gtk_layer_set_monitor(win, match);
  if(visible || g_object_get_data(G_OBJECT(bar),"visible"))
  {
    gtk_widget_show_now(GTK_WIDGET(win));
    g_object_set_data(G_OBJECT(bar),"visible",GINT_TO_POINTER(FALSE));
  }
  return FALSE;
}

void bar_set_monitor ( gchar *mon_name, GtkWindow *bar )
{
  gchar *monitor;

  if(!bar || !mon_name)
    return;
  monitor = g_object_get_data(G_OBJECT(bar),"monitor");

  if(!monitor || g_ascii_strcasecmp(monitor, mon_name))
  {
    g_free(monitor);
    g_object_set_data(G_OBJECT(bar),"monitor", g_strdup(mon_name));
    bar_update_monitor(bar);
  }
}

void bar_monitor_added_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;

  xdg_output_new(gmon);
  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    g_idle_add((GSourceFunc)bar_update_monitor,bar);
}

void bar_monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    bar_update_monitor(bar);
}

void bar_set_size ( gchar *size, GtkWindow *bar )
{
  gdouble number;
  gchar *end;
  GdkRectangle rect;
  GdkWindow *win;
  gint toplevel_dir;

  if(!bar || !size)
    return;
  number = g_ascii_strtod(size, &end);
  win = gtk_widget_get_window(GTK_WIDGET(bar));
  gdk_monitor_get_geometry( gdk_display_get_monitor_at_window(
      gdk_window_get_display(win),win), &rect );
  toplevel_dir = bar_get_toplevel_dir(GTK_WIDGET(bar));

  if ( toplevel_dir == GTK_POS_BOTTOM || toplevel_dir == GTK_POS_TOP )
  {
    if(*end == '%')
      number = number * rect.width / 100;
    if ( number >= rect.width )
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_LEFT, TRUE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_RIGHT, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_LEFT, FALSE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_RIGHT, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar),(gint)number,-1);
    }
  }
  else
  {
    if(*end == '%')
      number = number * rect.height / 100;
    if ( number >= rect.height )
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_TOP, TRUE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_TOP, FALSE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar),-1,(gint)number);
    }
  }
}

GtkWindow *bar_new ( gchar *name )
{
  GtkWindow *win;
  GtkWidget *box;
  gint toplevel_dir;

  win = (GtkWindow *)gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (win);
  gtk_widget_set_name(GTK_WIDGET(win),name);
  gtk_layer_auto_exclusive_zone_enable ( win );
  gtk_layer_set_keyboard_interactivity(win,FALSE);
  gtk_layer_set_layer(win,GTK_LAYER_SHELL_LAYER_TOP);

  gtk_widget_style_get(GTK_WIDGET(win),"direction",&toplevel_dir,NULL);
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  if(toplevel_dir == GTK_POS_LEFT || toplevel_dir == GTK_POS_RIGHT)
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  else
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  gtk_container_add(GTK_CONTAINER(win), box);

  if(!bar_list)
    bar_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  g_hash_table_insert(bar_list, g_strdup(name), win);

  return win;
}
