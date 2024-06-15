/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "expr.h"
#include "basewidget.h"
#include "grid.h"
#include "flowgrid.h"
#include "action.h"
#include "module.h"
#include "meson.h"
#include "bar.h"

G_DEFINE_TYPE_WITH_CODE (BaseWidget, base_widget, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE (BaseWidget))

static GHashTable *base_widget_id_map;
static GList *widgets_scan;
static GMutex widget_mutex;
static gint64 base_widget_default_id = 0;

static void base_widget_attachment_free ( base_widget_attachment_t *attach )
{
  if(!attach)
    return;

  action_free(attach->action, NULL);
  g_free(attach);
}

static void base_widget_destroy ( GtkWidget *self )
{
  BaseWidgetPrivate *priv,*ppriv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_mutex_lock(&widget_mutex);
  widgets_scan = g_list_remove(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);

  if(priv->mirror_parent)
  {
    ppriv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));
    ppriv->mirror_children = g_list_remove(ppriv->mirror_children, self);
    priv->mirror_parent = NULL;
  }

  if(base_widget_id_map && priv->id)
    g_hash_table_remove(base_widget_id_map, priv->id);

  g_list_free_full(priv->css, g_free);
  priv->css = NULL;
  g_clear_pointer(&priv->id, g_free);
  g_clear_pointer(&priv->value, expr_cache_free);
  g_clear_pointer(&priv->style, expr_cache_free);
  g_clear_pointer(&priv->tooltip, expr_cache_free);
  g_list_free_full(g_steal_pointer(&priv->actions),
      (GDestroyNotify)base_widget_attachment_free);
  priv->trigger = NULL;

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
    gtk_widget_get_preferred_width(base_widget_get_child(self), &m, &n);
    alloc->width = MAX(m,MIN(priv->maxw, alloc->width));
  }
  if(priv->maxh)
  {
    gtk_widget_get_preferred_height(base_widget_get_child(self), &m, &n);
    alloc->height = MAX(m,MIN(priv->maxh, alloc->height));
  }
  BASE_WIDGET_GET_CLASS(self)->old_size_allocate(self, alloc);
}

static void base_widget_get_pref_width ( GtkWidget *self, gint *m, gint *n )
{
  BaseWidgetPrivate *priv;
  GtkWidget *child;

  *m = 0;
  *n = 0;
  g_return_if_fail(IS_BASE_WIDGET(self));

  child = base_widget_get_child(self);
  if(child && gtk_widget_get_visible(child))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(self));
    gtk_widget_get_preferred_width(child, m, n);
    if(priv->maxw)
      *n = MAX(*m, MIN(*n, priv->maxw));
  }
}

static void base_widget_get_pref_height ( GtkWidget *self, gint *m, gint *n )
{
  BaseWidgetPrivate *priv;
  GtkWidget *child;

  *m = 0;
  *n = 0;
  g_return_if_fail(IS_BASE_WIDGET(self));

  child = base_widget_get_child(self);
  if(child && gtk_widget_get_visible(child))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(self));
    gtk_widget_get_preferred_height(child, m, n);
    if(priv->maxh)
      *n = MAX(*m, MIN(*n, priv->maxh));
  }
}

GdkModifierType base_widget_get_modifiers ( GtkWidget *self )
{
  GdkModifierType state;

  bar_set_interactivity(self, TRUE);
  bar_set_interactivity(self, TRUE);
  state = gdk_keymap_get_modifier_state(gdk_keymap_get_for_display(
        gdk_display_get_default())) & gtk_accelerator_get_default_mod_mask();
  bar_set_interactivity(self, FALSE);

  g_debug("modifier state: %x", state);
  return state;
}

gboolean base_widget_check_action_slot ( GtkWidget *self, gint slot )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  for(iter=priv->actions; iter; iter=g_list_next(iter))
    if(((base_widget_attachment_t *)(iter->data))->event == slot)
      break;

  return !!iter;
}

