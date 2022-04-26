/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

static GtkWindow *bar_window;
static gint toplevel_dir;
static gint exclusive_zone = -2;
static GtkLayerShellLayer layer = GTK_LAYER_SHELL_LAYER_TOP;

gboolean bar_hide_event ( struct json_object *obj )
{
  gchar *mode, state;
  struct json_object *visible;
  static gchar pstate = 's';

  if ( obj )
  {
    mode = json_string_by_name(obj,"mode");
    if(mode)
    {
      pstate = *mode;
      g_free(mode);
    }
    state = pstate;

    if(json_object_object_get_ex(obj,"visible_by_modifier",&visible))
      if(json_object_get_boolean(visible))
        state = 's';
  }
  else
    if( gtk_widget_is_visible (GTK_WIDGET(bar_window)) )
      pstate = state = 'h';
    else
      pstate = state = 's';

  if(state=='h')
    gtk_widget_hide(GTK_WIDGET(bar_window));
  else
    if(!gtk_widget_is_visible(GTK_WIDGET(bar_window)))
      bar_update_monitor(bar_window);
  return TRUE;
}

gint bar_get_toplevel_dir ( void )
{
  return toplevel_dir;
}

void bar_set_layer ( gchar *layer_str )
{
  if(!g_ascii_strcasecmp(layer_str,"background"))
    layer = GTK_LAYER_SHELL_LAYER_BACKGROUND;
  if(!g_ascii_strcasecmp(layer_str,"bottom"))
    layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
  if(!g_ascii_strcasecmp(layer_str,"top"))
    layer = GTK_LAYER_SHELL_LAYER_TOP;
  if(!g_ascii_strcasecmp(layer_str,"overlay"))
    layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
  gtk_layer_set_layer(bar_window,layer);
}

void bar_set_exclusive_zone ( gchar *zone )
{
  if(!g_ascii_strcasecmp(zone,"auto"))
  {
    exclusive_zone = -2;
    gtk_layer_auto_exclusive_zone_enable ( bar_window );
  }
  else
  {
    exclusive_zone = MAX(-1,g_ascii_strtoll(zone,NULL,10));
    gtk_layer_set_exclusive_zone ( bar_window, exclusive_zone );
  }
}

gchar *bar_get_output ( void )
{
  GdkWindow *win;

  win = gtk_widget_get_window(GTK_WIDGET(bar_window));
  return gdk_monitor_get_xdg_name( gdk_display_get_monitor_at_window(
        gdk_window_get_display(win),win) );
}

void bar_update_monitor ( GtkWindow *win )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
  GtkWidget *g;
  gint nmon,i;
  gchar *name, *monitor;

  if(!win)
    return;

  monitor = g_object_get_data(G_OBJECT(win),"monitor");

  gdisp = gdk_display_get_default();
  match = gdk_display_get_primary_monitor(gdisp);

  if(monitor)
  {
    nmon = gdk_display_get_n_monitors(gdisp);
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      name = gdk_monitor_get_xdg_name(gmon);
      if(!g_strcmp0(name,monitor))
        match = gmon;
      g_free(name);
    }
  }

  g = gtk_bin_get_child(GTK_BIN(bar_window));
  g_object_ref(g);
  gtk_container_remove(GTK_CONTAINER(bar_window),g);
  gtk_window_close(bar_window);
  bar_window = bar_new(NULL);
  gtk_layer_set_monitor(bar_window, match);
  gtk_container_add(GTK_CONTAINER(bar_window),g);
  gtk_widget_show_all ((GtkWidget *)bar_window);
  wayland_reset_inhibitors(g,NULL);
  g_object_unref(g);
}

void bar_set_monitor ( gchar *mon_name )
{
  gchar *monitor;

  monitor = g_object_get_data(G_OBJECT(bar_window),"monitor");

  if(!monitor || g_ascii_strcasecmp(monitor, mon_name))
  {
    g_free(monitor);
    monitor = g_strdup(mon_name);
    g_object_set_data(G_OBJECT(bar_window),"monitor",monitor);
  }

  bar_update_monitor(bar_window);
}

void bar_monitor_change_cb ( void )
{
  bar_update_monitor(bar_window);
}

void bar_set_size ( gchar *size )
{
  gdouble number;
  gchar *end;
  GdkRectangle rect;
  GdkWindow *win;

  if(!bar_window)
    return;

  number = g_ascii_strtod(size, &end);
  win = gtk_widget_get_window(GTK_WIDGET(bar_window));
  gdk_monitor_get_geometry( gdk_display_get_monitor_at_window(
      gdk_window_get_display(win),win), &rect );

  if ( toplevel_dir == GTK_POS_BOTTOM || toplevel_dir == GTK_POS_TOP )
  {
    if(*end == '%')
      number = number * rect.width / 100;
    if ( number >= rect.width )
    {
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_LEFT, TRUE );
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_RIGHT, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_LEFT, FALSE );
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_RIGHT, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar_window),(gint)number,-1);
    }
  }
  else
  {
    if(*end == '%')
      number = number * rect.height / 100;
    if ( number >= rect.height )
    {
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_TOP, TRUE );
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_TOP, FALSE );
      gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar_window),-1,(gint)number);
    }
  }
}

GtkWindow *bar_new ( GtkApplication *app )
{
  GtkWindow *win;
  static GtkApplication *napp;

  if(app)
    napp = app;

  if(!napp)
    return NULL;

  win = (GtkWindow *)gtk_application_window_new (napp);
  gtk_widget_set_name(GTK_WIDGET(win),"sfwbar");
  gtk_layer_init_for_window (win);
  if( exclusive_zone < -1 )
    gtk_layer_auto_exclusive_zone_enable ( win );
  else
    gtk_layer_set_exclusive_zone ( win, exclusive_zone );
  gtk_layer_set_keyboard_interactivity(win,FALSE);
  gtk_layer_set_layer(win,layer);

  gtk_widget_style_get(GTK_WIDGET(win),"direction",&toplevel_dir,NULL);
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  bar_window = win;

  return win;
}

