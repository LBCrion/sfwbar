/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "window.h"
#include "meson.h"
#include "trigger.h"
#include "gui/css.h"
#include "gui/bar.h"
#include "gui/monitor.h"
#include "gui/grid.h"
#include "gui/taskbarshell.h"
#include "util/string.h"

G_DEFINE_TYPE_WITH_CODE (Bar, bar, GTK_TYPE_WINDOW, G_ADD_PRIVATE (Bar))

static GHashTable *bar_list;
static gint bar_count;
extern GtkApplication *application;

static void bar_revealer_notify ( GtkRevealer *revealer, GParamSpec *spec,
    GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(!gtk_revealer_get_child_revealed(revealer) && !priv->visible)
    gtk_widget_hide(self);
  else if(gtk_revealer_get_child_revealed(revealer) && !priv->visible &&
      !priv->visible_by_mod)
    gtk_revealer_set_reveal_child(revealer, FALSE);
}

static void bar_reveal ( GtkWidget *self )
{
  BarPrivate *priv;
  gint dir;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  gtk_revealer_set_reveal_child(priv->revealer, FALSE);
  gtk_widget_show(self);

  gtk_widget_style_get(self, "direction", &dir, NULL);

  if(!priv->sensor_transition)
    gtk_revealer_set_transition_type(priv->revealer,
      GTK_REVEALER_TRANSITION_TYPE_NONE);
  else if(dir == GTK_POS_TOP)
    gtk_revealer_set_transition_type(priv->revealer,
      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  else if(dir == GTK_POS_BOTTOM)
    gtk_revealer_set_transition_type(priv->revealer,
      GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  else if(dir == GTK_POS_LEFT)
    gtk_revealer_set_transition_type(priv->revealer,
      GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  else if(dir == GTK_POS_RIGHT)
    gtk_revealer_set_transition_type(priv->revealer,
      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_reveal_child(priv->revealer, TRUE);
}

static gboolean bar_sensor_unblock_cb ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self), FALSE);
  priv = bar_get_instance_private(BAR(self));
  priv->sensor_block = FALSE;

  return FALSE;
}

static void bar_sensor_hide_complete ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  if(gtk_revealer_get_child_revealed(priv->revealer))
    return;

  css_add_class(self, "sensor");
  gtk_container_remove(GTK_CONTAINER(self), gtk_bin_get_child(GTK_BIN(self)));
  gtk_container_add(GTK_CONTAINER(self), priv->sensor);
  priv->sensor_state = FALSE;
  g_signal_handlers_disconnect_by_func(priv->revealer,
      bar_sensor_hide_complete, self);
}

static gboolean bar_sensor_hide ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self), FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(gtk_bin_get_child(GTK_BIN(self))==priv->sensor)
    return FALSE;

  if(window_ref_check(self))
    return TRUE;
  gtk_revealer_set_reveal_child(priv->revealer, FALSE);
  priv->sensor_block = TRUE;
  g_idle_add((GSourceFunc)bar_sensor_unblock_cb, self);
  g_signal_connect_swapped(G_OBJECT(priv->revealer),
      "notify::child-revealed", G_CALLBACK(bar_sensor_hide_complete), self);
  priv->sensor_handle = 0;

  return FALSE;
}

static void bar_sensor_unref_event ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(!priv->sensor_timeout || !priv->sensor_state || priv->sensor_block)
    return;

  if(!priv->sensor_handle)
    priv->sensor_handle = g_timeout_add(priv->sensor_timeout,
        (GSourceFunc)bar_sensor_hide, self);
}

static gboolean bar_leave_notify_event ( GtkWidget *self,
    GdkEventCrossing *event )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self), TRUE);
  priv = bar_get_instance_private(BAR(self));

  if(event->detail==GDK_NOTIFY_INFERIOR)
    return TRUE;

#if GTK_LAYER_VER_MINOR > 5
  gtk_layer_set_keyboard_mode(GTK_WINDOW(self),
      GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
#else
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self), FALSE);
#endif

  if(priv->show_handle)
  {
    g_source_remove(priv->show_handle);
    priv->show_handle = 0;
  }

  bar_sensor_unref_event(self);
  return TRUE;
}

