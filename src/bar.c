/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"
#include "bar.h"
#include "wayland.h"
#include "grid.h"
#include "config.h"
#include "taskbar.h"

G_DEFINE_TYPE_WITH_CODE (Bar, bar, GTK_TYPE_WINDOW, G_ADD_PRIVATE (Bar));

static GHashTable *bar_list;
static GList *mirrors;

static void bar_destroy ( GtkWidget *self )
{
  BarPrivate *priv;

  priv = bar_get_instance_private(BAR(self));
  mirrors = g_list_remove(mirrors, self);
  g_clear_pointer(&priv->name,g_free);
  g_clear_pointer(&priv->output,g_free);
  g_clear_pointer(&priv->bar_id,g_free);
  g_clear_pointer(&priv->size,g_free);
  g_clear_pointer(&priv->ezone,g_free);
  g_clear_pointer(&priv->layer,g_free);
  g_clear_pointer(&priv->mirror_targets,g_strfreev);
  g_clear_pointer(&priv->mirror_blocks,g_strfreev);
  g_clear_pointer(&priv->sensor,gtk_widget_destroy);
  g_clear_pointer(&priv->box,gtk_widget_destroy);
  g_list_free(g_steal_pointer(&priv->sensor_refs));
  GTK_WIDGET_CLASS(bar_parent_class)->destroy(self);
}

static void bar_init ( Bar *self )
{
}

static void bar_class_init ( BarClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = bar_destroy;
}

GtkWidget *bar_from_name ( gchar *name )
{
  if(!bar_list)
    return NULL;

  return g_hash_table_lookup(bar_list,name?name:"sfwbar");
}

GtkWidget *bar_grid_from_name ( gchar *addr )
{
  GtkWidget *bar;
  BarPrivate *priv;
  GtkWidget *widget = NULL;
  gchar *grid, *ptr, *name;

  if(!addr)
    addr = "sfwbar";

  if(!g_ascii_strcasecmp(addr,"*"))
    return NULL;

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
  if(!g_ascii_strcasecmp(name,"*"))
  {
    g_message(
        "invalid bar name '*' in grid address %s, defaulting to 'sfwbar'",
        addr);
    g_free(name);
    name = g_strdup("sfwbar");
  }

  bar = bar_from_name(name);
  if(!bar)
    bar = bar_new(name);
  g_free(name);
  priv = bar_get_instance_private(BAR(bar));

  if(grid && !g_ascii_strcasecmp(grid,"center"))
    widget = priv->center;
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
    widget = priv->end;
  else
    widget = priv->start;

  if(widget)
    return widget;

  widget = grid_new();
  gtk_widget_set_name(base_widget_get_child(widget),"layout");

  if(grid && !g_ascii_strcasecmp(grid,"center"))
  {
    gtk_box_set_center_widget(GTK_BOX(priv->box),widget);
    priv->center = widget;
  }
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
  {
    gtk_box_pack_end(GTK_BOX(priv->box),widget,TRUE,TRUE,0);
    priv->end = widget;
  }
  else
  {
    gtk_box_pack_start(GTK_BOX(priv->box),widget,TRUE,TRUE,0);
    priv->start = widget;
  }
  return widget;
}

gboolean bar_address_all ( GtkWidget *self, gchar *value,
    void (*bar_func)( GtkWidget *, gchar * ) )
{
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_mirrors(bar, value);
    }
    return TRUE;
  }
  return FALSE;
}

void bar_set_mirrors ( GtkWidget *self, gchar *mirror )
{
  BarPrivate *priv;

  if(bar_address_all(self, mirror, bar_set_mirrors))
    return;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_strfreev(priv->mirror_targets);
  priv->mirror_targets = g_strsplit(mirror,";",-1);
  bar_update_monitor(self);
}

void bar_set_mirror_blocks ( GtkWidget *self, gchar *mirror )
{
  BarPrivate *priv;

  if(bar_address_all(self, mirror, bar_set_mirror_blocks))
    return;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_strfreev(priv->mirror_blocks);
  priv->mirror_blocks = g_strsplit(mirror,";",-1);
  bar_update_monitor(self);
}

void bar_set_id ( GtkWidget *self, gchar *id )
{
  BarPrivate *priv;


  if(bar_address_all(self, id, bar_set_id))
    return;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->bar_id);
  priv->bar_id = g_strdup(id);

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_id,id);
}

void bar_visibility_toggle_all ( gpointer d )
{
  bar_set_visibility ( NULL, NULL, 't' );
}

