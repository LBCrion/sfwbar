/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gtk-layer-shell.h>
#include "expr.h"
#include "basewidget.h"
#include "flowgrid.h"
#include "action.h"
#include "module.h"

G_DEFINE_TYPE_WITH_CODE (BaseWidget, base_widget, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE (BaseWidget));

static const GdkModifierType mod_mask = GDK_SHIFT_MASK | GDK_CONTROL_MASK |
  GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK |
  GDK_MOD5_MASK;
static GHashTable *widgets_id;
static GList *widgets_scan;
static GMutex widget_mutex;

static void base_widget_attachment_free ( base_widget_attachment_t *attach )
{
  if(!attach)
    return;

  action_free(attach->action, NULL);
  g_free(attach);
}

static base_widget_attachment_t *base_widget_attachment_dup (
    base_widget_attachment_t *src )
{
  base_widget_attachment_t *dest;

  if(!src)
    return NULL;

  dest = g_malloc0(sizeof(base_widget_attachment_t));
  dest->event = src->event;
  dest->mods = src->mods;
  dest->action = action_dup(src->action);

  return dest;
}

static void base_widget_destroy ( GtkWidget *self )
{
  BaseWidgetPrivate *priv,*ppriv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_mutex_lock(&widget_mutex);
  widgets_scan = g_list_remove(widgets_scan,self);
  g_mutex_unlock(&widget_mutex);

  if(priv->mirror_parent)
  {
    ppriv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));
    ppriv->mirror_children = g_list_remove(ppriv->mirror_children,self);
    priv->mirror_parent = NULL;
  }

  if(widgets_id && priv->id)
    g_hash_table_remove(widgets_id,priv->id);

  g_list_free_full(priv->css,g_free);
  priv->css = NULL;
  g_clear_pointer(&priv->id,g_free);
  g_clear_pointer(&priv->value,expr_cache_free);
  g_clear_pointer(&priv->style,expr_cache_free);
  g_clear_pointer(&priv->tooltip,expr_cache_free);
  g_clear_pointer(&priv->trigger,g_free);
  g_list_free_full(g_steal_pointer(&priv->actions),
      (GDestroyNotify)base_widget_attachment_free);

  GTK_WIDGET_CLASS(base_widget_parent_class)->destroy(self);
}

static void base_widget_size_allocate ( GtkWidget *self, GtkAllocation *alloc )
{
  BaseWidgetPrivate *priv;
  gint m, n;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->maxw)
  {
    gtk_widget_get_preferred_width(gtk_bin_get_child(GTK_BIN(self)),&m,&n);
    alloc->width = MAX(m,MIN(priv->maxw,alloc->width));
  }
  if(priv->maxh)
  {
    gtk_widget_get_preferred_height(gtk_bin_get_child(GTK_BIN(self)),&m,&n);
    alloc->height = MAX(m,MIN(priv->maxh,alloc->height));
  }
  BASE_WIDGET_GET_CLASS(self)->old_size_allocate(self,alloc);
}

static void base_widget_get_pref_width ( GtkWidget *self, gint *m, gint *n )
{
  BaseWidgetPrivate *priv;
  GtkWidget *child;

  *m = 0;
  *n = 0;
  g_return_if_fail(IS_BASE_WIDGET(self));

  child = gtk_bin_get_child(GTK_BIN(self));
  if(child && gtk_widget_get_visible(child))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(self));
    gtk_widget_get_preferred_width(child,m,n);
    if(priv->maxw)
      *n = MAX(*m,MIN(*n,priv->maxw));
  }
}

static void base_widget_get_pref_height ( GtkWidget *self, gint *m, gint *n )
{
  BaseWidgetPrivate *priv;
  GtkWidget *child;

  *m = 0;
  *n = 0;
  g_return_if_fail(IS_BASE_WIDGET(self));

  child = gtk_bin_get_child(GTK_BIN(self));
  if(child && gtk_widget_get_visible(child))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(self));
    gtk_widget_get_preferred_height(child,m,n);
    if(priv->maxh)
      *n = MAX(*m,MIN(*n,priv->maxh));
  }
}

