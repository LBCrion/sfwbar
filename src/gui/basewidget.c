/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "trigger.h"
#include "module.h"
#include "meson.h"
#include "scanner.h"
#include "gui/basewidget.h"
#include "gui/css.h"
#include "gui/grid.h"
#include "gui/flowgrid.h"
#include "gui/bar.h"
#include "util/string.h"

G_DEFINE_TYPE_WITH_CODE (BaseWidget, base_widget, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE (BaseWidget))

enum {
  BASE_WIDGET_STORE = 1,
  BASE_WIDGET_LOCAL_STATE,
  BASE_WIDGET_USER_STATE,
  BASE_WIDGET_TOOLTIP,
  BASE_WIDGET_VALUE,
  BASE_WIDGET_STYLE,
  BASE_WIDGET_RECTANGLE,
  BASE_WIDGET_TRIGGER,
  BASE_WIDGET_INTERVAL,
};

static GList *widgets_scan;
static GMutex widget_mutex;
static gint64 base_widget_default_id = 0;

static guint value_update_signal;
static guint style_update_signal;
static guint new_css_signal;

static void base_widget_attachment_free ( base_widget_attachment_t *attach )
{
  if(!attach)
    return;

  g_bytes_unref(attach->action);
  g_free(attach);
}

static void base_widget_trigger_cb ( GtkWidget *self, vm_store_t *store )
{
  BaseWidgetPrivate *priv;
  static gint count;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(expr_cache_eval(priv->value) || BASE_WIDGET_GET_CLASS(self)->always_update)
    g_signal_emit(self, value_update_signal, 0);
  if(expr_cache_eval(priv->style))
    g_signal_emit(self, style_update_signal, 0);
  count++;
}

static void base_widget_destroy ( GtkWidget *self )
{
  BaseWidgetPrivate *priv, *ppriv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  trigger_remove((gchar *)(priv->trigger),
      (trigger_func_t)base_widget_trigger_cb, self);
  priv->trigger = NULL;
  g_mutex_lock(&widget_mutex);
  widgets_scan = g_list_remove(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);

  if(priv->mirror_parent)
  {
    ppriv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));
    ppriv->mirror_children = g_list_remove(ppriv->mirror_children, self);
    priv->mirror_parent = NULL;
  }

  if(priv->store && priv->store->widget_map && priv->id)
    g_hash_table_remove(priv->store->widget_map, priv->id);

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
  GTK_WIDGET_CLASS(base_widget_parent_class)->size_allocate(self, alloc);
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

  state = gdk_keymap_get_modifier_state(gdk_keymap_get_for_display(
        gdk_display_get_default())) & gtk_accelerator_get_default_mod_mask();

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
  BaseWidgetPrivate *priv;
  GBytes *action;

  if(!base_widget_check_action_slot(self, slot))
    return FALSE;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if( (action = base_widget_get_action(self, slot,
        base_widget_get_modifiers(self))) )
    vm_run_action(action, self, (GdkEvent *)ev,
        wintree_from_id(wintree_get_focus()), NULL, priv->store);
  return !!action;
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

static void base_widget_apply_css ( GtkWidget *host, gchar *css, GtkWidget *self )
{
  css_widget_apply(base_widget_get_child(self), css);
}

static void base_widget_set_signals ( GtkWidget *self, GtkWidget *src )
{
  g_signal_handlers_disconnect_by_func(G_OBJECT(self),
      G_CALLBACK(base_widget_update_value), self);
  g_signal_handlers_disconnect_by_func(G_OBJECT(self),
      G_CALLBACK(base_widget_style), self);

  g_signal_connect_swapped(G_OBJECT(src), "value-update",
      G_CALLBACK(base_widget_update_value), self);
  g_signal_connect_swapped(G_OBJECT(src), "style-update",
      G_CALLBACK(base_widget_style), self);
}