static gboolean base_widget_button_release_event ( GtkWidget *self,
    GdkEventButton *ev )
{
  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  if (ev->type != GDK_BUTTON_RELEASE || ev->button < 1 || ev->button > 3 )
    return FALSE;

  return base_widget_action_exec(self, ev->button, (GdkEvent *)ev);
}

static gboolean base_widget_scroll_event ( GtkWidget *self,
    GdkEventScroll *ev )
{
  gint slot;

  g_return_val_if_fail(IS_BASE_WIDGET(self),FALSE);

  switch(ev->direction)
  {
    case GDK_SCROLL_UP:
      slot = 4;
      break;
    case GDK_SCROLL_DOWN:
      slot = 5;
      break;
    case GDK_SCROLL_LEFT:
      slot = 6;
      break;
    case GDK_SCROLL_RIGHT:
      slot = 7;
      break;
    default:
      return FALSE;
  }

  return base_widget_action_exec(self, slot, (GdkEvent *)ev);
}

static gboolean base_widget_action_exec_impl ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  action_t *action;

  if(!base_widget_check_action_slot(self, slot))
    return FALSE;

  action = base_widget_get_action(self, slot,
        base_widget_get_modifiers(self));
  if(!action)
    return FALSE;

  action_exec(self, action, (GdkEvent *)ev,
      wintree_from_id(wintree_get_focus()), NULL);
  return TRUE;
}

static gboolean base_widget_drag_motion( GtkWidget *self, GdkDragContext *ctx,
    gint x, gint y, guint time )
{
  BaseWidgetPrivate *priv;
  GList *target;
  gchar *name;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if (priv->is_drag_dest)
    return TRUE;

  priv->is_drag_dest = TRUE;

  for(target=gdk_drag_context_list_targets(ctx); target; target=target->next)
  {
    name = gdk_atom_name(target->data);
    if(g_str_has_prefix(name, "flow-item-"))
    {
      g_free(name);
      return TRUE;
    }
    g_free(name);
  }

  base_widget_action_exec(self, 8, NULL);
  return TRUE;
}

static void base_widget_drag_leave (GtkWidget *self,
    GdkDragContext *context, guint time )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  priv->is_drag_dest = FALSE;
}

static void base_widget_action_configure_impl ( GtkWidget *self, gint slot )
{
  if(slot>=1 && slot<=3)
    gtk_widget_add_events(GTK_WIDGET(self), GDK_BUTTON_RELEASE_MASK);
  else if(slot>=4 && slot<=7)
    gtk_widget_add_events(GTK_WIDGET(self), GDK_SCROLL_MASK);
  else if(slot==8)
  {
    if(!gtk_drag_dest_get_target_list(self))
      gtk_drag_dest_set(self, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_MOVE);
    gtk_drag_dest_set_track_motion(self, TRUE);
  }
}

static void base_widget_class_init ( BaseWidgetClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = base_widget_destroy;
  kclass->old_size_allocate = GTK_WIDGET_CLASS(kclass)->size_allocate;
  kclass->action_exec = base_widget_action_exec_impl;
  kclass->action_configure = base_widget_action_configure_impl;
  GTK_WIDGET_CLASS(kclass)->size_allocate = base_widget_size_allocate;
  GTK_WIDGET_CLASS(kclass)->get_preferred_width = base_widget_get_pref_width;
  GTK_WIDGET_CLASS(kclass)->get_preferred_height = base_widget_get_pref_height;
  GTK_WIDGET_CLASS(kclass)->button_release_event =
    base_widget_button_release_event;
  GTK_WIDGET_CLASS(kclass)->scroll_event = base_widget_scroll_event;
  GTK_WIDGET_CLASS(kclass)->drag_motion = base_widget_drag_motion;
  GTK_WIDGET_CLASS(kclass)->drag_leave = base_widget_drag_leave;
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
  base_widget_set_id(GTK_WIDGET(self), NULL);
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
  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  return gtk_bin_get_child(GTK_BIN(self));
}