static void base_widget_class_init ( BaseWidgetClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = base_widget_destroy;
  kclass->old_size_allocate = GTK_WIDGET_CLASS(kclass)->size_allocate;
  GTK_WIDGET_CLASS(kclass)->size_allocate = base_widget_size_allocate;
  GTK_WIDGET_CLASS(kclass)->get_preferred_width = base_widget_get_pref_width;
  GTK_WIDGET_CLASS(kclass)->get_preferred_height = base_widget_get_pref_height;
}

static void base_widget_init ( BaseWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  priv->value = expr_cache_new();
  priv->style = expr_cache_new();
  priv->tooltip = expr_cache_new();
  priv->interval = 1000000;
  priv->dir = GTK_POS_RIGHT;
  priv->rect.x = -1;
  priv->rect.y = -1;
  priv->rect.width = 1;
  priv->rect.height = 1;
}

GdkModifierType base_widget_get_modifiers ( GtkWidget *self )
{
  GdkModifierType state;
  GtkWindow *win;

  win = GTK_WINDOW(gtk_widget_get_ancestor(self, GTK_TYPE_WINDOW));
  if(gtk_window_get_window_type(win)==GTK_WINDOW_POPUP)
    win = g_object_get_data(G_OBJECT(win),"parent_window");

  if(win || !gtk_layer_is_layer_window(win))
  {
    gtk_layer_set_keyboard_mode(win, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    gtk_main_iteration();
    gtk_main_iteration();
    gtk_main_iteration();
    state = gdk_keymap_get_modifier_state(gdk_keymap_get_for_display(
          gdk_display_get_default())) & mod_mask;
    gtk_layer_set_keyboard_mode(win, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
  }
  else
    state = 0;

  return state;
}

static gboolean base_widget_button_cb ( GtkWidget *self, GdkEventButton *ev,
    gpointer data )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(data),FALSE);

  priv = base_widget_get_instance_private(BASE_WIDGET(data));

  if(ev->button == 1)
    priv->saved_modifiers = base_widget_get_modifiers(self);

  return FALSE;
}

static gboolean base_widget_click_cb ( GtkWidget *self, GdkEventButton *ev,
    gpointer data )
{
  BaseWidgetPrivate *priv;
  action_t *action;
  GdkModifierType mods;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(GTK_IS_BUTTON(base_widget_get_child(self)) && ev->button == 1) 
  {
    if(ev->type != GDK_BUTTON_RELEASE)
      return FALSE;
    mods = priv->saved_modifiers;
  }
  else if (ev->type != GDK_BUTTON_PRESS || ev->button < 1 || ev->button > 3 )
    return FALSE;
  else
    mods = base_widget_get_modifiers(self);

  action = base_widget_get_action(self, ev->button, mods);
  if(action)
    action_exec(self, action, (GdkEvent *)ev,
        wintree_from_id(wintree_get_focus()),NULL);

  return TRUE;
}

static gboolean base_widget_scroll_cb ( GtkWidget *self,
    GdkEventScroll *ev, gpointer *data )
{
  action_t *action;
  gint button;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  switch(ev->direction)
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
      return FALSE;
  }

  action = base_widget_get_action(self, button,
      base_widget_get_modifiers(self));
  if(action)
    action_exec(self, action, (GdkEvent *)ev,
        wintree_from_id(wintree_get_focus()),NULL);

  return TRUE;
}
static gboolean base_widget_tooltip_update ( GtkWidget *self,
    gint x, gint y, gboolean kbmode, GtkTooltip *tooltip, gpointer data )
{
  BaseWidgetPrivate *priv; 

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if(!priv->tooltip)
    return FALSE;
  expr_cache_eval(priv->tooltip);
  if(!priv->tooltip->cache)
    return FALSE;

  gtk_tooltip_set_markup(tooltip, priv->tooltip->cache);

  return TRUE;
}

GtkWidget *base_widget_get_child ( GtkWidget *self )
{
  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);

  if(BASE_WIDGET_GET_CLASS(self)->get_child)
    return BASE_WIDGET_GET_CLASS(self)->get_child(self);
  else
    return NULL;
}

