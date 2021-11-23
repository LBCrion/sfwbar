/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include "sfwbar.h"
#include "config.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

gboolean widget_action ( GtkWidget *widget, gchar *cmd )
{
  gchar *argv[] = {NULL,NULL};
  GPid pid;
  if(!cmd)
    return TRUE;
  argv[0]=cmd;
  g_spawn_async(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,&pid,NULL);
  return TRUE;
}

struct layout_widget *layout_widget_new ( void )
{
  struct layout_widget *lw;
  lw = g_malloc(sizeof(struct layout_widget));
  lw->widget = NULL;
  lw->style = NULL;
  lw->css = NULL;
  lw->value = NULL;
  lw->action = NULL;
  lw->icon = NULL;
  lw->eval = NULL;
  lw->wtype = 0;
  lw->interval = 0;
  lw->next_poll = 0;
  lw->dir = GTK_POS_RIGHT;
  lw->invalid = FALSE;
  lw->ready = FALSE;
  lw->rect.x = 0;
  lw->rect.y = 0;
  lw->rect.w = 0;
  lw->rect.h = 0;
  return lw;
}

void layout_widget_config ( struct layout_widget *lw )
{
  GtkWidget *img;

  if(lw->style)
  {
    gtk_widget_set_name(lw->widget,lw->style);
    g_free(lw->style);
    lw->style = NULL;
  }

  if(lw->css)
  {
    GtkStyleContext *context = gtk_widget_get_style_context (lw->widget);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,lw->css,strlen(lw->css),NULL);
    gtk_style_context_add_provider (context,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(lw->css);
    lw->css = NULL;
  }

  if(GTK_IS_PROGRESS_BAR(lw->widget))
  {
    gtk_widget_style_get(lw->widget,"direction",&(lw->dir),NULL);
    if((lw->dir==GTK_POS_LEFT)||(lw->dir==GTK_POS_RIGHT))
      gtk_orientable_set_orientation(GTK_ORIENTABLE(lw->widget),GTK_ORIENTATION_HORIZONTAL);
    else
      gtk_orientable_set_orientation(GTK_ORIENTABLE(lw->widget),GTK_ORIENTATION_VERTICAL);
    if((lw->dir==GTK_POS_TOP)||(lw->dir==GTK_POS_LEFT))
      gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(lw->widget),TRUE);
    else
      gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(lw->widget),FALSE);
  }

  if(lw->wtype==G_TOKEN_TASKBAR)
    taskbar_init(lw->widget);

  if(lw->wtype==G_TOKEN_PAGER)
    pager_init(lw->widget);

  if(lw->wtype==G_TOKEN_TRAY)
    sni_init(lw->widget);

  if(lw->icon&&GTK_IS_BUTTON(lw->widget))
  {
    img = scale_image_new();
    scale_image_set_image(img,lw->icon);
    gtk_container_add(GTK_CONTAINER(lw->widget),img);
  }

  if(GTK_IS_LABEL(lw->widget))
  {
    gdouble xalign;
    gtk_widget_style_get(lw->widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(lw->widget),xalign);
  }

  if((lw->action)&&(GTK_BUTTON(lw->widget)))
    g_signal_connect(G_OBJECT(lw->widget),"clicked",
      G_CALLBACK(widget_action),g_strdup(lw->action));

  widget_set_css(lw->widget);
}

void layout_widget_free ( struct layout_widget *lw )
{
  if(!lw)
    return;
  g_free(lw->style);
  g_free(lw->css);
  g_free(lw->value);
  g_free(lw->action);
  g_free(lw->icon);
  g_free(lw->eval);
  g_free(lw);
}

void layout_widgets_update ( void )
{
  GList *iter;
  gint64 ctime;
  struct layout_widget *lw;
  gchar *eval;
  guint vcount;

  ctime = g_get_real_time();

  for(iter=context->widgets;iter!=NULL;iter=g_list_next(iter))
  {
    lw = iter->data;
    if(lw->ready)
      continue;
    if(lw->invalid)
    {
      layout_widget_free(lw);
      context->widgets = g_list_delete_link(context->widgets,iter);
      continue;
    }
    if(lw->next_poll > ctime)
      continue;
    lw->next_poll = ctime+lw->interval;
    if((lw->value!=NULL)&&(GTK_IS_LABEL(lw->widget)||
        GTK_IS_PROGRESS_BAR(lw->widget)||GTK_IS_IMAGE(lw->widget)))
    {
      eval = expr_parse(lw->value, &vcount);
      if(g_strcmp0(eval,lw->eval))
      {
        lw->ready = TRUE;
        g_free(lw->eval);
        lw->eval = eval;
      }
      else
        g_free(eval);
      if(!vcount)
        lw->invalid = TRUE;
    }
  }
}

void layout_widgets_draw ( void )
{
  GList *iter;
  struct layout_widget *lw;

  for(iter=context->widgets;iter!=NULL;iter=g_list_next(iter))
  {
    lw = iter->data;
    if(lw->ready)
    {
      if(GTK_IS_LABEL(GTK_WIDGET(lw->widget)))
        gtk_label_set_markup(GTK_LABEL(lw->widget),lw->eval);
      if(GTK_IS_PROGRESS_BAR(lw->widget))
        if(!g_strrstr(lw->eval,"nan"))
         gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lw->widget),
              g_ascii_strtod(lw->eval,NULL));

      if(GTK_IS_IMAGE(lw->widget))
      {
        scale_image_set_image(GTK_WIDGET(lw->widget),lw->eval);
        scale_image_update(GTK_WIDGET(lw->widget));
      }
      lw->ready = FALSE;
    }
  }
}

void widget_set_css ( GtkWidget *widget )
{
  gboolean expand;
  GList *l;
  gtk_widget_style_get(widget,"hexpand",&expand,NULL);
  gtk_widget_set_hexpand(GTK_WIDGET(widget),expand);
  gtk_widget_style_get(widget,"vexpand",&expand,NULL);
  gtk_widget_set_vexpand(GTK_WIDGET(widget),expand);
  if(GTK_IS_CONTAINER(widget))
  {
    l = gtk_container_get_children(GTK_CONTAINER(widget));
    for(;l!=NULL;l=g_list_next(l))
      widget_set_css(l->data);
  }

}