static void bar_sensor_show_bar ( GtkWidget *self )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  priv->show_handle = 0;
  priv->sensor_state = TRUE;
  priv->sensor_block = TRUE;
  g_idle_add((GSourceFunc)bar_sensor_unblock_cb, self);
  css_remove_class(self, "sensor");

  g_signal_handlers_disconnect_by_func(priv->revealer,
      bar_sensor_hide_complete, self);

  if(gtk_bin_get_child(GTK_BIN(self))==priv->sensor)
  {
    gtk_container_remove(GTK_CONTAINER(self), gtk_bin_get_child(GTK_BIN(self)));
    gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->ebox));
  }
  
  bar_reveal(self);
}

static gboolean bar_enter_notify_event ( GtkWidget *self,
    GdkEventCrossing *event )
{
  BarPrivate *priv;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

#if GTK_LAYER_VER_MINOR > 5
  gtk_layer_set_keyboard_mode(GTK_WINDOW(self),
      GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
#else
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self), TRUE);
#endif
  if(!priv->sensor_timeout || priv->sensor_block)
    return TRUE;

  bar_sensor_cancel_hide(self);
  if(!priv->show_handle)
    priv->show_handle = g_timeout_add(priv->show_timeout,
        (GSourceFunc)bar_sensor_show_bar, self);
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
  gboolean horizontal, full_size;
  gchar *end;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  GTK_WIDGET_CLASS(bar_parent_class)->style_updated(self);

  gtk_widget_style_get(self, "halign", &halign, NULL);
  gtk_widget_style_get(self, "valign", &valign, NULL);
  gtk_widget_style_get(self, "direction", &dir, NULL);

  horizontal = (dir == GTK_POS_TOP || dir == GTK_POS_BOTTOM);
  if(priv->current_monitor)
  {
    gdk_monitor_get_geometry(priv->current_monitor, &rect);
    if(!priv->size)
      size = horizontal? rect.width : rect.height;
    else
    {
      size = g_ascii_strtod(priv->size, &end);
      if(*end=='%')
        size = (horizontal?  rect.width : rect.height) * size / 100;
    }
    gtk_widget_set_size_request(GTK_WIDGET(priv->revealer),
        horizontal? size : 1, horizontal? 1 : size);
    gtk_widget_set_size_request(self,
        horizontal? size : 1, horizontal? 1 : size);
  }
  full_size = !priv->current_monitor ||
    (size == (horizontal? rect.width : rect.height));

  if(priv->dir == dir && priv->halign == halign && priv->valign == valign &&
      priv->full_size == full_size)
    return;


  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_TOP,
      (dir == GTK_POS_TOP || ((full_size || valign == GTK_ALIGN_START) &&
        !horizontal)));
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_LEFT,
      (dir == GTK_POS_LEFT || ((full_size || halign == GTK_ALIGN_START) &&
        horizontal)));
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_RIGHT,
      (dir == GTK_POS_RIGHT || ((full_size || halign == GTK_ALIGN_END) &&
        horizontal)));
  gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_BOTTOM,
      (dir == GTK_POS_BOTTOM || ((full_size || valign == GTK_ALIGN_END) &&
        !horizontal)));

  if(priv->dir != dir)
  {
    gtk_orientable_set_orientation(GTK_ORIENTABLE(priv->box),
        (dir==GTK_POS_TOP || dir==GTK_POS_BOTTOM)?
        GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);
    priv->dir = dir;
    trigger_emit("bar-direction");
  }
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
  g_unix_signal_add(SIGUSR2, (GSourceFunc)bar_visibility_toggle_all, NULL);
  g_object_set(G_OBJECT(gtk_settings_get_default()), "gtk-enable-animations",
        TRUE, NULL);
}

GtkWidget *bar_from_name ( gchar *name )
{
  if(!bar_list)
    return NULL;

  return g_hash_table_lookup(bar_list, name? name : "sfwbar");
}