void bar_set_visibility ( GtkWidget *self, const gchar *id, gchar state )
{
  BarPrivate *priv;
  gboolean visible;
  void *bar,*key;
  GHashTableIter iter;
  GList *liter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_visibility(bar, id, state);
    }
    return;
  }

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(id && priv->bar_id && g_strcmp0(priv->bar_id,id))
    return;

  if(state == 't')
    priv->visible = !priv->visible;
  else if(state == 'h')
    priv->visible = FALSE;
  else if(state != 'x' && state != 'v')
    priv->visible = TRUE;

  visible = priv->visible || (state == 'v');
  if(!visible && gtk_widget_is_visible(self))
    gtk_widget_hide(self);
  else if(!gtk_widget_is_visible(self))
  {
    bar_update_monitor(self);
    gtk_widget_show_now(self);
  }

  for(liter=priv->mirror_children;liter;liter=g_list_next(liter))
    bar_set_visibility(liter->data,id,state);
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

void bar_set_layer ( GtkWidget *self, gchar *layer_str )
{
  BarPrivate *priv;
  GtkLayerShellLayer layer;

  if(bar_address_all(self, layer_str, bar_set_layer))
    return;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(layer_str!=NULL);
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->layer);
  priv->layer = g_strdup(layer_str);

  if(!g_ascii_strcasecmp(layer_str,"background"))
    layer = GTK_LAYER_SHELL_LAYER_BACKGROUND;
  else if(!g_ascii_strcasecmp(layer_str,"bottom"))
    layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
  else if(!g_ascii_strcasecmp(layer_str,"overlay"))
    layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
  else
    layer = GTK_LAYER_SHELL_LAYER_TOP;

#if GTK_LAYER_VER_MINOR > 5 || GTK_LAYER_VER_MAJOR > 0
  if(layer == gtk_layer_get_layer(GTK_WINDOW(self)))
    return;
#endif

  gtk_layer_set_layer(GTK_WINDOW(self), layer);
  if(gtk_widget_is_visible(self))
  {
    gtk_widget_hide(self);
    gtk_widget_show_now(self);
  }

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_layer,layer_str);
}

void bar_set_exclusive_zone ( GtkWidget *self, gchar *zone )
{
  BarPrivate *priv;

  if(bar_address_all(self, zone, bar_set_exclusive_zone))
    return;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(zone!=NULL);
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->ezone);
  priv->ezone = g_strdup(zone);

  if(!g_ascii_strcasecmp(zone,"auto"))
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(self));
  else
    gtk_layer_set_exclusive_zone(GTK_WINDOW(self),
        MAX(-1,g_ascii_strtoll(zone,NULL,10)));

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_exclusive_zone,zone);
}

GdkMonitor *bar_get_monitor ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),NULL);
  priv = bar_get_instance_private(BAR(self));

  return priv->current_monitor;
}

gchar *bar_get_output ( GtkWidget *widget )
{
  BarPrivate *priv;

  priv = bar_get_instance_private(
      BAR(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW)));

  return g_object_get_data( G_OBJECT(priv->current_monitor),"xdg_name");
}

gboolean bar_update_monitor ( GtkWidget *self )
{
  BarPrivate *priv;
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
  GList *iter;
  gint nmon, i;
  gchar *output;
  gboolean present;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(!xdg_output_check() )
    return TRUE;

  gdisp = gdk_display_get_default();
  if(priv->jump)
  {
    match = gdk_display_get_primary_monitor(gdisp);
    if(!match)
      match = gdk_display_get_monitor(gdisp,0);
  }
  else
    match = NULL;

  nmon = gdk_display_get_n_monitors(gdisp);
  if(priv->output)
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      output = g_object_get_data(G_OBJECT(gmon),"xdg_name");
      if(output && !g_strcmp0(output,priv->output))
        match = gmon;
    }

  if(match)
  {
    gtk_widget_hide(self);
    priv->current_monitor = match;
    gtk_layer_set_monitor(GTK_WINDOW(self), match);
    if(priv->visible)
    {
      gtk_widget_show_now(self);
      taskbar_invalidate_conditional();
    }
  }

  /* remove any mirrors from new primary output */
  for(iter=priv->mirror_children;iter;iter=g_list_next(iter))
    if(bar_get_monitor(iter->data) == priv->current_monitor)
      bar_destroy(iter->data);

  /* add mirrors to any outputs where they are missing */
  for(i=0;i<nmon;i++)
  {
    gmon = gdk_display_get_monitor(gdisp,i);
    output = g_object_get_data(G_OBJECT(gmon),"xdg_name");
    present = FALSE;
    for(iter=priv->mirror_children;iter;iter=g_list_next(iter))
      if(bar_get_monitor(iter->data) == gmon)
        present = TRUE;

    if(!present && gmon != priv->current_monitor &&
        pattern_match(priv->mirror_targets,output) &&
        !pattern_match(priv->mirror_blocks,output) )
      bar_mirror(self, gmon);
  }

  return FALSE;
}