static void base_widget_set_trigger ( GtkWidget *self, gchar *trigger )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  trigger_remove((gchar *)(priv->trigger),
      (trigger_func_t)base_widget_trigger_cb, self);
  priv->trigger = NULL;

  if(priv->mirror_parent && !priv->local_state)
    return;

  if(trigger)
  {
    priv->interval = 0;
    priv->trigger = trigger_add(trigger,
        (trigger_func_t)base_widget_trigger_cb, self);
  }
}

static void base_widget_set_local_state ( GtkWidget *self, gboolean state )
{
  GtkWidget *parent;
  BaseWidgetPrivate *priv, *ppriv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  priv->local_state = state;
  parent = base_widget_get_mirror_parent(self);
  ppriv = base_widget_get_instance_private(BASE_WIDGET(parent));

  g_mutex_lock(&widget_mutex);
  if(state && !g_list_find(widgets_scan, self))
      widgets_scan = g_list_append(widgets_scan, self);
  else if(!state)
    widgets_scan = g_list_remove(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);

  if(state)
  {
    base_widget_set_signals(self, self);
    base_widget_set_trigger(self, (gchar *)ppriv->trigger);
  }
  else
  {
    base_widget_set_signals(self, parent);
    base_widget_set_trigger(self, NULL);
  }
}

static void base_widget_mirror_impl ( GtkWidget *dest, GtkWidget *src )
{
  BaseWidgetPrivate *spriv, *dpriv;
  GList *iter;

  spriv = base_widget_get_instance_private(BASE_WIDGET(src));
  dpriv = base_widget_get_instance_private(BASE_WIDGET(dest));

  dpriv->mirror_parent = src;
  if(!g_list_find(spriv->mirror_children, dest))
  {
    spriv->mirror_children = g_list_prepend(spriv->mirror_children, dest);
    base_widget_style(dest);
    base_widget_update_value(dest);
  }

  g_object_bind_property(G_OBJECT(src), "user-state",
      G_OBJECT(dest), "user-state", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "tooltip",
      G_OBJECT(dest), "tooltip", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "value",
      G_OBJECT(dest), "value", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "style",
      G_OBJECT(dest), "style", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "rectangle",
      G_OBJECT(dest), "rectangle", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "store",
      G_OBJECT(dest), "store", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "trigger",
      G_OBJECT(dest), "trigger", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "has-tooltip",
      G_OBJECT(dest), "has-tooltip", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "local-state",
      G_OBJECT(dest), "local-state", G_BINDING_SYNC_CREATE);
  for(iter=spriv->css; iter; iter=g_list_next(iter))
    css_widget_apply(base_widget_get_child(dest), iter->data);
}

static gboolean base_widget_query_tooltip ( GtkWidget *self, gint x, gint y,
    gboolean kbmode, GtkTooltip *tooltip )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  expr_cache_eval(priv->tooltip);
  gtk_tooltip_set_markup(tooltip, priv->tooltip->cache);

  return priv->tooltip->cache && *(priv->tooltip->cache);
}

static void base_widget_set_tooltip ( GtkWidget *self, GBytes *code )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_bytes_unref(priv->tooltip->code);
  priv->tooltip->code = code? g_bytes_ref(code) : NULL;
  priv->tooltip->widget = self;
  priv->tooltip->eval = !!code;

  gtk_widget_set_has_tooltip(self, !!code);
}

static void base_widget_set_value ( GtkWidget *self, GBytes *code )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->value->code == code)
    return;
  g_bytes_unref(priv->value->code);
  priv->value->code = code? g_bytes_ref(code) : NULL;
  priv->value->widget = self;
  priv->value->eval = !!code;

  if(expr_cache_eval(priv->value) || BASE_WIDGET_GET_CLASS(self)->always_update)
    g_signal_emit(self, value_update_signal, 0);

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan, self))
    widgets_scan = g_list_append(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);
}

static void base_widget_set_style ( GtkWidget *self, GBytes *code )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  self = base_widget_get_mirror_parent(self);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->style->code == code)
    return;
  g_bytes_unref(priv->style->code);
  priv->style->code = code? g_bytes_ref(code) : NULL;
  priv->style->widget = self;
  priv->style->eval = !!code;

  if((priv->mirror_parent && !priv->local_state) ||
      expr_cache_eval(priv->style))
    g_signal_emit(self, style_update_signal, 0);

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan, self))
    widgets_scan = g_list_append(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);
}

