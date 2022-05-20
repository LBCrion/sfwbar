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
    gpointer wid, guint16 *state )
{
  GdkGravity wanchor, manchor;

  if(!menu || !widget)
    return;

  if(state)
    g_object_set_data( G_OBJECT(menu), "state", GUINT_TO_POINTER(*state) );
  g_object_set_data( G_OBJECT(menu), "wid", wid );
  g_object_set_data( G_OBJECT(menu), "caller", widget );

  switch(bar_get_toplevel_dir(widget))
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
  static const gint8 act_check[MAX_BUTTON*sizeof(struct layout_action *)];
  return memcmp(lw->actions,act_check,sizeof(struct layout_action *)*MAX_BUTTON);
}

gboolean widget_menu_action ( GtkWidget *w ,struct layout_action *action )
{
  GtkWidget *parent = gtk_widget_get_ancestor(w,GTK_TYPE_MENU);
  gpointer wid;
  guint16 state;
  GtkWidget *widget;

  if(parent)
  {
    wid = g_object_get_data ( G_OBJECT(parent), "wid" );
    state = GPOINTER_TO_UINT(g_object_get_data ( G_OBJECT(parent), "state" ));
    widget = g_object_get_data ( G_OBJECT(parent),"caller" );
  }
  else
  {
    wid = NULL;
    state = 0;
    widget = NULL;
  }

  if(!wid)
    wid = wintree_get_focus();

  action_exec(widget,action,NULL,wintree_from_id(wid),&state);
  return TRUE;
}

gboolean layout_widget_button_cb ( GtkWidget *widget, struct layout_widget *lw )
{
  action_exec(lw->widget,lw->actions[1],NULL,
      wintree_from_id(wintree_get_focus()),NULL);
  return TRUE;
}

gboolean layout_widget_click_cb ( GtkWidget *w, GdkEventButton *ev,
    struct layout_widget *lw )
{
  if(GTK_IS_BUTTON(w) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(lw->widget,lw->actions[ev->button],(GdkEvent *)ev,
        wintree_from_id(wintree_get_focus()),NULL);
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
    action_exec(lw->widget,lw->actions[button],(GdkEvent *)event,
        wintree_from_id(wintree_get_focus()),NULL);

  return TRUE;
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

gboolean layout_widget_draw ( struct layout_widget *lw )
{
  if(GTK_IS_LABEL(lw->widget))
    gtk_label_set_markup(GTK_LABEL(lw->widget),lw->evalue);

  if(GTK_IS_PROGRESS_BAR(lw->widget))
    if(!g_strrstr(lw->evalue,"nan"))
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lw->widget),
          g_ascii_strtod(lw->evalue,NULL));

  if(GTK_IS_IMAGE(lw->widget))
  {
    scale_image_set_image(GTK_WIDGET(lw->widget),lw->evalue,NULL);
    scale_image_update(GTK_WIDGET(lw->widget));
  }

  return FALSE;
}

gboolean layout_tooltip_update ( GtkWidget *widget, gint x, gint y,
    gboolean kbmode, GtkTooltip *tooltip, struct layout_widget *lw )
{
  gchar *eval;

  eval = expr_parse(lw->tooltip, NULL);
  if(eval)
  {
    gtk_tooltip_set_markup(tooltip,eval);
    g_free(eval);
  }

  return TRUE;
}

void layout_widget_set_tooltip ( struct layout_widget *lw )
{
  gchar *eval;
  guint vcount;

  if(!lw)
    return;

  if(lw->tooltip)
  {
    eval = expr_parse(lw->tooltip, &vcount);
    if(eval)
    {
      gtk_widget_set_has_tooltip(lw->widget,TRUE);
      gtk_widget_set_tooltip_markup(lw->widget,eval);
      g_free(eval);
    }
    if(!vcount)
    {
      g_free(lw->tooltip);
      lw->tooltip = NULL;
    }
    else
      lw->tooltip_h = g_signal_connect(lw->widget,"query-tooltip",
          G_CALLBACK(layout_tooltip_update),lw);
  }
  else
    gtk_widget_set_has_tooltip(lw->widget,FALSE);


  if(!lw->tooltip && lw->tooltip_h)
  {
    g_signal_handler_disconnect(lw->widget,lw->tooltip_h);
    lw->tooltip_h = 0;
  }
}

