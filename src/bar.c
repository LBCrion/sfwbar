/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

static GtkWindow *bar_window;

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

void bar_set_layer ( gchar *layer_str )
{
  GtkLayerShellLayer layer;

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
  gint exclusive_zone;

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
  return g_object_get_data( G_OBJECT(gdk_display_get_monitor_at_window(
        gdk_window_get_display(win),win)), "xdg_name" );
}

void bar_update_monitor ( GtkWindow *win )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
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
      name = g_object_get_data(G_OBJECT(gmon),"xdg_name");
      if(name && !g_strcmp0(name,monitor))
        match = gmon;
    }
  }

  gtk_widget_hide(GTK_WIDGET(bar_window));
  gtk_layer_set_monitor(bar_window, match);
  gtk_widget_show(GTK_WIDGET(bar_window));
}

void bar_set_monitor ( gchar *mon_name )
{
  gchar *monitor;

  monitor = g_object_get_data(G_OBJECT(bar_window),"monitor");

  if(!monitor || g_ascii_strcasecmp(monitor, mon_name))
  {
    g_free(monitor);
    g_object_set_data(G_OBJECT(bar_window),"monitor", g_strdup(mon_name));
    bar_update_monitor(bar_window);
  }
}

void bar_monitor_added_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  wayland_output_new(gmon);
  bar_update_monitor(bar_window);
}

void bar_monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  wayland_output_destroy(gmon);
  bar_update_monitor(bar_window);
}

void bar_set_size ( gchar *size )
{
  gdouble number;
  gchar *end;
  GdkRectangle rect;
  GdkWindow *win;
  gint toplevel_dir;

  if(!bar_window)
    return;

  number = g_ascii_strtod(size, &end);
  win = gtk_widget_get_window(GTK_WIDGET(bar_window));
  gdk_monitor_get_geometry( gdk_display_get_monitor_at_window(
      gdk_window_get_display(win),win), &rect );
  toplevel_dir = bar_get_toplevel_dir(GTK_WIDGET(bar_window));

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
  gint toplevel_dir;

  if(app)
    napp = app;

  if(!napp)
    return NULL;

  win = (GtkWindow *)gtk_application_window_new (napp);
  gtk_widget_set_name(GTK_WIDGET(win),"sfwbar");
  gtk_layer_init_for_window (win);
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

  bar_window = win;

  return win;
}