void bar_set_monitor ( GtkWidget *self, gchar *monitor )
{
  BarPrivate *priv;
  gchar *mon_name;

  if(bar_address_all(self, monitor, bar_set_monitor))
    return;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(monitor!=NULL);
  priv = bar_get_instance_private(BAR(self));

  if(!g_ascii_strncasecmp(monitor,"static:",7))
  {
    priv->jump = FALSE;
    mon_name = monitor+7;
  }
  else
  {
    priv->jump = TRUE;
    mon_name = monitor;
  }

  if(!priv->output || g_ascii_strcasecmp(priv->output, mon_name))
  {
    g_free(priv->output);
    priv->output =  g_strdup(mon_name);
    bar_update_monitor(self);
  }
}

void bar_monitor_added_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;
  char trigger[256];

  xdg_output_new(gmon);
  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    g_idle_add((GSourceFunc)bar_update_monitor,bar);

  g_snprintf(trigger,255,"%s_connected",
      (gchar *)g_object_get_data(G_OBJECT(gmon),"xdg_name"));
  g_idle_add((GSourceFunc)base_widget_emit_trigger,trigger);
}

void bar_monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;
  char trigger[256];

  g_hash_table_iter_init(&iter,bar_list);
  g_snprintf(trigger,255,"%s_disconnected",
      (gchar *)g_object_get_data(G_OBJECT(gmon),"xdg_name"));
  g_idle_add((GSourceFunc)base_widget_emit_trigger,trigger);

  while(g_hash_table_iter_next(&iter,&key,&bar))
    bar_update_monitor(bar);
}

void bar_set_size ( GtkWidget *self, gchar *size )
{
  BarPrivate *priv;
  gdouble number;
  gchar *end;
  GdkRectangle rect;
  GdkWindow *win;
  gint toplevel_dir;

  if(bar_address_all(self, size, bar_set_size))
    return;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(size!=NULL);
  priv = bar_get_instance_private(BAR(self));
  g_free(priv->size);
  priv->size = g_strdup(size);

  number = g_ascii_strtod(size, &end);
  win = gtk_widget_get_window(self);
  gdk_monitor_get_geometry( gdk_display_get_monitor_at_window(
      gdk_window_get_display(win),win), &rect );
  toplevel_dir = bar_get_toplevel_dir(self);

  if ( toplevel_dir == GTK_POS_BOTTOM || toplevel_dir == GTK_POS_TOP )
  {
    if(*end == '%')
      number = number * rect.width / 100;
    if ( number >= rect.width )
    {
      gtk_layer_set_anchor(GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
      gtk_layer_set_anchor(GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_RIGHT,TRUE);
    }
    else
    {
      gtk_layer_set_anchor(GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_LEFT, FALSE );
      gtk_layer_set_anchor(GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_RIGHT, FALSE );
      gtk_widget_set_size_request(self,(gint)number,-1);
    }
  }
  else
  {
    if(*end == '%')
      number = number * rect.height / 100;
    if ( number >= rect.height )
    {
      gtk_layer_set_anchor (GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_TOP, TRUE );
      gtk_layer_set_anchor (GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_TOP, FALSE );
      gtk_layer_set_anchor (GTK_WINDOW(self),
          GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE );
      gtk_widget_set_size_request(self,-1,(gint)number);
    }
  }

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_size,size);
}

void bar_unref ( GtkWidget *child, GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(g_list_find(priv->sensor_refs,child))
    priv->sensor_refs = g_list_remove(priv->sensor_refs,child);
}

void bar_ref ( GtkWidget *self, GtkWidget *child )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(!g_list_find(priv->sensor_refs,child))
    priv->sensor_refs = g_list_prepend(priv->sensor_refs,child);
  g_signal_connect(G_OBJECT(child),"unmap",G_CALLBACK(bar_unref),self);
}

static gboolean bar_sensor_enter_cb ( GtkWidget *,GdkEventCrossing *,gpointer);
static gboolean bar_sensor_leave_cb ( GtkWidget *,GdkEventCrossing *,gpointer);

