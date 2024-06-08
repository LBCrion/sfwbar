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
#include "taskbarshell.h"
#include "window.h"
#include "meson.h"

G_DEFINE_TYPE_WITH_CODE (Bar, bar, GTK_TYPE_WINDOW, G_ADD_PRIVATE (Bar))

static GHashTable *bar_list;
extern GtkApplication *application;

static gboolean bar_sensor_unblock_cb ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self), FALSE);
  priv = bar_get_instance_private(BAR(self));
  priv->sensor_block = FALSE;

  return FALSE;
}

static gboolean bar_sensor_hide ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(gtk_bin_get_child(GTK_BIN(self))==priv->sensor)
    return FALSE;

  if(window_ref_check(self))
    return TRUE;
  priv->sensor_block = TRUE;
  g_idle_add((GSourceFunc)bar_sensor_unblock_cb, self);
  css_add_class(self, "sensor");
  gtk_container_remove(GTK_CONTAINER(self), gtk_bin_get_child(GTK_BIN(self)));
  gtk_container_add(GTK_CONTAINER(self), priv->sensor);
  priv->sensor_state = FALSE;
  priv->sensor_handle = 0;

  return FALSE;
}

static gboolean bar_leave_notify_event ( GtkWidget *self,
    GdkEventCrossing *event )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(event->detail==GDK_NOTIFY_INFERIOR)
    return TRUE;

  if(!priv->sensor_timeout || !priv->sensor_state || priv->sensor_block)
    return TRUE;

  if(!priv->sensor_handle)
    priv->sensor_handle = g_timeout_add(priv->sensor_timeout,
        (GSourceFunc)bar_sensor_hide, self);

  return TRUE;
}

static void bar_sensor_show_bar ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  bar_sensor_cancel_hide(self);
  priv->sensor_state = TRUE;

  if(gtk_bin_get_child(GTK_BIN(self)) != priv->sensor)
    return;

  priv->sensor_block = TRUE;
  g_idle_add((GSourceFunc)bar_sensor_unblock_cb, self);
  css_remove_class(self,"sensor");
  gtk_container_remove(GTK_CONTAINER(self), gtk_bin_get_child(GTK_BIN(self)));
  gtk_widget_show(priv->box);
  gtk_container_add(GTK_CONTAINER(self), priv->box);
}

static gboolean bar_enter_notify_event ( GtkWidget *self,
    GdkEventCrossing *event )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(!priv->sensor_timeout || priv->sensor_block)
    return TRUE;

  bar_sensor_show_bar(self);
  return TRUE;
}

static void bar_destroy ( GtkWidget *self )
{
  BarPrivate *priv, *ppriv;

  priv = bar_get_instance_private(BAR(self));
  if(priv->mirror_parent)
  {
    ppriv = bar_get_instance_private(BAR(priv->mirror_parent));
    ppriv->mirror_children = g_list_remove(ppriv->mirror_children, self);
    priv->mirror_parent = NULL;
  }
  g_clear_pointer(&priv->name, g_free);
  g_clear_pointer(&priv->output, g_free);
  g_clear_pointer(&priv->bar_id, g_free);
  g_clear_pointer(&priv->size, g_free);
  g_clear_pointer(&priv->ezone, g_free);
  g_clear_pointer(&priv->layer, g_free);
  g_list_free_full(g_steal_pointer(&priv->mirror_targets), g_free);
  g_clear_pointer(&priv->sensor, gtk_widget_destroy);
  g_clear_pointer(&priv->box, gtk_widget_destroy);
  GTK_WIDGET_CLASS(bar_parent_class)->destroy(self);
}

