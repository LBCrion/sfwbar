/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include "sfwbar.h"
#include "config.h"
#include <gtk/gtk.h>

static GList *widget_list;
static GHashTable *menus;

GtkWidget *layout_menu_get ( gchar *name )
{
  if(!menus || !name)
    return NULL;
  return g_hash_table_lookup(menus, name);
}

void layout_menu_remove ( gchar *name )
{
  if(name)
    g_hash_table_remove(menus,name);
}

void layout_menu_add ( gchar *name, GtkWidget *menu )
{
  if(!menus)
    menus = g_hash_table_new_full((GHashFunc)str_nhash,(GEqualFunc)str_nequal,
        g_free,g_object_unref);

  if(!g_hash_table_lookup_extended(menus, name, NULL, NULL))
  {
    g_object_ref_sink(menu);
    g_hash_table_insert(menus, name, menu);
  }
  else
    g_free(name);
}

void layout_menu_popup ( GtkWidget *widget, GtkWidget *menu, GdkEvent *event,
    gpointer wid )
{
  GdkGravity wanchor, manchor;

  if(!menu || !widget)
    return;

  g_object_set_data( G_OBJECT(menu), "wid", wid );

  switch(get_toplevel_dir())
  {
    case GTK_POS_TOP:
      wanchor = GDK_GRAVITY_SOUTH_WEST;
      manchor = GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_LEFT:
      wanchor = GDK_GRAVITY_NORTH_EAST;
      manchor = GDK_GRAVITY_NORTH_WEST;
      break;
    case GTK_POS_RIGHT:
      wanchor = GDK_GRAVITY_NORTH_WEST;
      manchor = GDK_GRAVITY_NORTH_EAST;
      break;
    default:
      wanchor = GDK_GRAVITY_NORTH_WEST;
      manchor = GDK_GRAVITY_SOUTH_WEST;
      break;
  }
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_widget(GTK_MENU(menu),widget,wanchor,manchor,event);
}

gboolean layout_widget_has_actions ( struct layout_widget * lw )
{
  static const gchar *act_check[MAX_BUTTON];
  return memcmp(lw->action,act_check,sizeof(gchar *)*MAX_BUTTON);
}

gboolean widget_menu_action ( GtkWidget *w ,struct layout_action *action )
{
  GtkWidget *parent = gtk_widget_get_ancestor(w,GTK_TYPE_MENU);
  gpointer wid;

  if(parent)
    wid = g_object_get_data ( G_OBJECT(parent), "wid" );
  else
    wid = NULL;

  if(!wid)
    wid = wintree_get_focus();

  action_exec(w,action,NULL,wintree_from_id(wid));
  return TRUE;
}

gboolean layout_widget_button_cb ( GtkWidget *widget, struct layout_widget *lw )
{
  action_exec(lw->widget,&(lw->action[0]),NULL,
      wintree_from_id(wintree_get_focus()));
  return TRUE;
}

gboolean layout_widget_click_cb ( GtkWidget *w, GdkEventButton *ev,
    struct layout_widget *lw )
{
  if(GTK_IS_BUTTON(w) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(lw->widget,&(lw->action[ev->button-1]),(GdkEvent *)ev,
        wintree_from_id(wintree_get_focus()));
  return TRUE;
}

gboolean layout_widget_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    struct layout_widget *lw )
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
    action_exec(lw->widget,&(lw->action[button-1]),(GdkEvent *)event,
        wintree_from_id(wintree_get_focus()));

  return TRUE;
}

gpointer layout_scanner_thread ( gpointer data )
{
  GList *iter;
  gint64 timer;

  while ( TRUE )
  {
    scanner_expire();
    layout_widgets_update(data);
    if(!widget_list)
      g_thread_exit(NULL);
    timer = G_MAXINT64;
    for(iter=widget_list;iter!=NULL;iter=g_list_next(iter))
      timer = MIN(timer,((struct layout_widget *)iter->data)->next_poll);
    timer -= g_get_monotonic_time();
    if(timer>0)
      usleep(timer);
  }
}

struct layout_widget *layout_widget_new ( void )
{
  struct layout_widget *lw;
  lw = g_malloc0(sizeof(struct layout_widget));
  lw->interval = 1000000;
  lw->dir = GTK_POS_RIGHT;
  lw->rect.w = 1;
  lw->rect.h = 1;
  return lw;
}