void base_widget_set_local_state ( GtkWidget *self, gboolean state )
{
  GtkWidget *parent;
  BaseWidgetPrivate *priv, *ppriv;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->local_state == state)
    return;

  priv->local_state = state;
  parent = base_widget_get_mirror_parent(self);
  if(parent == self)
  {
    for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
      base_widget_set_local_state(iter->data, state);
    return;
  }

  g_mutex_lock(&widget_mutex);
  if(state)
  {
    if(!g_list_find(widgets_scan, self))
      widgets_scan = g_list_append(widgets_scan, self);
  }
  else
    widgets_scan = g_list_remove(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);

  if(state)
  {
    ppriv = base_widget_get_instance_private(BASE_WIDGET(parent));
    base_widget_set_value(self, g_strdup(ppriv->value->definition));
    base_widget_set_style(self, g_strdup(ppriv->style->definition));
  }
  else
  {
    BASE_WIDGET_GET_CLASS(self)->update_value(self);
    gtk_widget_set_name(base_widget_get_child(self),
        priv->style->cache);
    css_widget_cascade(self, NULL);
  }
}

gboolean base_widget_get_local_state ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->local_state;
}

gboolean base_widget_update_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(BASE_WIDGET_GET_CLASS(self)->update_value)
    BASE_WIDGET_GET_CLASS(self)->update_value(self);

  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    if(!base_widget_get_local_state(iter->data))
      BASE_WIDGET_GET_CLASS(self)->update_value(iter->data);

  return FALSE;
}

gboolean base_widget_style ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  self = base_widget_get_mirror_parent(self);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  gtk_widget_set_name(base_widget_get_child(self), priv->style->cache);
  css_widget_cascade(self, NULL);
  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    if(!base_widget_get_local_state(iter->data))
    {
      gtk_widget_set_name(base_widget_get_child(iter->data),
          priv->style->cache);
      css_widget_cascade(iter->data, NULL);
    }

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
    gtk_widget_set_has_tooltip(self, FALSE);
    return;
  }

  if(expr_cache_eval(priv->tooltip))
  {
    gtk_widget_set_has_tooltip(self, TRUE);
    gtk_widget_set_tooltip_markup(self, priv->tooltip->cache);
  }
  if(!priv->tooltip_h)
    priv->tooltip_h = g_signal_connect(self, "query-tooltip",
        G_CALLBACK(base_widget_tooltip_update), self);
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
  self = base_widget_get_mirror_parent(self);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_free(priv->style->definition);
  priv->style->definition = style;
  priv->style->eval = TRUE;
  priv->value->widget = self;

  if(expr_cache_eval(priv->style))
    base_widget_style(self);

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan, self))
    widgets_scan = g_list_append(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);
}

void base_widget_set_trigger ( GtkWidget *self, gchar *trigger )
{
  gchar *lower;
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  base_widget_set_interval(self, 0);
  lower = g_ascii_strdown(trigger, -1);
  priv->trigger = g_intern_string(lower);
  g_free(lower);
}

void base_widget_set_id ( GtkWidget *self, gchar *id )
{
  BaseWidgetPrivate *priv;
  GtkWidget *old;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!base_widget_id_map)
    base_widget_id_map = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, NULL);

  if(priv->id)
    g_hash_table_remove(base_widget_id_map, priv->id);

  g_free(priv->id);
  priv->id = id? id: g_strdup_printf("_w%lld",
      (long long int)base_widget_default_id++);

  old = g_hash_table_lookup(base_widget_id_map, priv->id);
  if(!old)
    g_hash_table_insert(base_widget_id_map, g_strdup(priv->id), self);
  else if (old != self)
    g_message("widget id collision: '%s'", priv->id);
}