static void bar_style_updated ( GtkWidget *self )
{
  BarPrivate *priv;
  GtkAlign halign, valign;
  GdkRectangle rect;
  gint dir, size;
  gboolean top, left, right, bottom, full_size;
  gchar *end;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  GTK_WIDGET_CLASS(bar_parent_class)->style_updated(self);

  gtk_widget_style_get(self,"halign",&halign,NULL);
  gtk_widget_style_get(self,"valign",&valign,NULL);
  gtk_widget_style_get(self,"direction",&dir,NULL);

  if(!priv->size || !priv->current_monitor)
    full_size = TRUE;
  else
  {
    gdk_monitor_get_geometry(priv->current_monitor, &rect);
    size = g_ascii_strtod(priv->size, &end);
    if(*end=='%')
    {
      if(dir==GTK_POS_TOP || dir==GTK_POS_BOTTOM)
       size *= (gdouble)rect.width/100;
      else
        size *= (gdouble)rect.height/100;
    }

    if(dir==GTK_POS_TOP || dir==GTK_POS_BOTTOM)
      full_size = size >= rect.width;
    else
      full_size = size >= rect.height;
  }

  if(!full_size)
  {
    if(dir==GTK_POS_TOP || dir==GTK_POS_BOTTOM)
      gtk_widget_set_size_request(self, (gint)size, -1);
    else
      gtk_widget_set_size_request(self, -1, (gint)size);
  }

  if(priv->dir == dir && priv->halign == halign && priv->valign == valign &&
      priv->full_size == full_size)
    return;

  top = (dir == GTK_POS_TOP ||
      ((dir == GTK_POS_LEFT || dir == GTK_POS_RIGHT) &&
       (full_size || valign == GTK_ALIGN_START)));
  bottom = (dir == GTK_POS_BOTTOM ||
      ((dir == GTK_POS_LEFT || dir == GTK_POS_RIGHT) &&
       (full_size || valign == GTK_ALIGN_END)));
  left = (dir == GTK_POS_LEFT ||
      ((dir == GTK_POS_TOP || dir == GTK_POS_BOTTOM) &&
       (full_size || halign == GTK_ALIGN_START)));
  right = (dir == GTK_POS_RIGHT ||
      ((dir == GTK_POS_TOP || dir == GTK_POS_BOTTOM) &&
       (full_size || halign == GTK_ALIGN_END)));

  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_TOP, top);
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_LEFT, left);
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_RIGHT, right);
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_BOTTOM, bottom);

  if(priv->dir != dir)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(priv->box),
        (dir==GTK_POS_TOP || dir==GTK_POS_BOTTOM)?
        GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);

  priv->dir = dir;
  priv->halign = halign;
  priv->valign = valign;
  priv->full_size = full_size;

  g_return_if_fail(IS_BAR(self));
}

static void bar_map ( GtkWidget *self )
{
  GTK_WIDGET_CLASS(bar_parent_class)->map(self);
  bar_style_updated(self);
}

static void bar_init ( Bar *self )
{
}

static void bar_class_init ( BarClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = bar_destroy;
  GTK_WIDGET_CLASS(kclass)->enter_notify_event = bar_enter_notify_event;
  GTK_WIDGET_CLASS(kclass)->leave_notify_event = bar_leave_notify_event;
  GTK_WIDGET_CLASS(kclass)->style_updated = bar_style_updated;
  GTK_WIDGET_CLASS(kclass)->map = bar_map;
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
  base_widget_set_style(widget, g_strdup("\"layout\""));

  if(grid && !g_ascii_strcasecmp(grid,"center"))
  {
    gtk_box_set_center_widget(GTK_BOX(priv->box), widget);
    priv->center = widget;
  }
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
  {
    gtk_box_pack_end(GTK_BOX(priv->box), widget, TRUE, TRUE, 0);
    priv->end = widget;
  }
  else
  {
    gtk_box_pack_start(GTK_BOX(priv->box), widget, TRUE, TRUE, 0);
    priv->start = widget;
  }
  return widget;
}

GHashTable *bar_get_list ( void )
{
  return bar_list;
}

gboolean bar_address_all ( GtkWidget *self, gchar *value,
    void (*bar_func)( GtkWidget *, gchar * ) )
{
  void *bar,*key;
  GHashTableIter iter;

  if(self)
    return FALSE;
  if(!bar_list)
    return TRUE;

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    bar_func(bar, value);

  return TRUE;
}

void bar_set_mirrors ( GtkWidget *self, GList *mirrors )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_list_free_full(priv->mirror_targets, g_free);
  priv->mirror_targets = mirrors;
  bar_update_monitor(self);
}

void bar_set_mirrors_old ( GtkWidget *self, gchar *mirror )
{
  GList *list = NULL;
  gchar **strv;
  gsize i;

  if( (strv = g_strsplit(mirror, ";", -1)) )
  {
    for(i=0; strv[i]; i++)
      list = g_list_append(list, strv[i]);
    g_free(strv);
    bar_set_mirrors(self, list);
  }
}

static gboolean bar_mirror_check ( GtkWidget *self, gchar *monitor )
{
  BarPrivate *priv;
  GList *iter;
  gboolean match;

  g_return_val_if_fail(IS_BAR(self), FALSE);
  priv = bar_get_instance_private(BAR(self));

  match = FALSE;
  for(iter=priv->mirror_targets; iter; iter=g_list_next(iter))
  {
    if(*((gchar *)iter->data)=='!')
    {
      if(g_pattern_match_simple(iter->data + sizeof(gchar), monitor))
        return FALSE;
    }
    else if(g_pattern_match_simple(iter->data, monitor))
      match = TRUE;
  }

  return match;
}

void bar_set_id ( GtkWidget *self, gchar *id )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->bar_id);
  priv->bar_id = g_strdup(id);

  g_list_foreach(priv->mirror_children, (GFunc)bar_set_id, id);
}