GtkWidget *layout_widget_config ( struct layout_widget *lw, GtkWidget *parent,
    GtkWidget *sibling )
{
  gint dir;
  GtkWidget *tmp;
  lw->lobject = lw->widget;

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
    taskbar_init(lw);

  if(lw->wtype==G_TOKEN_PAGER)
    pager_init(lw->widget);

  if(lw->wtype==G_TOKEN_TRAY)
    sni_init(lw->widget);

  if(layout_widget_has_actions(lw))
  {
    lw->lobject = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(lw->lobject),lw->widget);
    gtk_widget_add_events(GTK_WIDGET(lw->lobject),GDK_SCROLL_MASK);
    g_signal_connect(G_OBJECT(lw->lobject),"button_press_event",
        G_CALLBACK(layout_widget_click_cb),lw);
    g_signal_connect(G_OBJECT(lw->lobject),"scroll-event",
      G_CALLBACK(layout_widget_scroll_cb),lw);
    if(GTK_IS_BUTTON(lw->widget))
      g_signal_connect(G_OBJECT(lw->widget),"clicked",
        G_CALLBACK(layout_widget_button_cb),lw);
  }

  widget_set_css(lw->widget);

  if(GTK_IS_BUTTON(lw->widget))
  {
    tmp = lw->widget;
    lw->widget = scale_image_new();
    gtk_container_add(GTK_CONTAINER(tmp),lw->widget);
  }

  if(parent)
  {
    gtk_widget_style_get(parent,"direction",&dir,NULL);
    if( (lw->rect.x < 1) || (lw->rect.y < 1 ) )
      gtk_grid_attach_next_to(GTK_GRID(parent),lw->lobject,sibling,dir,1,1);
    else
      gtk_grid_attach(GTK_GRID(parent),lw->lobject,
          lw->rect.x,lw->rect.y,lw->rect.w,lw->rect.h);
  }

  return lw->lobject;
}

void layout_widget_free ( struct layout_widget *lw )
{
  gint i;
  if(!lw)
    return;
  g_free(lw->style);
  g_free(lw->css);
  g_free(lw->value);
  for(i=0;i<MAX_BUTTON;i++)
    g_free(lw->action[i].command);
  g_free(lw->eval);
  g_free(lw);
}

gboolean layout_widget_draw ( struct layout_widget *lw )
{
  if(GTK_IS_LABEL(lw->widget))
    gtk_label_set_markup(GTK_LABEL(lw->widget),lw->eval);

  if(GTK_IS_PROGRESS_BAR(lw->widget))
    if(!g_strrstr(lw->eval,"nan"))
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lw->widget),
          g_ascii_strtod(lw->eval,NULL));

  if(GTK_IS_IMAGE(lw->widget))
  {
    scale_image_set_image(GTK_WIDGET(lw->widget),lw->eval,NULL);
    scale_image_update(GTK_WIDGET(lw->widget));
  }

  return FALSE;
}

gboolean layout_widget_cache ( gchar *expr, gchar **cache )
{
  gchar *eval;

  if(!expr)
    return FALSE;

  eval = expr_parse(expr, NULL);
  if(g_strcmp0(eval,*cache))
  {
    g_free(*cache);
    *cache = eval;
    return TRUE;
  }
  g_free(eval);
  return FALSE;
}

void layout_widgets_update ( GMainContext *gmc )
{
  GList *iter;
  gint64 ctime;
  struct layout_widget *lw;

  ctime = g_get_monotonic_time();

  for(iter=widget_list;iter!=NULL;iter=g_list_next(iter))
  {
    lw = iter->data;

    if(lw->next_poll > ctime)
      continue;
    lw->next_poll = ctime+lw->interval;
      if(layout_widget_cache(lw->value,&lw->eval))
        g_main_context_invoke(gmc,(GSourceFunc)layout_widget_draw,lw);
  }
}

void layout_widget_attach ( struct layout_widget *lw )
{
  guint vcount;
  gboolean updatable;

  if(!lw->value && !layout_widget_has_actions(lw))
    return layout_widget_free(lw);

  if(lw->value)
  {
    lw->eval = expr_parse(lw->value, &vcount);
    layout_widget_draw(lw);
    updatable = GTK_IS_LABEL(lw->widget) ||
      GTK_IS_PROGRESS_BAR(lw->widget) ||
      GTK_IS_IMAGE(lw->widget);

    if(!vcount && !layout_widget_has_actions(lw))
      return layout_widget_free(lw);

    if(!updatable || !vcount)
    {
      g_free(lw->value);
      lw->value = NULL;
    }
    g_free(lw->eval);
    lw->eval = NULL;
  }

  widget_list = g_list_append(widget_list,lw);
}

void widget_set_css ( GtkWidget *widget )
{
  gboolean expand;
  gdouble xalign;
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

  if(GTK_IS_LABEL(widget))
  {
    gtk_widget_style_get(widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(widget),xalign);
  }

}