GtkWidget *bar_grid_from_name ( gchar *addr )
{
  GtkWidget *bar;
  BarPrivate *priv;
  GtkWidget *widget = NULL;
  gchar *grid, *ptr, *name;

  if(!addr)
    addr = "sfwbar";

  if(!g_ascii_strcasecmp(addr, "*"))
    return NULL;

  if( (ptr = strchr(addr, ':')) )
  {
    grid = ptr + 1;
    if(ptr == addr)
      name = g_strdup("sfwbar");
    else
      name = g_strndup(addr, ptr-addr);
  }
  else
  {
    grid = NULL;
    name = g_strdup(addr);
  }
  if(!g_ascii_strcasecmp(name, "*"))
  {
    g_message(
        "invalid bar name '*' in grid address %s, defaulting to 'sfwbar'",
        addr);
    g_free(name);
    name = g_strdup("sfwbar");
  }

  if( !(bar=bar_from_name(name)) )
    bar = bar_new(name);
  g_free(name);
  priv = bar_get_instance_private(BAR(bar));

  if(grid && !g_ascii_strcasecmp(grid, "center"))
    widget = priv->center;
  else if(grid && !g_ascii_strcasecmp(grid, "end"))
    widget = priv->end;
  else
    widget = priv->start;

  if(widget)
    return widget;

  widget = grid_new();

  base_widget_set_style_static(widget, g_strdup("layout"));

  if(grid && !g_ascii_strcasecmp(grid, "center"))
  {
    gtk_box_set_center_widget(GTK_BOX(priv->box), widget);
    priv->center = widget;
  }
  else if(grid && !g_ascii_strcasecmp(grid, "end"))
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
  if(!monitor)
    return FALSE;

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

gboolean bar_visibility_toggle_all ( gpointer d )
{
  bar_set_visibility (NULL, NULL, 't');

  return TRUE;
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

  priv->visible_by_mod = (state == 'v');
  visible = priv->visible || priv->visible_by_mod;
  if(!visible && gtk_revealer_get_reveal_child(priv->revealer))
    gtk_revealer_set_reveal_child(priv->revealer, FALSE);
  else if(visible && !gtk_revealer_get_reveal_child(priv->revealer))
    bar_reveal(self);

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

  g_list_foreach(priv->mirror_children,(GFunc)bar_set_layer,layer_str);
}

void bar_set_exclusive_zone ( GtkWidget *self, gchar *zone )
{
  BarPrivate *priv;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(zone);
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
    return monitor_get_name(priv->current_monitor);
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
  if(!priv->jump)
    match = NULL;
  else if( !(match = gdk_display_get_primary_monitor(gdisp)) )
    match = gdk_display_get_monitor(gdisp, 0);

  nmon = gdk_display_get_n_monitors(gdisp);
  if(priv->output)
    for(i=0; i<nmon; i++)
    {
      gmon = gdk_display_get_monitor(gdisp, i);
      if((output = monitor_get_name(gmon)) && !g_strcmp0(output, priv->output))
        match = gmon;
    }

  if(priv->current_monitor != match)
  {
    priv->current_monitor = match;
    if(priv->visible)
      gtk_widget_hide(self);
    gtk_layer_set_monitor(GTK_WINDOW(self), match);
    if(priv->visible)
      gtk_widget_show(self);
  }

  /* remove any mirrors from new primary output */
  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    if(bar_get_monitor(iter->data) == priv->current_monitor)
      break;
  if(iter)
    gtk_widget_destroy(iter->data);

  /* add mirrors to any outputs where they are missing */
  for(i=0; i<nmon; i++)
  {
    gmon = gdk_display_get_monitor(gdisp, i);
    output = monitor_get_name(gmon);
    present = (gmon == priv->current_monitor);
    for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
      if(bar_get_monitor(iter->data) == gmon)
        present = TRUE;

    if(!present && bar_mirror_check(self, output) )
      bar_mirror(self, gmon);
  }

  return FALSE;
}

void bar_set_monitor ( GtkWidget *self, gchar *monitor )
{
  BarPrivate *priv;
  gchar *mon_name;

  g_return_if_fail(IS_BAR(self));
  g_return_if_fail(monitor);
  priv = bar_get_instance_private(BAR(self));

  if(!g_ascii_strncasecmp(monitor, "static:", 7))
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
    bar_mirror(self, priv->current_monitor);
  }
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
      window_set_unref_func(self, (void *)(void *)bar_sensor_unref_event);
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

void bar_set_show_sensor ( GtkWidget *self, gint64 timeout )
{
  BarPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));
  priv->show_timeout = timeout;
  for(iter = priv->mirror_children; iter; iter=g_list_next(iter))
    bar_set_transition(iter->data, timeout);
}