void bar_visibility_toggle_all ( gpointer d )
{
  bar_set_visibility (NULL, NULL, 't');
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
      g_hash_table_iter_init(&iter, bar_list);
      while(g_hash_table_iter_next(&iter, &key, &bar))
        bar_set_visibility(bar, id, state);
    }
    return;
  }

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(id && priv->bar_id && g_strcmp0(priv->bar_id, id))
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

  for(liter=priv->mirror_children; liter; liter=g_list_next(liter))
    bar_set_visibility(liter->data, id, state);
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

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(layer_str);
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

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(zone!=NULL);
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->ezone);
  priv->ezone = g_strdup(zone);

  if(!g_ascii_strcasecmp(zone,"auto"))
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(self));
  else
    gtk_layer_set_exclusive_zone(GTK_WINDOW(self),
        MAX(-1, g_ascii_strtoll(zone, NULL, 10)));

  g_list_foreach(priv->mirror_children, (GFunc)bar_set_exclusive_zone, zone);
}

GdkMonitor *bar_get_monitor ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),NULL);
  priv = bar_get_instance_private(BAR(self));

  return priv->current_monitor;
}

gchar *bar_get_output ( GtkWidget *self )
{
  Bar *bar;
  BarPrivate *priv;

  if( !(bar = BAR(gtk_widget_get_ancestor(self, BAR_TYPE))) )
    return NULL;
  priv = bar_get_instance_private(bar);

  if(!priv->current_monitor)
    return NULL;
  else
    return g_object_get_data( G_OBJECT(priv->current_monitor), "xdg_name");
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
    for(i=0; i<nmon; i++)
    {
      gmon = gdk_display_get_monitor(gdisp, i);
      output = g_object_get_data(G_OBJECT(gmon), "xdg_name");
      if(output && !g_strcmp0(output, priv->output))
        match = gmon;
    }

  gtk_widget_hide(self);
  priv->current_monitor = match;
  if(match)
  {
    gtk_layer_set_monitor(GTK_WINDOW(self), match);
    if(priv->visible)
    {
      gtk_widget_show_now(self);
      taskbar_shell_invalidate_all();
    }
  }

  /* remove any mirrors from new primary output */
  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    if(bar_get_monitor(iter->data) == priv->current_monitor)
      gtk_widget_destroy(iter->data);

  /* add mirrors to any outputs where they are missing */
  for(i=0; i<nmon; i++)
  {
    gmon = gdk_display_get_monitor(gdisp, i);
    output = g_object_get_data(G_OBJECT(gmon),"xdg_name");
    present = FALSE;
    for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
      if(bar_get_monitor(iter->data) == gmon)
        present = TRUE;

    if(!present && gmon != priv->current_monitor &&
        bar_mirror_check(self, output) )
      bar_mirror(self, gmon);
  }

  return FALSE;
}

void bar_set_monitor ( GtkWidget *self, gchar *monitor )
{
  BarPrivate *priv;
  gchar *mon_name;

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
  if(bar_list)
  {
    g_hash_table_iter_init(&iter,bar_list);
    while(g_hash_table_iter_next(&iter, &key, &bar))
      g_idle_add((GSourceFunc)bar_update_monitor, bar);
  }

  g_snprintf(trigger, 255, "%s_connected",
      (gchar *)g_object_get_data(G_OBJECT(gmon), "xdg_name"));
  g_idle_add((GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_string(trigger));
}

void bar_monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  BarPrivate *priv;
  GHashTableIter iter;
  void *key, *bar;
  GList *liter;
  static char trigger[256];

  if(bar_list)
  {
    g_hash_table_iter_init(&iter,bar_list);
    while(g_hash_table_iter_next(&iter,&key,&bar))
    {
      priv = bar_get_instance_private(BAR(bar));
      for(liter=priv->mirror_children; liter; liter=g_list_next(liter))
        if(bar_get_monitor(liter->data) == gmon)
        {
          gtk_widget_destroy(liter->data);
          break;
        }
      bar_update_monitor(bar);
    }
  }

  g_snprintf(trigger,255,"%s_disconnected",
      (gchar *)g_object_get_data(G_OBJECT(gmon),"xdg_name"));
  g_idle_add((GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_string(trigger));
}

void bar_set_margin ( GtkWidget *self, gint64 margin )
{
  BarPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  priv->margin = margin;
#if GTK_LAYER_VER_MINOR > 5 || GTK_LAYER_VER_MAJOR > 0
  gtk_layer_set_margin(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_TOP, margin);
  gtk_layer_set_margin(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_BOTTOM, margin);
  gtk_layer_set_margin(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_LEFT, margin);
  gtk_layer_set_margin(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_RIGHT, margin);
#endif

  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    bar_set_margin(iter->data, margin);
}

void bar_set_size ( GtkWidget *self, gchar *size )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(size);
  priv = bar_get_instance_private(BAR(self));
  g_free(priv->size);
  priv->size = g_strdup(size);
  bar_style_updated(self);

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_size, size);
}