static gboolean bar_sensor_hide_cb ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(gtk_bin_get_child(GTK_BIN(self))==priv->sensor)
    return FALSE;

  if(priv->sensor_refs)
    return TRUE;
  css_add_class(self,"sensor");
  gtk_container_remove(GTK_CONTAINER(self),priv->box);
  gtk_container_add(GTK_CONTAINER(self),priv->sensor);
  if(g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
        G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_leave_cb, self))
    g_signal_handler_block(self,priv->sensor_hleave);
  if(!g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
        G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_enter_cb, self))
    g_signal_handler_unblock(self,priv->sensor_henter);
  priv->sensor_handle = 0;

  return FALSE;
}

static gboolean bar_sensor_leave_cb ( GtkWidget *widget,
    GdkEventCrossing *event, gpointer self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(!priv->sensor_handle)
    priv->sensor_handle = g_timeout_add(priv->sensor_timeout,
        (GSourceFunc)bar_sensor_hide_cb, self);

  return TRUE;
}

static gboolean bar_sensor_enter_flip_cb ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
        G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_enter_cb, self))
    g_signal_handler_block(self,priv->sensor_henter);
  if(!g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
        G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_leave_cb, self))
    g_signal_handler_unblock(self,priv->sensor_hleave);

  return FALSE;
}

static void bar_sensor_show_bar ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(priv->sensor_handle)
  {
    g_source_remove(priv->sensor_handle);
    priv->sensor_handle = 0;
  }
  if(gtk_bin_get_child(GTK_BIN(self))==priv->box)
    return;
  css_remove_class(self,"sensor");
  gtk_container_remove(GTK_CONTAINER(self),priv->sensor);
  gtk_container_add(GTK_CONTAINER(self),priv->box);
}

static gboolean bar_sensor_enter_cb ( GtkWidget *widget,
    GdkEventCrossing *event, gpointer self )
{

  bar_sensor_show_bar(self);
  g_idle_add((GSourceFunc)bar_sensor_enter_flip_cb, self);

  return TRUE;
}

void bar_set_sensor ( GtkWidget *self, gchar *delay_str )
{
  BarPrivate *priv;

  if(bar_address_all(self, delay_str, bar_set_sensor))
    return;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  priv->sensor_timeout = g_ascii_strtoll(delay_str,NULL,10);

  if(priv->sensor_timeout)
  {
    if(!priv->sensor)
    {
      priv->sensor = gtk_grid_new();
      g_object_ref_sink(priv->sensor);
      css_add_class(priv->sensor,"sensor");
      gtk_widget_add_events(GTK_WIDGET(priv->sensor),GDK_ENTER_NOTIFY_MASK);
      gtk_widget_add_events(GTK_WIDGET(priv->sensor),GDK_LEAVE_NOTIFY_MASK);
      priv->sensor_henter = g_signal_connect(self,"enter-notify-event",
          G_CALLBACK(bar_sensor_enter_cb),self);
      priv->sensor_hleave = g_signal_connect(self,"leave-notify-event",
          G_CALLBACK(bar_sensor_leave_cb),self);
      gtk_widget_show(priv->sensor);
    }
    bar_sensor_hide_cb(self);
  }
  else if(priv->sensor_handle)
  {
    bar_sensor_show_bar(self);
    if(g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
          G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_enter_cb, self))
      g_signal_handler_block(self,priv->sensor_henter);
    if(g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA |
          G_SIGNAL_MATCH_UNBLOCKED, 0, 0, NULL, bar_sensor_leave_cb, self))
      g_signal_handler_block(self,priv->sensor_hleave);
  }

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_sensor,delay_str);
}

void bar_handle_direction ( GtkWidget *self )
{
  BarPrivate *priv;
  gint toplevel_dir;

  if(!IS_BAR(self))
    return;
  priv = bar_get_instance_private(BAR(self));

  gtk_widget_style_get(self,"direction",&toplevel_dir,NULL);

  if(priv->dir == toplevel_dir)
    return;
  priv->dir = toplevel_dir;

  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  if(priv->start)
    g_object_ref(priv->start);
  if(priv->center)
    g_object_ref(priv->center);
  if(priv->end)
    g_object_ref(priv->end);

  if(priv->box && gtk_bin_get_child(GTK_BIN(self))==priv->box)
    gtk_container_remove(GTK_CONTAINER(self),priv->box);
  if(priv->box)
    g_object_unref(priv->box);

  if(toplevel_dir == GTK_POS_LEFT || toplevel_dir == GTK_POS_RIGHT)
    priv->box = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  else
    priv->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  g_object_ref_sink(priv->box);

  if(!gtk_bin_get_child(GTK_BIN(self)))
    gtk_container_add(GTK_CONTAINER(self), priv->box);

  if(priv->start)
    gtk_box_pack_start(GTK_BOX(priv->box),priv->start,TRUE,TRUE,0);
  if(priv->center)
    gtk_box_set_center_widget(GTK_BOX(priv->box),priv->center);
  if(priv->end)
    gtk_box_pack_end(GTK_BOX(priv->box),priv->end,TRUE,TRUE,0);
}