gboolean base_widget_update_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(BASE_WIDGET_GET_CLASS(self)->update_value)
    BASE_WIDGET_GET_CLASS(self)->update_value(self);

  for(iter=priv->mirror_children;iter;iter=g_list_next(iter))
    BASE_WIDGET_GET_CLASS(self)->update_value(iter->data);

  return FALSE;
}

gboolean base_widget_style ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;
  GtkWidget *child;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  if(!BASE_WIDGET_GET_CLASS(self)->get_child)
    return FALSE;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  child = BASE_WIDGET_GET_CLASS(self)->get_child(self);
  if(priv->mirror_parent)
    priv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));
  gtk_widget_set_name(child,priv->style->cache);
  css_widget_cascade(child,NULL);

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  for(iter=priv->mirror_children;iter;iter=g_list_next(iter))
    base_widget_style(iter->data);

  return FALSE;
}

void base_widget_set_tooltip ( GtkWidget *self, gchar *tooltip )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!priv->tooltip)
    return;

  g_free(priv->tooltip->definition);
  priv->tooltip->definition = tooltip;
  priv->tooltip->eval = TRUE;
  priv->value->widget = self;

  if(!tooltip)
  {
    gtk_widget_set_has_tooltip(self,FALSE);
    return;
  }

  if(expr_cache_eval(priv->tooltip))
  {
    gtk_widget_set_has_tooltip(self,TRUE);
    gtk_widget_set_tooltip_markup(self,priv->tooltip->cache);
  }
  if(!priv->tooltip_h)
    priv->tooltip_h = g_signal_connect(self,"query-tooltip",
        G_CALLBACK(base_widget_tooltip_update),self);
}

void base_widget_set_value ( GtkWidget *self, gchar *value )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_free(priv->value->definition);
  priv->value->definition = value;
  priv->value->eval = TRUE;
  priv->value->widget = self;

  if(expr_cache_eval(priv->value) || priv->always_update)
    base_widget_update_value(self);

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan,self))
    widgets_scan = g_list_append(widgets_scan,self);
  g_mutex_unlock(&widget_mutex);
}

void base_widget_set_style ( GtkWidget *self, gchar *style )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_free(priv->style->definition);
  priv->style->definition = style;
  priv->style->eval = TRUE;
  priv->value->widget = self;

  if(expr_cache_eval(priv->style))
  {
    base_widget_style(self);
  }

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan,self))
    widgets_scan = g_list_append(widgets_scan,self);
  g_mutex_unlock(&widget_mutex);
}

void base_widget_set_trigger ( GtkWidget *self, gchar *trigger )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_free(priv->trigger);
  priv->trigger = trigger;
}

void base_widget_set_id ( GtkWidget *self, gchar *id )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_free(priv->id);
  priv->id = id;

  if(!widgets_id)
    widgets_id = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal,g_free,NULL);
  if(!g_hash_table_lookup(widgets_id,id))
    g_hash_table_insert(widgets_id,g_strdup(id),self);
}

GtkWidget *base_widget_from_id ( gchar *id )
{
  if(!widgets_id || !id)
    return NULL;

  return g_hash_table_lookup(widgets_id,id);
}

void base_widget_set_interval ( GtkWidget *self, gint64 interval )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->interval = interval;
}

void base_widget_set_state ( GtkWidget *self, guint16 mask, gboolean state )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(state)
    priv->user_state |= mask;
  else
    priv->user_state &= ~mask;
}

void base_widget_set_rect ( GtkWidget *self, GdkRectangle rect )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->rect = rect;
}

void base_widget_attach ( GtkWidget *parent, GtkWidget *self,
    GtkWidget *sibling )
{
  BaseWidgetPrivate *priv;
  gint dir;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(parent)
  {
    gtk_widget_style_get(parent,"direction",&dir,NULL);
    if( (priv->rect.x < 0) || (priv->rect.y < 0 ) )
      gtk_grid_attach_next_to(GTK_GRID(parent),self,sibling,dir,1,1);
    else
      gtk_grid_attach(GTK_GRID(parent),self,
          priv->rect.x,priv->rect.y,priv->rect.width,priv->rect.height);
  }
}

void base_widget_set_max_width ( GtkWidget *self, guint x )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->maxw = x;
}