GtkWidget *base_widget_from_id ( gchar *id )
{
  if(!base_widget_id_map || !id)
    return NULL;

  return g_hash_table_lookup(base_widget_id_map,id);
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
  GtkWidget *parent;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!memcmp(&priv->rect, &rect, sizeof(GdkRectangle)))
    return;

  priv->rect = rect;

  parent = gtk_widget_get_parent(self);
  parent = parent?gtk_widget_get_parent(parent):NULL;
  if(!parent || !IS_GRID(parent))
    return;

  g_object_ref(self);
  grid_detach(self, parent);
  gtk_container_remove(GTK_CONTAINER(base_widget_get_child(parent)), self);
  if(grid_attach(parent, self))
    g_object_unref(self);
}

void base_widget_attach ( GtkWidget *parent, GtkWidget *self,
    GtkWidget *sibling )
{
  BaseWidgetPrivate *priv;
  gint dir;

  g_return_if_fail(IS_BASE_WIDGET(self));
  g_return_if_fail(GTK_IS_GRID(parent));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  gtk_widget_style_get(parent, "direction", &dir, NULL);
  if( (priv->rect.x < 0) || (priv->rect.y < 0 ) )
    gtk_grid_attach_next_to(GTK_GRID(parent), self, sibling, dir, 1, 1);
  else
    gtk_grid_attach(GTK_GRID(parent), self,
        priv->rect.x, priv->rect.y, priv->rect.width, priv->rect.height);
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

  if(priv->trigger || !priv->interval)
    return G_MAXINT64;

  if(!priv->value->eval && !priv->style->eval)
    return G_MAXINT64;

  return priv->next_poll;
}

action_t *base_widget_get_action ( GtkWidget *self, gint n,
    GdkModifierType mods )
{
  BaseWidgetPrivate *priv;
  base_widget_attachment_t *attach;
  GList *iter;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  for(iter=priv->actions; iter; iter=g_list_next(iter))
  {
    attach = iter->data;
    if(attach->event == n && attach->mods == mods)
      return attach->action;
  }

  return NULL;
}

void base_widget_action_configure ( GtkWidget *self, gint slot )
{
  g_return_if_fail(IS_BASE_WIDGET(self));

  if(BASE_WIDGET_GET_CLASS(self)->action_configure)
    BASE_WIDGET_GET_CLASS(self)->action_configure(self, slot);
}

gboolean base_widget_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);

  if(!BASE_WIDGET_GET_CLASS(self)->action_exec)
    return FALSE;
  return BASE_WIDGET_GET_CLASS(self)->action_exec(self, slot, ev);
}

gchar *base_widget_get_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if(!priv->local_state)
    priv = base_widget_get_instance_private(
        BASE_WIDGET(base_widget_get_mirror_parent(self)));

  return priv->value->cache;
}

void base_widget_set_css ( GtkWidget *self, gchar *css )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!css || g_list_find_custom(priv->css, css, (GCompareFunc)g_strcmp0))
  {
    g_free(css);
    return;
  }

  css_widget_apply(base_widget_get_child(self), g_strdup(css));
  for(iter=priv->mirror_children; iter; iter=g_list_next(iter))
    css_widget_apply(base_widget_get_child(iter->data), g_strdup(css));

  priv->css = g_list_append(priv->css, css);
}

void base_widget_set_action ( GtkWidget *self, gint slot,
    GdkModifierType mods, action_t *action )
{
  BaseWidgetPrivate *priv;
  base_widget_attachment_t *attach;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(slot<0 || slot>BASE_WIDGET_MAX_ACTION)
    return;

  for(iter=priv->actions; iter; iter=g_list_next(iter))
  {
    attach = iter->data;
    if(attach->event == slot && attach->mods == mods)
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
    attach->event = slot;
    attach->mods = mods;
    priv->actions = g_list_prepend(priv->actions, attach);

  }
  attach->action = action;

  if(IS_FLOW_GRID(base_widget_get_child(self)))
    return;

  base_widget_action_configure(self, slot);
}

