/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GtkWidget *taskbar;
static gboolean icons, labels;
static gboolean invalid;
static struct layout_widget *taskbar_lw;

void taskbar_init ( struct layout_widget *lw )
{
  taskbar = lw->widget;
  taskbar_lw = lw;
}

void taskbar_invalidate ( void )
{
  invalid = TRUE;
}

void taskbar_set_visual ( gboolean nicons, gboolean nlabels )
{
  icons = nicons;
  labels = nlabels;

  if(!icons)
    labels = TRUE;
}

gboolean taskbar_click_cb ( GtkWidget *widget, GdkEventButton *ev,
    gpointer wid )
{
  if(GTK_IS_BUTTON(widget) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(gtk_bin_get_child(GTK_BIN(widget)),
          &(taskbar_lw->action[ev->button-1]),(GdkEvent *)ev, wid);
  return TRUE;
}

gboolean taskbar_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer wid )
{
  gint button;
  switch(event->direction)
  {
    case GDK_SCROLL_UP:
      button = 4;
      break;
    case GDK_SCROLL_DOWN:
      button = 5;
      break;
    case GDK_SCROLL_LEFT:
      button = 6;
      break;
    case GDK_SCROLL_RIGHT:
      button = 7;
      break;
    default:
      button = 0;
  }
  if(button)
    action_exec(gtk_bin_get_child(GTK_BIN(w)),
        &(taskbar_lw->action[button-1]), (GdkEvent *)event, wid);

  return TRUE;
}

void taskbar_button_cb( GtkWidget *widget, gpointer data )
{
  struct wt_window *button = g_object_get_data(G_OBJECT(widget),"parent");

  if(button == NULL)
    return;

  if(taskbar_lw->action[0].type)
    action_exec(widget,&(taskbar_lw->action[0]),NULL,button->uid);
  else
  {
    if ( wintree_is_focused(button->uid) )
      wintree_minimize(button->uid);
    else
      wintree_focus(button->uid);
  }

  taskbar_invalidate();
}

gint win_compare ( struct wt_window *a, struct wt_window *b)
{
  gint s;
  s = g_strcmp0(a->title,b->title);
  if(s==0)
    return (a->wid - b->wid);
  return s;
}

void taskbar_window_init ( struct wt_window *win )
{
  GtkWidget *box,*icon,*label,*button;
  gint dir;

  if(!taskbar)
    return;

  win->button = gtk_event_box_new();
  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(win->button),button);
  gtk_widget_set_name(button, "taskbar_normal");
  gtk_widget_style_get(button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(button),box);
  if(icons)
  {
    icon = scale_image_new();
    scale_image_set_image(icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(box),icon,NULL,dir,1,1);
  }
  else
    icon = NULL;
  if(labels)
  {
    label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(label),PANGO_ELLIPSIZE_END);
    widget_set_css(label);
    gtk_grid_attach_next_to(GTK_GRID(box),label,icon,dir,1,1);
  }

  g_object_set_data(G_OBJECT(win->button),"parent",win);
  g_object_set_data(G_OBJECT(button),"parent",win);
  g_object_ref(G_OBJECT(win->button));
  g_signal_connect(button,"clicked",G_CALLBACK(taskbar_button_cb),NULL);
  g_signal_connect(win->button,"button_press_event",
        G_CALLBACK(taskbar_click_cb),win->uid);
  gtk_widget_add_events(GTK_WIDGET(win->button),GDK_SCROLL_MASK);
  g_signal_connect(win->button,"scroll-event",
    G_CALLBACK(taskbar_scroll_cb),win->uid);
}

void taskbar_set_label ( struct wt_window *win, gchar *title )
{
  GtkWidget *button;
  GList *blist, *glist, *iter;

  if(!taskbar || !labels)
    return;

  if(!win->button)
    return;

  button = gtk_bin_get_child(GTK_BIN(win->button));

  blist = gtk_container_get_children(GTK_CONTAINER(button));

  if(!blist)
    return;

  glist = gtk_container_get_children(GTK_CONTAINER((blist->data)));

  for(iter = glist; iter; iter = g_list_next(iter))
    if(GTK_IS_LABEL(iter->data))
      gtk_label_set_text(GTK_LABEL(iter->data), title);
  g_list_free(glist);
  g_list_free(blist);
}

void taskbar_update( void )
{
  GList *item;
  struct wt_window *win;

  if(!taskbar || !invalid)
    return;

  flow_grid_clean(taskbar);
  for (item = wintree_get_list(); item; item = g_list_next(item) )
  {
    win = item->data;
    if ( wintree_is_focused(win->uid) )
      gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(win->button)),
          "taskbar_active");
    else
      gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(win->button)),
          "taskbar_normal");
    gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(win->button)),
        GTK_STATE_FLAG_PRELIGHT);

    widget_set_css(win->button);
    flow_grid_attach(taskbar,win->button);
  }
  flow_grid_pad(taskbar);
  gtk_widget_show_all(taskbar);
  invalid = FALSE;
}