void base_widget_set_max_height ( GtkWidget *self, guint x )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->maxh = x;
}

gchar *base_widget_get_id ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->id;
}

guint16 base_widget_get_state ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->user_state;
}

void base_widget_set_always_update ( GtkWidget *self, gboolean update )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->always_update = update;
}

void base_widget_set_next_poll ( GtkWidget *self, gint64 ctime )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->trigger)
    return;

  while(priv->next_poll <= ctime)
    priv->next_poll += priv->interval;
}

gint64 base_widget_get_next_poll ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),G_MAXINT64);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->trigger)
    return G_MAXINT64;

  if(!priv->value->eval && !priv->style->eval)
    return G_MAXINT64;

  return priv->next_poll;
}

gchar *base_widget_get_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->mirror_parent)
    priv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));

  return priv->value->cache;
}

action_t *base_widget_get_action ( GtkWidget *self, gint n,
    GdkModifierType mods )
{
  BaseWidgetPrivate *priv;
  base_widget_attachment_t *attach;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  for(iter=priv->actions; iter; iter=g_list_next(iter))
  {
    attach = iter->data;
    if(attach->event == n && attach->mods == mods)
      return attach->action;
  }

  return NULL;
}

void base_widget_set_css ( GtkWidget *self, gchar *css )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!css)
    return;

  priv->css = g_list_append(priv->css,g_strdup(css));
  css_widget_apply(base_widget_get_child(self),css);
}

void base_widget_set_action ( GtkWidget *self, gint n, GdkModifierType mods,
    action_t *action )
{
  BaseWidgetPrivate *priv;
  base_widget_attachment_t *attach;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(n<0 || n>=8)
    return;

  for(iter=priv->actions; iter; iter=g_list_next(iter))
  {
    attach = iter->data;
    if(attach->event == n && attach->mods == mods)
      break;
  }

  if(iter)
  {
    attach = iter->data;
    action_free(attach->action, NULL);
  }
  else
  {
    attach = g_malloc0(sizeof(base_widget_attachment_t));
    attach->event = n;
    attach->mods = mods;
    priv->actions = g_list_prepend(priv->actions, attach);

  }
  attach->action = action;

  if(IS_FLOW_GRID(base_widget_get_child(self)))
    return;

  if(!priv->scroll_h && n>=4 && n<=7)
  {
    gtk_widget_add_events(GTK_WIDGET(self),GDK_SCROLL_MASK);
    priv->scroll_h = g_signal_connect(G_OBJECT(self),"scroll-event",
      G_CALLBACK(base_widget_scroll_cb),NULL);
  }

  gtk_widget_add_events(GTK_WIDGET(self), GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  if(!priv->click_h && (n==2 || n==3 ||
        (n==1 && !GTK_IS_BUTTON(base_widget_get_child(self)))))
    priv->click_h = g_signal_connect(G_OBJECT(self),"button-press-event",
        G_CALLBACK(base_widget_click_cb),NULL);

  if(!priv->button_h && GTK_IS_BUTTON(base_widget_get_child(self)) && n==1)
    priv->button_h = g_signal_connect(G_OBJECT(self),"button-release-event",
        G_CALLBACK(base_widget_click_cb),NULL);
  if(!priv->buttonp_h && GTK_IS_BUTTON(base_widget_get_child(self)) && n==1)
    priv->buttonp_h = g_signal_connect(G_OBJECT(base_widget_get_child(self)),
        "button-press-event", G_CALLBACK(base_widget_button_cb),self);
}

void base_widget_copy_actions ( GtkWidget *dest, GtkWidget *src )
{
  BaseWidgetPrivate *spriv, *dpriv;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(dest) && IS_BASE_WIDGET(src));
  spriv = base_widget_get_instance_private(BASE_WIDGET(src));
  dpriv = base_widget_get_instance_private(BASE_WIDGET(dest));

  for(iter=spriv->actions; iter; iter=g_list_next(iter))
    dpriv->actions = g_list_prepend(dpriv->actions,
        base_widget_attachment_dup(iter->data));
}