void base_widget_copy_actions ( GtkWidget *dest, GtkWidget *src )
{
  BaseWidgetPrivate *spriv;
  base_widget_attachment_t *attach;
  GList *iter;

  g_return_if_fail(IS_BASE_WIDGET(dest) && IS_BASE_WIDGET(src));
  spriv = base_widget_get_instance_private(BASE_WIDGET(src));

  for(iter=spriv->actions; iter; iter=g_list_next(iter))
  {
    attach = iter->data;
    base_widget_set_action(dest, attach->event, attach->mods,
        action_dup(attach->action));
  }
}

GtkWidget *base_widget_mirror ( GtkWidget *src )
{
  BaseWidgetPrivate *spriv, *dpriv;
  GList *iter;

  GtkWidget *dest;

  g_return_val_if_fail(IS_BASE_WIDGET(src),NULL);
  if(!BASE_WIDGET_GET_CLASS(src)->mirror)
    return NULL;

  dest = BASE_WIDGET_GET_CLASS(src)->mirror(src);

  spriv = base_widget_get_instance_private(BASE_WIDGET(src));
  dpriv = base_widget_get_instance_private(BASE_WIDGET(dest));

  dpriv->mirror_parent = src;
  if(!g_list_find(spriv->mirror_children, dest))
  {
    spriv->mirror_children = g_list_prepend(spriv->mirror_children, dest);
    base_widget_style(src);
    base_widget_update_value(dest);
  }

  dpriv->trigger = spriv->trigger;
  if(spriv->tooltip)
    base_widget_set_tooltip(dest, g_strdup(spriv->tooltip->definition));
  base_widget_set_local_state(dest, spriv->local_state);
  base_widget_set_state(dest, spriv->user_state, TRUE);
  base_widget_set_rect(dest, spriv->rect);
  for(iter=spriv->css; iter; iter=g_list_next(iter))
    css_widget_apply(base_widget_get_child(dest), g_strdup(iter->data));

  return dest;
}

GList *base_widget_get_mirror_children ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->mirror_children;
}

GtkWidget *base_widget_get_mirror_parent ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->mirror_parent)
    return priv->mirror_parent;
  else
    return self;
}

gboolean base_widget_emit_trigger ( const gchar *trigger )
{
  BaseWidgetPrivate *priv;
  GList *iter;

  if(!trigger)
    return FALSE;
  g_debug("trigger: %s", trigger);

  scanner_invalidate();
  g_mutex_lock(&widget_mutex);
  for(iter=widgets_scan; iter!=NULL; iter=g_list_next(iter))
  {
    priv = base_widget_get_instance_private(BASE_WIDGET(iter->data));
    if(!priv->trigger || trigger!=priv->trigger)
      continue;
    if(expr_cache_eval(priv->value) || priv->always_update)
      base_widget_update_value(iter->data);
    if(expr_cache_eval(priv->style))
      base_widget_style(iter->data);
  }
  g_mutex_unlock(&widget_mutex);
  action_exec(NULL, action_trigger_lookup(trigger), NULL, NULL, NULL);

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
    for(iter=widgets_scan; iter!=NULL; iter=g_list_next(iter))
    {
      priv = base_widget_get_instance_private(BASE_WIDGET(iter->data));
      if(base_widget_get_next_poll(iter->data)<=ctime)
      {
        if(expr_cache_eval(priv->value) || priv->always_update)
          g_main_context_invoke(gmc,(GSourceFunc)base_widget_update_value,
              iter->data);
        if(expr_cache_eval(priv->style))
          g_main_context_invoke(gmc, (GSourceFunc)base_widget_style,
              iter->data);
        base_widget_set_next_poll(iter->data,ctime);
      }
      timer = MIN(timer, base_widget_get_next_poll(iter->data));
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
    gtk_container_forall(GTK_CONTAINER(self), base_widget_autoexec, data);

  if(!IS_BASE_WIDGET(self))
    return;

  action = base_widget_get_action(self, 0, 0);
  if(action)
    action_exec(self, action, NULL, wintree_from_id(wintree_get_focus()),NULL);
}
