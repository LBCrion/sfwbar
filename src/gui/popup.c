/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "css.h"
#include "window.h"
#include "grid.h"
#include "basewidget.h"
#include "popup.h"
#include "bar.h"
#include "util/string.h"
#include <gtk-layer-shell.h>

static GHashTable *popup_list;

void popup_get_gravity ( GtkWidget *widget, GdkGravity *wanchor,
    GdkGravity *manchor )
{
  
  gtk_widget_style_get(widget, "widget-anchor", wanchor, NULL);
  gtk_widget_style_get(widget, "window-anchor", manchor, NULL);

  switch(bar_get_toplevel_dir(widget))
  {
    case GTK_POS_TOP:
      *wanchor = *wanchor? *wanchor: GDK_GRAVITY_SOUTH_WEST;
      *manchor = *manchor? *manchor: GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_LEFT:
      *wanchor = *wanchor? *wanchor: GDK_GRAVITY_NORTH_EAST;
      *manchor = *manchor? *manchor: GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_RIGHT:
      *wanchor = *wanchor? *wanchor : GDK_GRAVITY_NORTH_WEST;
      *manchor = *manchor? *manchor : GDK_GRAVITY_NORTH_EAST;
      break;
    default:
      *wanchor = *wanchor? *wanchor : GDK_GRAVITY_NORTH_WEST;
      *manchor = *manchor? *manchor : GDK_GRAVITY_SOUTH_WEST;
      break;
  }
}

static void popup_popdown ( GtkWidget *widget )
{
  GdkSeat *seat;

  if(window_ref_check(widget))
    return;
  window_collapse_popups(widget);
  gtk_widget_hide(widget);
  if( (seat=g_object_get_data(G_OBJECT(widget), "seat")) )
      gdk_seat_ungrab(seat);
  gtk_grab_remove(gtk_bin_get_child(GTK_BIN(widget)));
}

void popup_popdown_autoclose ( void )
{
  GHashTableIter iter;
  GtkWidget *popup;

  g_hash_table_iter_init(&iter, popup_list);
  while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&popup))
    if(gtk_widget_get_visible(popup) &&
        gtk_window_get_type_hint(GTK_WINDOW(popup)) ==
        GDK_WINDOW_TYPE_HINT_POPUP_MENU)
      popup_popdown(popup);
}

static gboolean popup_button_cb ( GtkWidget *widget, GdkEventButton *ev,
    void *d )
{
  GdkWindow *window, *wparent;

  wparent = gtk_widget_get_window(d);
  window = ev->window;

  while(window && window != wparent)
    window = gdk_window_get_parent(window);

  if(window == wparent)
    return FALSE;

  popup_popdown(gtk_widget_get_ancestor(d, GTK_TYPE_WINDOW));

  return TRUE;
}

static gboolean popup_state_cb ( GtkWidget *widget, GdkEventWindowState *ev,
    gpointer d )
{
  if( ev->changed_mask & GDK_WINDOW_STATE_WITHDRAWN &&
      ev->new_window_state & GDK_WINDOW_STATE_WITHDRAWN)
    popup_popdown(widget);

  return FALSE;
}

static void popup_transfer_window_grab (GtkWidget *widget, GdkSeat *seat)
{
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkWindow *parent;
  static const GdkSeatCapabilities grab_caps = GDK_SEAT_CAPABILITY_KEYBOARD |
    GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_TABLET_STYLUS;

  if(gtk_window_get_type_hint(GTK_WINDOW(widget)) !=
      GDK_WINDOW_TYPE_HINT_POPUP_MENU)
    return;

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  parent = gdk_screen_get_root_window (gtk_widget_get_screen (widget));
  window = gdk_window_new (parent, &attributes, attributes_mask);
  gtk_widget_register_window (widget, window);

  gdk_window_show (window);
  gdk_seat_grab(seat, window, grab_caps, TRUE, NULL, NULL, NULL, NULL);
  gdk_seat_ungrab(seat);
  gdk_seat_grab(seat, window, grab_caps, TRUE, NULL, NULL, NULL, NULL);
  g_object_set_data (G_OBJECT (gtk_widget_get_window(widget)),
      "gdk-attached-grab-window", window);
}