void bar_sensor_cancel_hide( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(priv->sensor_handle)
  {
    g_source_remove(priv->sensor_handle);
    priv->sensor_handle = 0;
  }
}

void bar_set_sensor ( GtkWidget *self, gint64 timeout )
{
  BarPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  priv->sensor_timeout = timeout;

  if(priv->sensor_timeout)
  {
    if(!priv->sensor)
    {
      priv->sensor = gtk_grid_new();
      g_object_ref_sink(priv->sensor);
      css_add_class(priv->sensor,"sensor");
      gtk_widget_add_events(priv->sensor, GDK_STRUCTURE_MASK);
      gtk_widget_add_events(priv->box, GDK_STRUCTURE_MASK);
      gtk_widget_show(priv->sensor);
    }
    bar_sensor_hide(self);
    priv->sensor_block = FALSE;
  }
  else if(priv->sensor_handle)
    bar_sensor_show_bar(self);

  for(iter = priv->mirror_children; iter; iter=g_list_next(iter))
    bar_set_sensor(iter->data, timeout);
}

GtkWidget *bar_new ( gchar *name )
{
  GtkWidget *self;
  BarPrivate *priv;

  self = GTK_WIDGET(g_object_new(bar_get_type(), NULL));
  gtk_application_add_window(application, GTK_WINDOW(self));
  priv = bar_get_instance_private(BAR(self));
  priv->name = g_strdup(name);
  priv->visible = TRUE;
  priv->current_monitor = wayland_monitor_get_default();
  priv->output = g_strdup(
      g_object_get_data(G_OBJECT(priv->current_monitor), "xdg_name"));
  priv->dir = -1;
  priv->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_ref_sink(priv->box);
  gtk_container_add(GTK_CONTAINER(self), priv->box);
  g_object_set_data(G_OBJECT(priv->box), "parent_window", self);
  gtk_layer_init_for_window(GTK_WINDOW(self));
  gtk_widget_set_name(self, name);
  gtk_layer_auto_exclusive_zone_enable (GTK_WINDOW(self));
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self), FALSE);
  gtk_layer_set_layer(GTK_WINDOW(self), GTK_LAYER_SHELL_LAYER_TOP);
  gtk_layer_set_monitor(GTK_WINDOW(self), priv->current_monitor);
  bar_style_updated(self);

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

  g_return_val_if_fail(IS_BAR(src),NULL);
  self = bar_new(NULL);
  spriv = bar_get_instance_private(BAR(src));
  dpriv = bar_get_instance_private(BAR(self));

  gtk_widget_set_name(self, gtk_widget_get_name(src));
  if(spriv->start)
  {
    dpriv->start = base_widget_mirror(spriv->start);
    gtk_box_pack_start(GTK_BOX(dpriv->box), dpriv->start, TRUE, TRUE, 0);
  }
  if(spriv->center)
  {
    dpriv->center = base_widget_mirror(spriv->center);
    gtk_box_set_center_widget(GTK_BOX(dpriv->box), dpriv->center);
  }
  if(spriv->end)
  {
    dpriv->end = base_widget_mirror(spriv->end);
    gtk_box_pack_end(GTK_BOX(dpriv->box), dpriv->end, TRUE, TRUE, 0);
  }

  dpriv->visible = spriv->visible;
  dpriv->hidden = spriv->hidden;
  dpriv->jump = spriv->jump;
  dpriv->bar_id = g_strdup(spriv->bar_id);
  spriv->mirror_children = g_list_prepend(spriv->mirror_children, self);
  dpriv->mirror_parent = src;
  dpriv->current_monitor = monitor;
  dpriv->output = g_strdup(g_object_get_data(G_OBJECT(monitor), "xdg_name"));

  bar_set_sensor(self, spriv->sensor_timeout);

//  gtk_application_add_window(app,GTK_WINDOW(self));
  gtk_layer_set_monitor(GTK_WINDOW(self), monitor);
  gtk_widget_show(self);
  css_widget_cascade(self, NULL);
  if(spriv->size)
    bar_set_size(self, spriv->size);
  if(spriv->margin)
    bar_set_margin(self, spriv->margin);
  if(spriv->layer)
    bar_set_layer(self, spriv->layer);
  if(spriv->ezone)
    bar_set_exclusive_zone(self, spriv->ezone);
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

void bar_set_icon_theme ( gchar *new_theme )
{
  GtkSettings *setts;

  setts = gtk_settings_get_default();
  g_object_set(G_OBJECT(setts),"gtk-icon-theme-name", new_theme, NULL);
  g_free(new_theme);
}
