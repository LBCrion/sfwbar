/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

static GtkWidget *switcher;
static GtkWidget *grid;
static gint interval;
static gchar hstate;
static gint counter;
static gint title_width = -1;
static gboolean icons, labels;
static gboolean invalid;
static GList *focus;

void switcher_init ( void )
{
  if(!grid)
    return;

  g_object_ref(grid);
  if(switcher)
  {
    gtk_container_remove(GTK_CONTAINER(switcher),grid);
    gtk_window_close(GTK_WINDOW(switcher));
  }

  switcher = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW(switcher));
  gtk_layer_set_layer(GTK_WINDOW(switcher),GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_widget_set_name(switcher, "switcher");
  gtk_container_add(GTK_CONTAINER(switcher),grid);
  g_object_unref(grid);
}

void switcher_config ( gint ncols, gchar *css, gint nmax,
    gboolean nicons, gboolean nlabels, gint twidth )
{
  GList *iter;

  if(!switcher)
  {
    grid = flow_grid_new(FALSE);
    gtk_widget_set_name(grid, "switcher");
    switcher_init();
  }
  flow_grid_set_cols(grid,ncols);
  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (grid);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }
  interval = nmax;
  hstate = 's';
  icons = nicons;
  labels = nlabels;
  title_width = twidth;
  if(!icons)
    labels = TRUE;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    switcher_window_init(iter->data);
}

void switcher_invalidate ( void )
{
  invalid = TRUE;
}

gboolean switcher_event ( struct json_object *obj )
{
  gchar *state,*id;
  GList *item;
  gboolean event = FALSE;

  if(!switcher)
    return TRUE;

  if(obj)
  {
    state = json_string_by_name(obj,"hidden_state");
    if(state)
    {
      if(*state!='h')
      {
        event=TRUE;
        id = json_string_by_name(obj,"id");
        if(id)
          sway_ipc_command("bar %s hidden_state hide",id);
        g_free(id);
      }
      g_free(state);

    }
  }
  else
    event = TRUE;

  if(event)
  {
    counter = interval;
    focus = NULL;
    for (item = wintree_get_list(); item; item = g_list_next(item) )
      if ( wintree_is_focused(((struct wt_window *)item->data)->uid) )
        focus = g_list_next(item);
    if(focus==NULL)
      focus=wintree_get_list();
    if(focus!=NULL)
      wintree_set_focus(((struct wt_window *)focus->data)->uid);
    switcher_invalidate();
  }

  return TRUE;
}

void switcher_window_init ( struct wt_window *win)
{
  GtkWidget *icon,*label;
  gint dir;

  if(!switcher)
  {
    win->switcher = NULL;
    return;
  }

  win->switcher = gtk_grid_new();
  gtk_widget_set_name(win->switcher, "switcher_normal");
  gtk_widget_style_get(win->switcher,"direction",&dir,NULL);
  g_object_ref(G_OBJECT(win->switcher));
  if(icons)
  {
    icon = scale_image_new();
    scale_image_set_image(icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(win->switcher),icon,NULL,dir,1,1);
  }
  else
    icon = NULL;
  if(labels)
  {
    label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(label),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label),title_width);
    widget_set_css(label,FALSE);
    gtk_grid_attach_next_to(GTK_GRID(win->switcher),label,icon,dir,1,1);
  }
}

void switcher_set_label ( struct wt_window *win, gchar *title )
{
  GList *glist, *iter;

  if(!switcher || !labels)
    return;

  if(!win->switcher)
    return;

  glist = gtk_container_get_children(GTK_CONTAINER(win->switcher));

  for(iter = glist; iter; iter = g_list_next(iter))
    if(GTK_IS_LABEL(iter->data))
      gtk_label_set_text(GTK_LABEL(iter->data), title);
  g_list_free(glist);
}

void switcher_update ( void )
{
  GList *item;
  struct wt_window *win;

  if(!switcher)
    return;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    if(!invalid)
      return;

    flow_grid_clean(grid);
    for (item = wintree_get_list(); item; item = g_list_next(item) )
    {
      win = item->data;
      if (wintree_is_focused(win->uid) )
        gtk_widget_set_name(win->switcher,"switcher_active");
      else
        gtk_widget_set_name(win->switcher,"switcher_normal");

      flow_grid_attach(grid,win->switcher);
    }
    if(!gtk_widget_is_visible(switcher))
      switcher_init();
    gtk_widget_show_all(switcher);
    invalid = FALSE;
  }
  else
  {
    gtk_widget_hide(switcher);
    win = focus->data;
    wintree_focus(win->uid);
  }
}