void base_widget_copy_properties ( GtkWidget *dest, GtkWidget *src )
{
  BaseWidgetPrivate *spriv, *dpriv;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(dest) && IS_BASE_WIDGET(src));
  spriv = base_widget_get_instance_private(BASE_WIDGET(src));
  dpriv = base_widget_get_instance_private(BASE_WIDGET(dest));

  base_widget_copy_actions(dest, src);

  if(spriv->tooltip)
    base_widget_set_tooltip( dest, spriv->tooltip->definition );
  if(spriv->trigger)
    base_widget_set_trigger( dest, spriv->trigger );
  else
    base_widget_set_interval( dest, spriv->interval );
  base_widget_set_max_width( dest, spriv->maxw );
  base_widget_set_max_height( dest, spriv->maxh );
  base_widget_set_state( dest, spriv->user_state, TRUE );
  base_widget_set_rect( dest, spriv->rect );
  for(iter=spriv->css;iter;iter=g_list_next(iter))
    css_widget_apply(base_widget_get_child(dest),iter->data);
  if(!g_list_find(spriv->mirror_children, dest))
  {
    spriv->mirror_children = g_list_prepend(spriv->mirror_children, dest);
    dpriv->mirror_parent = src;
    base_widget_style(dest);
    base_widget_update_value(dest);
  }
}

GtkWidget *base_widget_mirror ( GtkWidget *src )
{
  g_return_val_if_fail(IS_BASE_WIDGET(src),NULL);
  if(!BASE_WIDGET_GET_CLASS(src)->mirror)
    return NULL;

  return BASE_WIDGET_GET_CLASS(src)->mirror(src);
}

gboolean base_widget_emit_trigger ( gchar *trigger )
{
  BaseWidgetPrivate *priv;
  GList *iter;
  action_t *action;

  if(!trigger)
    return FALSE;
  g_debug("trigger: %s",trigger);

  scanner_invalidate();
  g_mutex_lock(&widget_mutex);
  for(iter=widgets_scan;iter!=NULL;iter=g_list_next(iter))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(iter->data));
    if(!priv->trigger || g_ascii_strcasecmp(trigger,priv->trigger))
      continue;
    if(expr_cache_eval(priv->value) || priv->always_update)
      base_widget_update_value(iter->data);
    if(expr_cache_eval(priv->style))
      base_widget_style(iter->data);
  }
  g_mutex_unlock(&widget_mutex);
  action = action_trigger_lookup(trigger);
  if(action)
    action_exec(NULL,action,NULL,NULL,NULL);

  return FALSE;
}

gpointer base_widget_scanner_thread ( GMainContext *gmc )
{
  BaseWidgetPrivate *priv;
  GList *iter;
  gint64 timer, ctime;

  while ( TRUE )
  {
    scanner_invalidate();
    module_invalidate_all();
    timer = G_MAXINT64;
    ctime = g_get_monotonic_time();

    g_mutex_lock(&widget_mutex);
    for(iter=widgets_scan;iter!=NULL;iter=g_list_next(iter))
    {
      priv = base_widget_get_instance_private(BASE_WIDGET(iter->data));
      if(base_widget_get_next_poll(iter->data)<=ctime)
      {
        if(expr_cache_eval(priv->value) || priv->always_update)
          g_main_context_invoke(gmc,(GSourceFunc)base_widget_update_value,
              iter->data);
        if(expr_cache_eval(priv->style))
          g_main_context_invoke(gmc,(GSourceFunc)base_widget_style,
              iter->data);
        base_widget_set_next_poll(iter->data,ctime);
      }
      timer = MIN(timer,base_widget_get_next_poll(iter->data));
    }
    g_mutex_unlock(&widget_mutex);

    timer -= g_get_monotonic_time();
    if(timer>0)
      g_usleep(timer);
  }
}

void base_widget_autoexec ( GtkWidget *self, gpointer data )
{
  action_t *action;

  if(GTK_IS_CONTAINER(self))
    gtk_container_forall(GTK_CONTAINER(self),base_widget_autoexec, data);

  if(!IS_BASE_WIDGET(self))
    return;

  action = base_widget_get_action(self,0,0);
  if(action)
    action_exec(self,action,NULL,wintree_from_id(wintree_get_focus()),NULL);
}