void popup_show ( GtkWidget *parent, GtkWidget *popup, GdkSeat *seat )
{
  GdkRectangle rect;
  GtkWidget *child, *old_popup;
  GdkWindow *gparent, *gpopup, *transfer;
  GdkGravity wanchor, panchor;
  GHashTableIter iter;

  child = gtk_bin_get_child(GTK_BIN(popup));
  if(!child)
    return;

  g_hash_table_iter_init(&iter, popup_list);
  while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&old_popup))
    if(old_popup != popup && gtk_widget_get_visible(old_popup))
      popup_popdown(old_popup);

  css_widget_cascade(child, NULL);
  gtk_widget_unrealize(popup);
  gtk_widget_realize(popup);
  gparent = gtk_widget_get_window(parent);
  gpopup = gtk_widget_get_window(
      gtk_widget_get_ancestor(popup, GTK_TYPE_WINDOW));

  rect.x = 0;
  rect.y = 0;
  rect.width = gdk_window_get_width(gparent);
  rect.height = gdk_window_get_height(gparent);
  popup_get_gravity(parent,&wanchor,&panchor);
  window_ref(gtk_widget_get_ancestor(parent, GTK_TYPE_WINDOW), popup);

  if(!seat)
    seat = gdk_display_get_default_seat(gdk_display_get_default());

  g_object_set_data(G_OBJECT(popup), "parent_window",
      gtk_widget_get_ancestor(parent, GTK_TYPE_WINDOW));
  g_object_set_data(G_OBJECT(popup), "parent", parent);
  g_object_set_data(G_OBJECT(popup), "seat", seat);

  popup_transfer_window_grab(popup, seat);
  gtk_window_set_transient_for(GTK_WINDOW(popup),
      GTK_WINDOW(gtk_widget_get_ancestor(parent, GTK_TYPE_WINDOW)));

  gdk_window_set_transient_for(gpopup, gparent);
  gdk_window_move_to_rect(gpopup, &rect, wanchor, panchor,
      GDK_ANCHOR_FLIP_X | GDK_ANCHOR_FLIP_Y, 0, 0);
  css_widget_cascade(popup,NULL);

  transfer = g_object_get_data(G_OBJECT(gpopup), "gdk-attached-grab-window");
  if(transfer)
  {
    gdk_seat_ungrab(seat);
    gtk_widget_unregister_window(popup, transfer);
    gdk_window_destroy(transfer);
    g_object_set_data (G_OBJECT (gpopup), "gdk-attached-grab-window", NULL);
    gtk_grab_add(child);
  }
}

void popup_trigger ( GtkWidget *parent, gchar *name, GdkEvent *ev )
{
  GtkWidget *popup;

  popup = popup_from_name(name);
  if(!popup || !parent)
    return;

  if(gtk_widget_get_visible(popup))
    popup_popdown(popup);
  else
    popup_show(parent, popup, gdk_device_get_seat(gdk_event_get_device(ev)));
}

void popup_resize_maybe ( GtkWidget *self )
{
  GtkWidget *grab;
  GtkRequisition req;
  gint old_width, old_height;

  if(!gtk_widget_is_visible(self))
    return;
  if(window_ref_check(self))
    return;

  old_width = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(self), "width"));
  old_height = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(self), "height"));
  gtk_widget_get_preferred_size(gtk_bin_get_child(GTK_BIN(self)), NULL, &req);

  if(old_width==req.width  && old_height==req.height)
    return;

  if( (grab = gtk_grab_get_current()) )
    gtk_grab_remove(grab);

  g_object_set_data(G_OBJECT(self), "width", GINT_TO_POINTER(req.width));
  g_object_set_data(G_OBJECT(self), "height", GINT_TO_POINTER(req.height));

  gtk_widget_hide(self);
  gtk_window_resize(GTK_WINDOW(self), req.width, req.height);
  popup_show(g_object_get_data(G_OBJECT(self), "parent"), self,
      g_object_get_data(G_OBJECT(self), "seat"));
}

void popup_size_allocate_cb ( GtkWidget *grid, gpointer dummy, GtkWidget *win )
{
  popup_resize_maybe(win);
}

void popup_set_autoclose ( GtkWidget *win, gboolean autoclose )
{
  if(autoclose)
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  else
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_NORMAL);
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
  window_set_unref_func(win, (void(*)(gpointer))popup_resize_maybe);
  grid = grid_new();
  gtk_container_add(GTK_CONTAINER(win),grid);
  gtk_widget_set_name(win,name);
  gtk_widget_set_name(grid,name);
  gtk_window_set_accept_focus(GTK_WINDOW(win), TRUE);
  g_signal_connect(grid, "button-release-event", G_CALLBACK(popup_button_cb),
      win);
  g_signal_connect(win,"window-state-event", G_CALLBACK(popup_state_cb),NULL);
  g_signal_connect(grid, "size-allocate", G_CALLBACK(popup_size_allocate_cb), win);

  g_hash_table_insert(popup_list, g_strdup(name),win);
  return win;
}

GtkWidget *popup_from_name ( gchar *name )
{
  if(!popup_list || !name)
    return NULL;
  return g_hash_table_lookup(popup_list, name);
}
