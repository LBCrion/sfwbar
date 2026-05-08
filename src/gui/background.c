/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include "wayland.h"
#include "gui/background.h"
#include "gui/basewidget.h"

#define EXT_BACKGROUND_EFFECT_VERSION 1

static struct ext_background_effect_manager_v1 *background_effect_manager;
GType background_effect_enum;
GQuark background_effect_effect_q, background_effect_tag_q;

GEnumValue background_effect_enum_defs[] = {
  { BACKGROUND_EFFECT_NONE, "none", "none" },
  { BACKGROUND_EFFECT_BLUR, "blur", "blur" },
  { 0, NULL, NULL },
};

static void background_effect_region_calc ( GtkWidget *widget,
    struct wl_region *region )
{
  GtkStyleContext *style;
  GtkStateFlags flags;
  GtkBorder margin;
  GtkAllocation alloc;
  gint x, y, w, h, r, dx, dy, t;

  if(background_effect_get(widget) == BACKGROUND_EFFECT_BLUR)
  {
    style = gtk_widget_get_style_context(widget);
    flags = gtk_style_context_get_state(style);
    gtk_widget_get_allocation(widget, &alloc);
    gtk_style_context_get(style, flags,
        GTK_STYLE_PROPERTY_BORDER_RADIUS, &r, NULL);

    if(GTK_IS_MENU(widget))
      margin.left = margin.right = margin.top = margin.bottom = 0;
    else
      gtk_style_context_get_margin(style, flags, &margin);

    gtk_widget_translate_coordinates(widget, gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW),
        margin.left + r, margin.right + r, &x, &y);
    w = alloc.width - margin.left - margin.right - 2*r;
    h = alloc.height - margin.top - margin.bottom - 2*r;
    wl_region_add(region, x - r, y, w + r*2, h);

    t = r  / 16;
    dx = r;
    dy = 0;
    do {
      wl_region_add(region, x - dx, y - dy, w + 2*dx, 1);
      wl_region_add(region, x - dx, y + h - 1 + dy, w + 2*dx, 1);
      dy++;
      t += dy;

      if(t-dx >= 0)
      {
        wl_region_add(region, x - dy + 1, y - dx, w + 2*dy - 2, 1);
        wl_region_add(region, x - dy + 1, y + h - 1 + dx, w + 2*dy - 2, 1);
        t -= dx;
        dx--;
      }
    } while(dx>=dy);
  }
  if(GTK_IS_CONTAINER(widget))
    gtk_container_foreach(GTK_CONTAINER(widget),
        (GtkCallback)background_effect_region_calc, region);
}

void background_effect_apply ( GtkWidget *self )
{
  struct ext_background_effect_surface_v1 *effect;
  struct wl_compositor *compositor;
  struct wl_region *region;
  struct wl_surface *surface;
  GdkWindow *gwin;

  if(!background_effect_manager)
    return;

  if( !(gwin = gtk_widget_get_window(self)) ||
      !gdk_window_is_visible(gwin) ||
      !(surface = gdk_wayland_window_get_wl_surface(gwin)) )
    return;

  if( !(effect = g_object_get_qdata(G_OBJECT(self),
          background_effect_effect_q)) )
  {
    effect = ext_background_effect_manager_v1_get_background_effect(
        background_effect_manager, surface);
    g_object_set_qdata(G_OBJECT(self), background_effect_effect_q,
        effect);
  }

  compositor = gdk_wayland_display_get_wl_compositor(
      gdk_display_get_default());
  region = wl_compositor_create_region(compositor);
  background_effect_region_calc(self, region);

  ext_background_effect_surface_v1_set_blur_region(effect, region);
  wl_region_destroy(region);
}

static void background_effect_size_allocate ( GtkWidget *self,
    GdkRectangle *allocation )
{
  background_effect_apply(self);
}

static void background_effect_unmap ( GtkWidget *self, gpointer d )
{
  struct ext_background_effect_surface_v1 *effect;

  if((effect = g_object_get_qdata(G_OBJECT(self), background_effect_effect_q)))
  {
    g_object_set_qdata(G_OBJECT(self), background_effect_effect_q, NULL);
    ext_background_effect_surface_v1_destroy(effect);
  }
}

void background_effect_init ( void )
{
  if(background_effect_manager)
    return;

  if(!background_effect_enum)
    background_effect_enum = g_enum_register_static("background_effect",
        background_effect_enum_defs);

  background_effect_manager = wayland_iface_register(
      ext_background_effect_manager_v1_interface.name,
      EXT_BACKGROUND_EFFECT_VERSION, EXT_BACKGROUND_EFFECT_VERSION,
      &ext_background_effect_manager_v1_interface);

  background_effect_effect_q =
    g_quark_from_static_string("background_effect_priv");
  background_effect_tag_q =
    g_quark_from_static_string("background_effect_tag");
}

background_effect_t background_effect_get ( GtkWidget *self )
{
  return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(self),
        background_effect_tag_q));
}

void background_effect_surface_init ( GtkWidget *self )
{
  GtkWidget *surface_widget;

  if(!background_effect_manager)
    return;

  if( !(surface_widget = gtk_widget_get_ancestor(self, GTK_TYPE_WINDOW)) )
    return;

  if(!g_signal_handler_find(surface_widget, G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, background_effect_apply, NULL))
    g_signal_connect(G_OBJECT(surface_widget), "map-event",
        G_CALLBACK(background_effect_apply), NULL);
  if(!g_signal_handler_find(surface_widget, G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, background_effect_size_allocate, NULL))
    g_signal_connect(G_OBJECT(surface_widget), "size-allocate",
        G_CALLBACK(background_effect_size_allocate), NULL);
  if(!g_signal_handler_find(surface_widget, G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, background_effect_unmap, NULL))
    g_signal_connect(G_OBJECT(surface_widget), "unmap",
        G_CALLBACK(background_effect_unmap), NULL);

  background_effect_apply(surface_widget);
}

void background_effect_set ( GtkWidget *self, background_effect_t type )
{
  if(!background_effect_manager)
    return;

  g_return_if_fail(GTK_IS_WIDGET(self));

  g_object_set_qdata(G_OBJECT(self), background_effect_tag_q,
      GINT_TO_POINTER(type));

  if(!g_signal_handler_find(self, G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, background_effect_surface_init, NULL))
    g_signal_connect(G_OBJECT(self), "hierarchy_changed",
        G_CALLBACK(background_effect_surface_init), NULL);
  background_effect_surface_init(self);
}
