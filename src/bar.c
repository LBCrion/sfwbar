/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gtk/gtk.h>
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
  GtkWidget *box;
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
        "invalid bar name 'all' in grid address %s, defaulting to 'sfwbar'",
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

  box = gtk_bin_get_child(GTK_BIN(bar));
  if(grid && !g_ascii_strcasecmp(grid,"center"))
  {
    gtk_box_set_center_widget(GTK_BOX(box),widget);
    priv->center = widget;
  }
  else if(grid && !g_ascii_strcasecmp(grid,"end"))
  {
    gtk_box_pack_end(GTK_BOX(box),widget,TRUE,TRUE,0);
    priv->end = widget;
  }
  else
  {
    gtk_box_pack_start(GTK_BOX(box),widget,TRUE,TRUE,0);
    priv->start = widget;
  }
  return widget;
}

void bar_set_mirrors ( GtkWidget *self, gchar *mirror )
{
  BarPrivate *priv;
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_mirrors(bar, mirror);
    }
    return;
  }

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_strfreev(priv->mirror_targets);
  priv->mirror_targets = g_strsplit(mirror,";",-1);
}

void bar_set_mirror_blocks ( GtkWidget *self, gchar *mirror )
{
  BarPrivate *priv;
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_mirror_blocks(bar, mirror);
    }
    return;
  }

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_strfreev(priv->mirror_blocks);
  priv->mirror_blocks = g_strsplit(mirror,";",-1);
}

void bar_set_id ( GtkWidget *self, gchar *id )
{
  BarPrivate *priv;
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_id(bar, id);
    }
    return;
  }

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  g_free(priv->bar_id);
  priv->bar_id = g_strdup(id);
  if(priv->mirror_children)
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
  {
    bar_save_monitor(self);
    gtk_widget_hide(self);
  }
  else if(!gtk_widget_is_visible(self))
  {
    bar_update_monitor(self);
    gtk_widget_show_now(self);
  }

  if(priv->mirror_children)
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
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_layer(bar, layer_str);
    }
    return;
  }

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
  if(priv->mirror_children)
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
        MAX(-1,g_ascii_strtoll(zone,NULL,10)));

  if(priv->mirror_children)
    g_list_foreach(priv->mirror_children,(GFunc)bar_set_exclusive_zone,zone);
}

gchar *bar_get_output ( GtkWidget *widget )
{
  return g_object_get_data( G_OBJECT(widget_get_monitor(
          gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW))),"xdg_name");
}

gboolean bar_update_monitor ( GtkWidget *self )
{
  BarPrivate *priv;
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
  gint nmon,i;
  gchar *name;

  g_return_val_if_fail(IS_BAR(self),FALSE);
  priv = bar_get_instance_private(BAR(self));

  if(!xdg_output_check())
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

  if(priv->output)
  {
    nmon = gdk_display_get_n_monitors(gdisp);
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      name = g_object_get_data(G_OBJECT(gmon),"xdg_name");
      if(name && !g_strcmp0(name,priv->output))
        match = gmon;
    }
  }

  if(!match)
    return FALSE;
  bar_save_monitor(self);
  gtk_widget_hide(self);
  gtk_layer_set_monitor(GTK_WINDOW(self), match);
  if(priv->visible)
  {
    gtk_widget_show_now(self);
    taskbar_invalidate_conditional();
  }
  return FALSE;
}

void bar_save_monitor ( GtkWidget *self )
{
  BarPrivate *priv;
  GdkMonitor *mon;

  g_return_if_fail(IS_BAR(self));
  priv = bar_get_instance_private(BAR(self));

  if(priv->output)
    return;
  mon = widget_get_monitor(self);
  if(!mon)
    return;

  priv->output = g_strdup(g_object_get_data(G_OBJECT(mon),"xdg_name"));
}

void bar_set_monitor ( GtkWidget *self, gchar *monitor )
{
  BarPrivate *priv;
  gchar *old_mon,*mon_name;
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_monitor(bar, monitor);
    }
    return;
  }

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

  old_mon = priv->output;

  if(!old_mon || g_ascii_strcasecmp(old_mon, mon_name))
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
  void *bar,*key;
  GHashTableIter iter;

  if(!self)
  {
    if(bar_list)
    {
      g_hash_table_iter_init(&iter,bar_list);
      while(g_hash_table_iter_next(&iter,&key,&bar))
        bar_set_size(bar, size);
    }
    return;
  }

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
  if(priv->mirror_children)
    g_list_foreach(priv->mirror_children,(GFunc)bar_set_size,size);
}

GtkWidget *bar_new ( gchar *name )
{
  GtkWidget *self;
  BarPrivate *priv;
  GtkWidget *box;
  gint toplevel_dir;

  self = GTK_WIDGET(g_object_new(bar_get_type(), NULL));
  priv = bar_get_instance_private(BAR(self));
  priv->name = g_strdup(name);
  priv->visible = TRUE;
  gtk_layer_init_for_window (GTK_WINDOW(self));
  gtk_widget_set_name(self,name);
  gtk_layer_auto_exclusive_zone_enable (GTK_WINDOW(self));
  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(self),FALSE);
  gtk_layer_set_layer(GTK_WINDOW(self),GTK_LAYER_SHELL_LAYER_TOP);

  gtk_widget_style_get(self,"direction",&toplevel_dir,NULL);
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor(GTK_WINDOW(self),GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  if(toplevel_dir == GTK_POS_LEFT || toplevel_dir == GTK_POS_RIGHT)
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  else
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  gtk_container_add(GTK_CONTAINER(self), box);

  if(priv->name)
  {
    if(!bar_list)
      bar_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

    g_hash_table_insert(bar_list, priv->name, self);
  }

  return self;
}

GtkWidget *bar_mirror ( GtkWidget *src, gchar *output )
{
  GtkWidget *self, *box;
  BarPrivate *spriv,*dpriv;

  g_return_val_if_fail(IS_BAR(src),NULL);
  self = bar_new(NULL);
  spriv = bar_get_instance_private(BAR(src));
  dpriv = bar_get_instance_private(BAR(src));

  gtk_widget_set_name(self,gtk_widget_get_name(src));
  box = gtk_bin_get_child(GTK_BIN(self));
  if(spriv->start)
  {
    dpriv->start = base_widget_mirror(spriv->start);
    gtk_box_pack_start(GTK_BOX(box),dpriv->start,TRUE,TRUE,0);
  }
  if(spriv->center)
  {
    dpriv->center = base_widget_mirror(spriv->center);
    gtk_box_set_center_widget(GTK_BOX(box),dpriv->center);
  }
  if(spriv->end)
  {
    dpriv->end = base_widget_mirror(spriv->end);
    gtk_box_pack_end(GTK_BOX(box),dpriv->end,TRUE,TRUE,0);
  }

  dpriv->visible = spriv->visible;
  dpriv->hidden = spriv->hidden;
  dpriv->jump = spriv->jump;
  dpriv->jump = spriv->jump;
  dpriv->output = g_strdup(spriv->output);
  dpriv->bar_id = g_strdup(spriv->bar_id);
  spriv->mirror_children = g_list_prepend(spriv->mirror_children, self);
  dpriv->mirror_parent = src;
  mirrors = g_list_prepend(mirrors,self);

//  gtk_application_add_window(app,GTK_WINDOW(self));
  gtk_widget_show(self);
  css_widget_cascade(GTK_WIDGET(self),NULL);
  bar_set_monitor(self,output);
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