static void base_widget_set_rect ( GtkWidget *self, GdkRectangle *rect )
{
  BaseWidgetPrivate *priv;
  GtkWidget *parent;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!memcmp(&priv->rect, rect, sizeof(GdkRectangle)))
    return;

  priv->rect = *rect;

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

static void base_widget_get_property ( GObject *self, guint id, GValue *value,
    GParamSpec *spec )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  switch(id)
  {
    case BASE_WIDGET_STORE:
      g_value_set_pointer(value, priv->store);
      break;
    case BASE_WIDGET_LOCAL_STATE:
      g_value_set_boolean(value, priv->local_state);
      break;
    case BASE_WIDGET_USER_STATE:
      g_value_set_uint(value, priv->user_state);
      break;
    case BASE_WIDGET_TOOLTIP:
      g_value_set_pointer(value, priv->tooltip->code);
      break;
    case BASE_WIDGET_VALUE:
      g_value_set_pointer(value, priv->value->code);
      break;
    case BASE_WIDGET_STYLE:
      g_value_set_pointer(value, priv->style->code);
      break;
    case BASE_WIDGET_RECTANGLE:
      g_value_set_pointer(value, &priv->rect);
      break;
    case BASE_WIDGET_TRIGGER:
      g_value_set_string(value, priv->trigger);
      break;
    case BASE_WIDGET_INTERVAL:
      g_value_set_int64(value, priv->interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void base_widget_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  BaseWidgetPrivate *priv;
  GObject *old;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  switch(id)
  {
    case BASE_WIDGET_STORE:
      if(priv->store == g_value_get_pointer(value))
        return;

      if(priv->store && priv->id)
        g_hash_table_remove(priv->store->widget_map, priv->id);

      priv->store = g_value_get_pointer(value);
      if(priv->store)
      {
        if( !(old = g_hash_table_lookup(priv->store->widget_map, priv->id)) )
          g_hash_table_insert(priv->store->widget_map, priv->id, self);
        else if (old != self)
          g_message("widget id collision: '%s'", priv->id);
      }
      break;
    case BASE_WIDGET_LOCAL_STATE:
      base_widget_set_local_state(GTK_WIDGET(self), g_value_get_boolean(value));
      break;
    case BASE_WIDGET_USER_STATE:
      priv->user_state = g_value_get_uint(value);
      break;
    case BASE_WIDGET_TOOLTIP:
      base_widget_set_tooltip(GTK_WIDGET(self), g_value_get_pointer(value));
      break;
    case BASE_WIDGET_VALUE:
      base_widget_set_value(GTK_WIDGET(self), g_value_get_pointer(value));
      break;
    case BASE_WIDGET_STYLE:
      base_widget_set_style(GTK_WIDGET(self), g_value_get_pointer(value));
      break;
    case BASE_WIDGET_RECTANGLE:
      base_widget_set_rect(GTK_WIDGET(self), g_value_get_pointer(value));
      break;
    case BASE_WIDGET_TRIGGER:
      base_widget_set_trigger(GTK_WIDGET(self), (gchar *)g_value_get_string(value));
      break;
    case BASE_WIDGET_INTERVAL:
      priv->interval = g_value_get_int64(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void base_widget_class_init ( BaseWidgetClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = base_widget_destroy;
  kclass->action_exec = base_widget_action_exec_impl;
  kclass->action_configure = base_widget_action_configure_impl;
  kclass->mirror = base_widget_mirror_impl;
  G_OBJECT_CLASS(kclass)->get_property = base_widget_get_property;
  G_OBJECT_CLASS(kclass)->set_property = base_widget_set_property;
  GTK_WIDGET_CLASS(kclass)->size_allocate = base_widget_size_allocate;
  GTK_WIDGET_CLASS(kclass)->get_preferred_width = base_widget_get_pref_width;
  GTK_WIDGET_CLASS(kclass)->get_preferred_height = base_widget_get_pref_height;
  GTK_WIDGET_CLASS(kclass)->button_release_event =
    base_widget_button_release_event;
  GTK_WIDGET_CLASS(kclass)->scroll_event = base_widget_scroll_event;
  GTK_WIDGET_CLASS(kclass)->drag_motion = base_widget_drag_motion;
  GTK_WIDGET_CLASS(kclass)->drag_leave = base_widget_drag_leave;
  GTK_WIDGET_CLASS(kclass)->query_tooltip = base_widget_query_tooltip;
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_STORE,
      g_param_spec_pointer("store", "store", "store", G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_TOOLTIP,
      g_param_spec_pointer("tooltip", "tooltip", "tooltip", G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_VALUE,
      g_param_spec_pointer("value", "value", "value", G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_STYLE,
      g_param_spec_pointer("style", "style", "style", G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_RECTANGLE,
      g_param_spec_pointer("rectangle", "rectangle", "rectangle",
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_TRIGGER,
      g_param_spec_string("trigger", "trigger", "trigger", NULL,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      BASE_WIDGET_LOCAL_STATE, g_param_spec_boolean("local-state",
        "local-state", "local-state", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      BASE_WIDGET_USER_STATE, g_param_spec_uint("user-state",
        "user-state", "user-state", 0, UINT_MAX, 0, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_INTERVAL,
      g_param_spec_int64("interval", "interval", "interval", 0, INT64_MAX, 0,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      BASE_WIDGET_RECTANGLE, g_param_spec_boxed("rect", "rect", "rect",
        GDK_TYPE_RECTANGLE,G_PARAM_READABLE));
  value_update_signal = g_signal_new("value-update", G_TYPE_FROM_CLASS(kclass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);
  style_update_signal = g_signal_new("style-update", G_TYPE_FROM_CLASS(kclass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);
  new_css_signal = g_signal_new("css-new", G_TYPE_FROM_CLASS(kclass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
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

  g_signal_connect(G_OBJECT(self), "notify::tooltip",
      G_CALLBACK(gtk_widget_trigger_tooltip_query), NULL);
  g_signal_connect(G_OBJECT(self), "css-new",
      G_CALLBACK(base_widget_apply_css), self);
  base_widget_set_signals(GTK_WIDGET(self), GTK_WIDGET(self));
}

GtkWidget *base_widget_get_child ( GtkWidget *self )
{
  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  return gtk_bin_get_child(GTK_BIN(self));
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
  if(BASE_WIDGET_GET_CLASS(self)->update_value)
    BASE_WIDGET_GET_CLASS(self)->update_value(self);

  return FALSE;
}

gboolean base_widget_style ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));
  gtk_widget_set_name(base_widget_get_child(self), priv->style->cache);
  css_widget_cascade(self, NULL);

  return FALSE;
}

void base_widget_set_style_static ( GtkWidget *self, gchar *style )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  self = base_widget_get_mirror_parent(self);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_bytes_unref(priv->style->code);
  g_free(priv->style->cache);
  priv->style->code = NULL;
  priv->style->cache = g_strdup(style);
  priv->style->eval = FALSE;

  base_widget_style(self);
  g_signal_emit(self, style_update_signal, 0);

  g_mutex_lock(&widget_mutex);
  if(!g_list_find(widgets_scan, self))
    widgets_scan = g_list_append(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);
}

void base_widget_set_id ( GtkWidget *self, gchar *id )
{
  BaseWidgetPrivate *priv;
  GtkWidget *old;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->store && priv->store->widget_map && priv->id)
    g_hash_table_remove(priv->store->widget_map, priv->id);

  g_free(priv->id);
  priv->id = id? id: g_strdup_printf("_w%lld",
      (long long int)base_widget_default_id++);

  if(priv->store)
  {
    if( !(old = g_hash_table_lookup(priv->store->widget_map, priv->id)) )
      g_hash_table_insert(priv->store->widget_map, g_strdup(priv->id), self);
    else if (old != self)
      g_message("widget id collision: '%s'", priv->id);
  }
}

GtkWidget *base_widget_from_id ( vm_store_t *store, gchar *id )
{
  GtkWidget *widget;

  if(!store || !id)
    return NULL;

  while(store && !(widget = g_hash_table_lookup(store->widget_map ,id)))
    store = store->parent;

  return widget;
}

void base_widget_set_state ( GtkWidget *self, guint16 mask, gboolean state )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  g_object_set(G_OBJECT(self), "user-state",
      state? priv->user_state | mask : priv->user_state & ~mask, NULL);
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

vm_store_t * base_widget_get_store ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->store;
}

gchar *base_widget_get_id ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self),NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  return priv->id;
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

GBytes *base_widget_get_action ( GtkWidget *self, gint n,
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
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!BASE_WIDGET_GET_CLASS(self)->action_exec)
    return FALSE;
  return BASE_WIDGET_GET_CLASS(self)->action_exec(
      priv->local_state? self: base_widget_get_mirror_parent(self), slot, ev);
}

gchar *base_widget_get_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if(!priv->local_state)
    priv = base_widget_get_instance_private(
        BASE_WIDGET(base_widget_get_mirror_parent(self)));

  return priv->value->cache;
}

void base_widget_set_css ( GtkWidget *self, gchar *css )
{
  BaseWidgetPrivate *priv;
  gchar *copy;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(!css || g_list_find_custom(priv->css, css, (GCompareFunc)g_strcmp0))
    return;

  copy = g_strdup(css);
  css_widget_apply(base_widget_get_child(self), copy);
  priv->css = g_list_append(priv->css, copy);

  g_signal_emit(self, new_css_signal, 0, copy);
}

guint16 base_widget_state_build ( GtkWidget *self, window_t *win )
{
  BaseWidgetPrivate *priv;
  guint16 state = 0;

  if(win)
  {
    state = win->state;
    if(wintree_is_focused(win->uid))
      state |= WS_FOCUSED;
  }

  if(self && IS_BASE_WIDGET(self))
  {
    priv = base_widget_get_instance_private(
        BASE_WIDGET(base_widget_get_mirror_parent(self)));
    state |= priv->user_state;
  }

  return state;
}

void base_widget_set_action ( GtkWidget *self, gint slot,
    GdkModifierType mods, GBytes *action )
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
    g_bytes_unref(attach->action);
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

GtkWidget *base_widget_mirror ( GtkWidget *src )
{
  GtkWidget *dest;

  g_return_val_if_fail(IS_BASE_WIDGET(src), NULL);
  dest = GTK_WIDGET(g_object_new(G_TYPE_FROM_INSTANCE(src), NULL));
  BASE_WIDGET_GET_CLASS(src)->mirror(dest, src);

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

gint64 base_widget_update ( GtkWidget *self, gint64 *ctime )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if(!ctime || base_widget_get_next_poll(self) <= *ctime)
  {
    if(expr_cache_eval(priv->value) ||
        BASE_WIDGET_GET_CLASS(self)->always_update)
      g_signal_emit(self, value_update_signal, 0);
    if(expr_cache_eval(priv->style))
      g_signal_emit(self, style_update_signal, 0);
    if(ctime)
      base_widget_set_next_poll(self, *ctime);
  }
  return base_widget_get_next_poll(self);
}

gpointer base_widget_scanner_thread ( GMainContext *gmc )
{
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
      timer = MIN(timer, base_widget_update(iter->data, &ctime));
    g_mutex_unlock(&widget_mutex);

    timer -= g_get_monotonic_time();

    if(timer > 0)
      g_usleep(timer);
  }
}

void base_widget_autoexec ( GtkWidget *self, gpointer data )
{
  BaseWidgetPrivate *priv;
  GBytes *action;

  if(GTK_IS_CONTAINER(self))
    gtk_container_forall(GTK_CONTAINER(self), base_widget_autoexec, data);

  if(!IS_BASE_WIDGET(self))
    return;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if( (action = base_widget_get_action(self, 0, 0)) )
    vm_run_action(action, self, NULL, wintree_from_id(wintree_get_focus()),
        NULL, priv->store);
}