void bar_set_transition ( GtkWidget *self, guint duration )
{
  BarPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  priv->sensor_transition = duration;
  gtk_revealer_set_transition_duration(priv->revealer, duration);
  bar_style_updated(self);
  for(iter = priv->mirror_children; iter; iter=g_list_next(iter))
    bar_set_transition(iter->data, duration);
}

static gboolean bar_on_delete ( GtkWidget *self )
{
  BarPrivate *priv;

  priv = bar_get_instance_private(BAR(self));
  if(priv->mirror_parent)
    return FALSE;
  bar_update_monitor(self);
  return TRUE;
}

GtkWidget *bar_new ( gchar *name )
{
  GtkWidget *self;
  BarPrivate *priv;
  gchar *tmp;

  self = GTK_WIDGET(g_object_new(bar_get_type(), NULL));
  g_signal_connect(G_OBJECT(self), "delete-event",
      G_CALLBACK(bar_on_delete), NULL);
  gtk_application_add_window(application, GTK_WINDOW(self));
  priv = bar_get_instance_private(BAR(self));
  priv->name = name? g_strdup(name) : g_strdup_printf("_bar-%d", bar_count++);
  priv->visible = TRUE;
  priv->jump = TRUE;
  priv->output = g_strdup(monitor_get_name(monitor_default_get()));
  priv->dir = -1;
  priv->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  priv->ebox = gtk_grid_new();
  priv->revealer = GTK_REVEALER(gtk_revealer_new());
  gtk_widget_set_name(priv->ebox, "ebox");
  gtk_widget_set_name(GTK_WIDGET(priv->revealer), "revealer");
  g_signal_connect(G_OBJECT(priv->revealer), "notify::child-revealed",
      G_CALLBACK(bar_revealer_notify), self);
  gtk_container_add(GTK_CONTAINER(priv->revealer), priv->box);

  g_object_ref_sink(priv->ebox);
  gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->ebox));
  gtk_container_add(GTK_CONTAINER(priv->ebox), GTK_WIDGET(priv->revealer));
  g_object_set_data(G_OBJECT(priv->box), "parent_window", self);
  gtk_layer_init_for_window(GTK_WINDOW(self));
  gtk_widget_set_name(self, priv->name);
  gtk_layer_auto_exclusive_zone_enable (GTK_WINDOW(self));
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self), FALSE);
  gtk_layer_set_layer(GTK_WINDOW(self), GTK_LAYER_SHELL_LAYER_TOP);
  gtk_layer_set_monitor(GTK_WINDOW(self), priv->current_monitor);
  bar_style_updated(self);
  gtk_widget_show(priv->box);
  gtk_widget_show(priv->ebox);
  gtk_widget_show(GTK_WIDGET(priv->revealer));

  tmp = g_strdup_printf(
      "window#%s.sensor { background-color: rgba(0,0,0,0); }", priv->name);
  css_widget_apply(self, tmp);
  g_free(tmp);
  bar_update_monitor(self);
  bar_reveal(self);

  if(name)
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
  dpriv->output = g_strdup(monitor_get_name(monitor));

  bar_set_sensor(self, spriv->sensor_timeout);
  bar_set_show_sensor(self, spriv->show_timeout);
  bar_set_transition(self, spriv->sensor_transition);

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
  g_object_set(G_OBJECT(setts), "gtk-application-prefer-dark-theme", FALSE,
      NULL);
  g_object_set(G_OBJECT(setts), "gtk-theme-name", new_theme, NULL);
}

void bar_set_icon_theme ( gchar *new_theme )
{
  GtkSettings *setts;

  setts = gtk_settings_get_default();
  g_object_set(G_OBJECT(setts), "gtk-icon-theme-name", new_theme, NULL);
}