GtkWidget *bar_new ( gchar *name )
{
  GtkWidget *self;
  BarPrivate *priv;

  self = GTK_WIDGET(g_object_new(bar_get_type(), NULL));
  priv = bar_get_instance_private(BAR(self));
  priv->name = g_strdup(name);
  priv->visible = TRUE;
  priv->current_monitor = wayland_monitor_get_default();
  priv->output = g_strdup(
      g_object_get_data(G_OBJECT(priv->current_monitor),"xdg_name"));
  priv->dir = -1;
  gtk_layer_init_for_window (GTK_WINDOW(self));
  gtk_widget_set_name(self,name);
  gtk_layer_auto_exclusive_zone_enable (GTK_WINDOW(self));
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self),FALSE);
  gtk_layer_set_layer(GTK_WINDOW(self),GTK_LAYER_SHELL_LAYER_TOP);
  gtk_layer_set_monitor(GTK_WINDOW(self),priv->current_monitor);

  bar_handle_direction(self);

  if(priv->name)
  {
    if(!bar_list)
      bar_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

    g_hash_table_insert(bar_list, priv->name, self);
  }

  return self;
}

GtkWidget *bar_mirror ( GtkWidget *src, GdkMonitor *monitor )
{
  GtkWidget *self;
  BarPrivate *spriv,*dpriv;
  gchar *tmp;

  g_return_val_if_fail(IS_BAR(src),NULL);
  self = bar_new(NULL);
  spriv = bar_get_instance_private(BAR(src));
  dpriv = bar_get_instance_private(BAR(self));

  gtk_widget_set_name(self,gtk_widget_get_name(src));
  bar_handle_direction(self);
  if(spriv->start)
  {
    dpriv->start = base_widget_mirror(spriv->start);
    gtk_box_pack_start(GTK_BOX(dpriv->box),dpriv->start,TRUE,TRUE,0);
  }
  if(spriv->center)
  {
    dpriv->center = base_widget_mirror(spriv->center);
    gtk_box_set_center_widget(GTK_BOX(dpriv->box),dpriv->center);
  }
  if(spriv->end)
  {
    dpriv->end = base_widget_mirror(spriv->end);
    gtk_box_pack_end(GTK_BOX(dpriv->box),dpriv->end,TRUE,TRUE,0);
  }

  dpriv->visible = spriv->visible;
  dpriv->hidden = spriv->hidden;
  dpriv->jump = spriv->jump;
  dpriv->bar_id = g_strdup(spriv->bar_id);
  spriv->mirror_children = g_list_prepend(spriv->mirror_children, self);
  dpriv->mirror_parent = src;
  dpriv->current_monitor = monitor;
  dpriv->output = g_strdup(g_object_get_data(G_OBJECT(monitor),"xdg_name"));
  mirrors = g_list_prepend(mirrors,self);

  tmp = g_strdup_printf("%ld",spriv->sensor_timeout);
  bar_set_sensor(self, tmp);
  g_free(tmp);

//  gtk_application_add_window(app,GTK_WINDOW(self));
  gtk_layer_set_monitor(GTK_WINDOW(self),monitor);
  gtk_widget_show(self);
  css_widget_cascade(GTK_WIDGET(self),NULL);
  if(spriv->size)
    bar_set_size(self,spriv->size);
  if(spriv->layer)
    bar_set_layer(self,spriv->layer);
  if(spriv->ezone)
    bar_set_exclusive_zone(self,spriv->ezone);
  return self;
}

void bar_set_theme ( gchar *new_theme )
{
  GtkSettings *setts;

  setts = gtk_settings_get_default();
  g_object_set(G_OBJECT(setts),"gtk-application-prefer-dark-theme",FALSE,NULL);
  g_object_set(G_OBJECT(setts),"gtk-theme-name",new_theme,NULL);
  g_free(new_theme);
}
