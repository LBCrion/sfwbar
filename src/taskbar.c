/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

struct taskbar_item {
  GtkWidget *widget;
  void *parent;
};


static struct layout_widget *taskbar_lw;

void taskbar_init ( struct layout_widget *lw )
{
  taskbar_lw = lw;
}

void taskbar_invalidate ( GtkWidget *taskbar )
{
  g_object_set_data(G_OBJECT(taskbar),"invalid",GINT_TO_POINTER(TRUE));
}

void taskbar_invalidate_all ( void )
{
  taskbar_invalidate(taskbar_lw->widget);
}

struct taskbar_item *taskbar_item_lookup ( GtkWidget *taskbar, void *parent )
{
  GList *iter;

  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for(;iter;iter=g_list_next(iter))
    if(((struct taskbar_item *)iter->data)->parent == parent)
      return iter->data;

  return NULL;
}

gboolean taskbar_click_cb ( GtkWidget *widget, GdkEventButton *ev,
    gpointer wid )
{
  if(GTK_IS_BUTTON(widget) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(gtk_bin_get_child(GTK_BIN(widget)),
        &(taskbar_lw->action[ev->button]),(GdkEvent *)ev,
        wintree_from_id(wid),NULL);
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
        &(taskbar_lw->action[button]), (GdkEvent *)event,
        wintree_from_id(wid),NULL);

  return TRUE;
}

void taskbar_button_cb( GtkWidget *widget, gpointer data )
{
  struct wt_window *button = g_object_get_data(G_OBJECT(widget),"parent");

  if(button == NULL)
    return;

  if(taskbar_lw->action[1].type)
    action_exec(widget,&(taskbar_lw->action[1]),NULL,button,NULL);
  else
  {
    if ( wintree_is_focused(button->uid) )
      wintree_minimize(button->uid);
    else
      wintree_focus(button->uid);
  }

  taskbar_invalidate(taskbar_lw->widget);
}

void taskbar_item_init ( GtkWidget *taskbar, struct wt_window *win )
{
  struct taskbar_item *item;
  GtkWidget *box,*icon,*label,*button;
  gint dir;
  gboolean icons, labels;
  gint title_width;

  if(!taskbar_lw || !taskbar_lw->widget)
    return;

  item = taskbar_item_lookup(taskbar, win);
  if(item)
    return;
  item = g_malloc0(sizeof(struct taskbar_item));

  icons = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"icons"));
  labels = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"labels"));

  if(!icons)
    labels = TRUE;

  item->widget = gtk_event_box_new();
  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(item->widget),button);
  gtk_widget_set_name(button, "taskbar_normal");
  gtk_widget_style_get(button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(button),box);
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"title_width"));
  if(!title_width)
    title_width = -1;

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
    gtk_label_set_max_width_chars(GTK_LABEL(label),title_width);
    widget_set_css(label,FALSE);
    gtk_grid_attach_next_to(GTK_GRID(box),label,icon,dir,1,1);
  }

  g_object_set_data(G_OBJECT(item->widget),"parent",win);
  g_object_set_data(G_OBJECT(button),"parent",win);
  g_object_ref(G_OBJECT(item->widget));
  g_signal_connect(button,"clicked",G_CALLBACK(taskbar_button_cb),NULL);
  g_signal_connect(item->widget,"button_press_event",
      G_CALLBACK(taskbar_click_cb),win->uid);
  gtk_widget_add_events(GTK_WIDGET(item->widget),GDK_SCROLL_MASK);
  g_signal_connect(item->widget,"scroll-event",
      G_CALLBACK(taskbar_scroll_cb),win->uid);
  g_object_set_data(G_OBJECT(taskbar),"items", g_list_append(
      g_object_get_data(G_OBJECT(taskbar),"items"), item));
}

void taskbar_item_init_for_all ( struct wt_window *win )
{
  taskbar_item_init( taskbar_lw->widget, win );
}

void taskbar_set_label ( GtkWidget *taskbar, struct wt_window *win, gchar *title )
{
  struct taskbar_item *item;
  GtkWidget *button;
  GList *blist, *glist, *iter;

  if(!taskbar)
    return;

  if(!GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"labels")))
    return;

  item = taskbar_item_lookup(taskbar,win);

  if(!item)
    return;

  button = gtk_bin_get_child(GTK_BIN(item->widget));

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

void taskbar_set_label_for_all ( struct wt_window *win, gchar *title )
{
  if(!taskbar_lw)
    return;

  taskbar_set_label(taskbar_lw->widget, win, title);
}

void taskbar_update( GtkWidget *taskbar )
{
  struct wt_window *win;
  struct taskbar_item *item;
  GList *iter;
  gchar *output;
  gboolean filter_output;

  if(!taskbar)
    return;

  if(!GPOINTER_TO_INT(g_object_get_data(
          G_OBJECT(taskbar),"invalid")))
    return;

  output = bar_get_output(taskbar);
  flow_grid_clean(taskbar);
  filter_output = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"filter_output"));
  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for (; iter; iter = g_list_next(iter) )
  {
    item = iter->data;
    win = g_object_get_data(G_OBJECT(item->widget),"parent");
    if(( !filter_output || !!win->output || g_strcmp0(win->output,output)) &&
        item )
    {
      if ( wintree_is_focused(win->uid) )
        gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(item->widget)),
            "taskbar_active");
      else
        gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(item->widget)),
            "taskbar_normal");
      gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(item->widget)),
          GTK_STATE_FLAG_PRELIGHT);

      widget_set_css(item->widget,TRUE);
      flow_grid_attach(taskbar,item->widget);
    }
  }
  flow_grid_pad(taskbar);
  gtk_widget_show_all(taskbar);

  g_object_set_data(G_OBJECT(taskbar),"invalid", GINT_TO_POINTER(FALSE));
}

void taskbar_update_all ( void )
{
  if(!taskbar_lw)
    return;

  taskbar_update(taskbar_lw->widget);
}