GtkWidget *layout_widget_config ( struct layout_widget *lw, GtkWidget *parent,
    GtkWidget *sibling )
{
  gint dir;
  guint vcount;
  GtkWidget *tmp;
  lw->lobject = lw->widget;

  if(lw->style)
  {
    lw->estyle = expr_parse(lw->style, &vcount);
    gtk_widget_set_name(lw->widget,lw->estyle);
    if(!vcount)
    {
      g_free(lw->style);
      g_free(lw->estyle);
      lw->style = NULL;
      lw->estyle = NULL;
    }
  }

  layout_widget_set_tooltip(lw);

  if(lw->css)
  {
    GtkStyleContext *context = gtk_widget_get_style_context (lw->widget);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,lw->css,strlen(lw->css),NULL);
    gtk_style_context_add_provider (context,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
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
    g_signal_connect(G_OBJECT(lw->lobject),"button-press-event",
        G_CALLBACK(layout_widget_click_cb),lw);
    g_signal_connect(G_OBJECT(lw->lobject),"scroll-event",
      G_CALLBACK(layout_widget_scroll_cb),lw);
    if(GTK_IS_BUTTON(lw->widget))
      g_signal_connect(G_OBJECT(lw->widget),"clicked",
        G_CALLBACK(layout_widget_button_cb),lw);
  }

  widget_set_css(lw->widget,FALSE);

  if(GTK_IS_BUTTON(lw->widget))
  {
    tmp = lw->widget;
    lw->widget = scale_image_new();
    gtk_container_add(GTK_CONTAINER(tmp),lw->widget);
  }

  if(lw->value)
  {
    lw->evalue = expr_parse(lw->value, &vcount);
    layout_widget_draw(lw);
    if((!GTK_IS_LABEL(lw->widget) && !GTK_IS_PROGRESS_BAR(lw->widget) &&
          !GTK_IS_IMAGE(lw->widget)) || !vcount)
    {
      g_free(lw->value);
      g_free(lw->evalue);
      lw->value = NULL;
      lw->evalue = NULL;
    }
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
  g_free(lw->tooltip);
  for(i=0;i<MAX_BUTTON;i++)
    action_free(lw->actions[i],NULL);
  g_free(lw->evalue);
  g_free(lw->estyle);
  g_free(lw);
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

void layout_widgets_autoexec ( GtkWidget *widget, gpointer data )
{
  struct layout_widget *lw;

  if(GTK_IS_CONTAINER(widget))
    gtk_container_forall(GTK_CONTAINER(widget),layout_widgets_autoexec,data);

  lw = g_object_get_data(G_OBJECT(widget),"layout_widget");

  if(lw)
    action_exec(lw->widget,lw->actions[0],NULL,
        wintree_from_id(wintree_get_focus()),NULL);

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

    if( lw->trigger || lw->next_poll > ctime)
      continue;

    while(lw->next_poll <= ctime)
      lw->next_poll += lw->interval;

    if(layout_widget_cache(lw->value,&lw->evalue))
      g_main_context_invoke(gmc,(GSourceFunc)layout_widget_draw,lw);
    if(layout_widget_cache(lw->style,&lw->estyle))
      gtk_widget_set_name(lw->widget,lw->estyle);
  }
}

void layout_emit_trigger ( gchar *trigger )
{
  GList *iter;
  struct layout_widget *lw;
  struct layout_action *action;

  if(!trigger)
    return;

  scanner_expire();
  for(iter=widget_list;iter!=NULL;iter=g_list_next(iter))
  {
    lw = iter->data;
    if(!lw->trigger || g_ascii_strcasecmp(trigger,lw->trigger))
      continue;

    if(layout_widget_cache(lw->value,&lw->evalue))
      layout_widget_draw(lw);
    if(layout_widget_cache(lw->style,&lw->estyle))
      gtk_widget_set_name(lw->widget,lw->estyle);
  }
  action = action_trigger_lookup(trigger);
  if(action)
    action_exec(NULL,action,NULL,NULL,NULL);
}

void layout_widget_attach ( struct layout_widget *lw )
{
  if(!lw->value && !lw->tooltip && !lw->style && !layout_widget_has_actions(lw))
    return layout_widget_free(lw);

  g_object_set_data(G_OBJECT(lw->widget),"layout_widget",lw);

  if(lw->value || lw->style)
    widget_list = g_list_append(widget_list,lw);
  else
    widget_list = g_list_remove(widget_list,lw);
}

void widget_set_css ( GtkWidget *widget, gboolean propagate )
{
  gboolean expand;
  gdouble xalign;
  GList *l;

  gtk_widget_style_get(widget,"hexpand",&expand,NULL);
  gtk_widget_set_hexpand(GTK_WIDGET(widget),expand);
  gtk_widget_style_get(widget,"vexpand",&expand,NULL);
  gtk_widget_set_vexpand(GTK_WIDGET(widget),expand);

  if(GTK_IS_CONTAINER(widget) && propagate)
  {
    l = gtk_container_get_children(GTK_CONTAINER(widget));
    for(;l!=NULL;l=g_list_next(l))
      widget_set_css(l->data,propagate);
    g_list_free(l);
  }

  if(GTK_IS_LABEL(widget))
  {
    gtk_widget_style_get(widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(widget),xalign);
  }
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
      if(!((struct layout_widget *)iter->data)->trigger)
        timer = MIN(timer,((struct layout_widget *)iter->data)->next_poll);
    timer -= g_get_monotonic_time();
    if(timer>0)
      usleep(timer);
  }
}
