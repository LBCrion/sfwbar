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
  BASE_WIDGET_ID,
  BASE_WIDGET_LOCAL,
  BASE_WIDGET_USER_STATE,
  BASE_WIDGET_TOOLTIP,
  BASE_WIDGET_VALUE,
  BASE_WIDGET_STYLE,
  BASE_WIDGET_CSS,
  BASE_WIDGET_LOC,
  BASE_WIDGET_TRIGGER,
  BASE_WIDGET_INTERVAL,
  BASE_WIDGET_ACTION,
  BASE_WIDGET_DISABLE,
};

static GList *widgets_scan;
static GMutex widget_mutex;
static gint64 base_widget_default_id = 0;

static base_widget_attachment_t *base_widget_attachment_new ( GBytes *code, gint ev,
    GdkModifierType mods )
{
  base_widget_attachment_t *attach;

  attach = g_malloc0(sizeof(base_widget_attachment_t));
  attach->event = ev;
  attach->mods = mods;
  attach->action = g_bytes_ref(code);

  return attach;
}

static void base_widget_attachment_free ( base_widget_attachment_t *attach )
{
  if(!attach)
    return;

  g_bytes_unref(attach->action);
  g_free(attach);
}

GPtrArray *base_widget_attachment_new_array ( GBytes *code, gint ev,
    GdkModifierType mods )
{
  GPtrArray *array;

  array = g_ptr_array_new_with_free_func(
      (GDestroyNotify)base_widget_attachment_free);
  g_ptr_array_add(array, base_widget_attachment_new(code, ev, mods));

  return array;
}

static gboolean base_widget_update_value ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(BASE_WIDGET_GET_CLASS(self)->update_value)
    BASE_WIDGET_GET_CLASS(self)->update_value(self);

  if(!priv->local_state)
    g_list_foreach(base_widget_get_mirror_children(self),
        (GFunc)base_widget_update_value, NULL);

  return FALSE;
}

static gboolean base_widget_style ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));
  gtk_widget_set_name(base_widget_get_child(self), priv->style->cache);
  css_widget_cascade(self, NULL);

  if(!priv->local_state)
    g_list_foreach(base_widget_get_mirror_children(self),
        (GFunc)base_widget_style, NULL);

  return FALSE;
}

gboolean base_widget_update_expressions ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(expr_cache_eval(priv->value) || BASE_WIDGET_GET_CLASS(self)->always_update)
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_update_value, self);
  priv->value->eval = TRUE;
  if(expr_cache_eval(priv->style))
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_style, self);
  if(priv->local_state)
    g_list_foreach(priv->mirror_children,
        (GFunc)base_widget_update_expressions, NULL);
  return FALSE;
}

static void base_widget_destroy ( GtkWidget *self )
{
  BaseWidgetPrivate *priv, *ppriv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  trigger_remove((gchar *)(priv->trigger),
      (trigger_func_t)base_widget_update_expressions, self);
  priv->trigger = NULL;
  g_mutex_lock(&widget_mutex);
  widgets_scan = g_list_remove(widgets_scan, self);
  g_mutex_unlock(&widget_mutex);
  g_source_remove_by_user_data(self);

  if(priv->mirror_parent)
  {
    ppriv = base_widget_get_instance_private(BASE_WIDGET(priv->mirror_parent));
    ppriv->mirror_children = g_list_remove(ppriv->mirror_children, self);
    priv->mirror_parent = NULL;
  }

  if(priv->store && priv->store->widget_map && priv->id)
    g_hash_table_remove(priv->store->widget_map, priv->id);

  g_clear_pointer(&priv->css, g_free);
  priv->css = NULL;
  g_clear_pointer(&priv->id, g_free);
  g_clear_pointer(&priv->value, expr_cache_free);
  g_clear_pointer(&priv->style, expr_cache_free);
  g_clear_pointer(&priv->tooltip, expr_cache_free);
  g_clear_pointer(&priv->actions, g_ptr_array_unref);
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

gboolean base_widget_attachment_event ( gconstpointer p1, gconstpointer p2 )
{
  const base_widget_attachment_t *a1 = p1;

  return (a1->event == GPOINTER_TO_INT(p2));
}

gboolean base_widget_attachment_comp ( gconstpointer p1, gconstpointer p2 )
{
  const base_widget_attachment_t *a1 = p1, *a2 = p2;

  return (a1->mods == a2->mods) && (a1->event == a2->event);
}

gboolean base_widget_check_action_slot ( GtkWidget *self, gint event )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  return g_ptr_array_find_with_equal_func(priv->actions,
        GINT_TO_POINTER(event), base_widget_attachment_event, NULL);
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

  if(!base_widget_check_action_slot(base_widget_get_mirror_parent(self), slot))
    return FALSE;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if( (action = base_widget_get_action(base_widget_get_mirror_parent(self),
          slot, base_widget_get_modifiers(self))) )
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

static void base_widget_set_trigger ( GtkWidget *self, gchar *trigger )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  trigger_remove((gchar *)(priv->trigger),
      (trigger_func_t)base_widget_update_expressions, self);
  priv->trigger = NULL;

  if(priv->mirror_parent && !priv->local_state)
    return;

  if(trigger)
  {
    priv->interval = 0;
    priv->trigger = trigger_add(trigger,
        (trigger_func_t)base_widget_update_expressions, self);
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

  if(state)
    base_widget_set_trigger(self, (gchar *)ppriv->trigger);
  else
    base_widget_set_trigger(self, NULL);
}

static void base_widget_mirror_impl ( GtkWidget *dest, GtkWidget *src )
{
  BaseWidgetPrivate *spriv, *dpriv;

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
  g_object_bind_property(G_OBJECT(src), "action",
      G_OBJECT(dest), "action", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "css",
      G_OBJECT(dest), "css", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "loc",
      G_OBJECT(dest), "loc", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "store",
      G_OBJECT(dest), "store", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "trigger",
      G_OBJECT(dest), "trigger", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "has-tooltip",
      G_OBJECT(dest), "has-tooltip", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "local",
      G_OBJECT(dest), "local", G_BINDING_SYNC_CREATE);
}

static gboolean base_widget_query_tooltip ( GtkWidget *self, gint x, gint y,
    gboolean kbmode, GtkTooltip *tooltip )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), FALSE);
  if(BASE_WIDGET_GET_CLASS(self)->custom_tooltip)
    return FALSE;
  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  (void)expr_cache_eval(priv->tooltip);
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

  if(!BASE_WIDGET_GET_CLASS(self)->custom_tooltip)
  {
    gtk_widget_set_has_tooltip(self, !!code);
    gtk_widget_trigger_tooltip_query(self);
  }
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
    base_widget_update_value(self);

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
    base_widget_style(self);

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

static void base_widget_attachments_merge ( GtkWidget *self, GPtrArray *new )
{
  BaseWidgetPrivate *priv;
  base_widget_attachment_t *item;
  guint i, index;

  g_return_if_fail(IS_BASE_WIDGET(self));
  g_return_if_fail(new);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  for(i=0; i < new->len; i++)
  {
    item = g_ptr_array_index(new, i);
    if(g_ptr_array_find_with_equal_func(priv->actions,
          g_ptr_array_index(new, i), base_widget_attachment_comp, &index))
      g_ptr_array_remove_index_fast(priv->actions, index);
    g_ptr_array_add(priv->actions,
        base_widget_attachment_new(item->action, item->event, item->mods));
  }
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
    case BASE_WIDGET_ID:
      g_value_set_string(value, priv->id);
      break;
    case BASE_WIDGET_LOCAL:
      g_value_set_boolean(value, priv->local_state);
      break;
    case BASE_WIDGET_USER_STATE:
      g_value_set_uint(value, priv->user_state);
      break;
    case BASE_WIDGET_TOOLTIP:
      g_value_set_boxed(value, priv->tooltip->code);
      break;
    case BASE_WIDGET_VALUE:
      g_value_set_boxed(value, priv->value->code);
      break;
    case BASE_WIDGET_STYLE:
      g_value_set_boxed(value, priv->style->code);
      break;
    case BASE_WIDGET_ACTION:
      g_value_set_boxed(value, priv->actions);
      break;
    case BASE_WIDGET_LOC:
      g_value_set_boxed(value, &priv->rect);
      break;
    case BASE_WIDGET_TRIGGER:
      g_value_set_string(value, priv->trigger);
      break;
    case BASE_WIDGET_INTERVAL:
      g_value_set_int64(value, priv->interval/1000);
      break;
    case BASE_WIDGET_CSS:
      g_value_set_string(value, priv->css);
      break;
    case BASE_WIDGET_DISABLE:
      g_value_set_boolean(value, priv->disabled);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void base_widget_set_store ( GtkWidget *self, vm_store_t *store )
{
  BaseWidgetPrivate *priv;
  GtkWidget *old;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->store == store)
    return;

  if(priv->store && priv->id)
    g_hash_table_remove(priv->store->widget_map, priv->id);

  priv->store = store;
  if(priv->store)
  {
    if( !(old = g_hash_table_lookup(priv->store->widget_map, priv->id)) )
      g_hash_table_insert(priv->store->widget_map, priv->id, self);
    else if (old != self)
      g_message("widget id collision: '%s'", priv->id);
  }
}

static void base_widget_set_id ( GtkWidget *self, const gchar *id )
{
  BaseWidgetPrivate *priv;
  GtkWidget *old;

  g_return_if_fail(IS_BASE_WIDGET(self));
  priv = base_widget_get_instance_private(BASE_WIDGET(self));

  if(priv->store && priv->store->widget_map && priv->id)
    g_hash_table_remove(priv->store->widget_map, priv->id);

  g_free(priv->id);
  priv->id = id? g_strdup(id): g_strdup_printf("_w%lld",
      (long long int)base_widget_default_id++);

  if(priv->store)
  {
    if( !(old = g_hash_table_lookup(priv->store->widget_map, priv->id)) )
      g_hash_table_insert(priv->store->widget_map, g_strdup(priv->id), self);
    else if (old != self)
      g_message("widget id collision: '%s'", priv->id);
  }
}

static void base_widget_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  BaseWidgetPrivate *priv;

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  switch(id)
  {
    case BASE_WIDGET_STORE:
      base_widget_set_store(GTK_WIDGET(self), g_value_get_pointer(value));
      break;
    case BASE_WIDGET_ID:
      base_widget_set_id(GTK_WIDGET(self), g_value_get_string(value));
      break;
    case BASE_WIDGET_LOCAL:
      base_widget_set_local_state(GTK_WIDGET(self), g_value_get_boolean(value));
      break;
    case BASE_WIDGET_USER_STATE:
      priv->user_state = g_value_get_uint(value);
      break;
    case BASE_WIDGET_TOOLTIP:
      base_widget_set_tooltip(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    case BASE_WIDGET_VALUE:
      base_widget_set_value(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    case BASE_WIDGET_STYLE:
      base_widget_set_style(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    case BASE_WIDGET_ACTION:
      base_widget_attachments_merge(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    case BASE_WIDGET_LOC:
      base_widget_set_rect(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    case BASE_WIDGET_TRIGGER:
      base_widget_set_trigger(GTK_WIDGET(self), (gchar *)g_value_get_string(value));
      break;
    case BASE_WIDGET_INTERVAL:
      priv->interval = g_value_get_int64(value)*1000;
      break;
    case BASE_WIDGET_CSS:
      if(g_strcmp0(priv->css, g_value_get_string(value)))
      {
        g_clear_pointer(&priv->css, g_free);
        priv->css = g_strdup(g_value_get_string(value));
        if(priv->provider)
          gtk_style_context_remove_provider(gtk_widget_get_style_context(
                GTK_WIDGET(self)), GTK_STYLE_PROVIDER(priv->provider));
        priv->provider = css_widget_apply(
            base_widget_get_child(GTK_WIDGET(self)), priv->css);
      }
      break;
    case BASE_WIDGET_DISABLE:
      priv->disabled = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void base_widget_class_init ( BaseWidgetClass *kclass )
{
  kclass->action_exec = base_widget_action_exec_impl;
  kclass->mirror = base_widget_mirror_impl;

  G_OBJECT_CLASS(kclass)->get_property = base_widget_get_property;
  G_OBJECT_CLASS(kclass)->set_property = base_widget_set_property;

  GTK_WIDGET_CLASS(kclass)->destroy = base_widget_destroy;
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
      g_param_spec_pointer("store", "store", "no_config", G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_ID,
      g_param_spec_string("id", "id", "no_config", NULL, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_TOOLTIP,
      g_param_spec_boxed("tooltip", "tooltip", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_VALUE,
      g_param_spec_boxed("value", "value", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_STYLE,
      g_param_spec_boxed("style", "style", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_ACTION,
      g_param_spec_boxed("action", "action", "sfwbar_config:b",
        G_TYPE_PTR_ARRAY, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_CSS,
      g_param_spec_string("css", "css", "sfwbar_config", NULL,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_LOC,
      g_param_spec_boxed("loc", "loc", "sfwbar_config", GDK_TYPE_RECTANGLE,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_TRIGGER,
      g_param_spec_string("trigger", "trigger", "sfwbar_config", NULL,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_LOCAL,
      g_param_spec_boolean("local", "local", "sfwbar_config", FALSE,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_DISABLE,
      g_param_spec_boolean("disable", "disable", "sfwbar_config", FALSE,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), BASE_WIDGET_INTERVAL,
      g_param_spec_int64("interval", "interval", "sfwbar_config",
        0, INT64_MAX, 0, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      BASE_WIDGET_USER_STATE, g_param_spec_uint("user-state",
        "user-state", "no_config", 0, UINT_MAX, 0, G_PARAM_READWRITE));
}

static void base_widget_init ( BaseWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_if_fail(IS_BASE_WIDGET(self));

  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  priv->value = expr_cache_new();
  priv->style = expr_cache_new();
  priv->tooltip = expr_cache_new();
  priv->actions = g_ptr_array_new_with_free_func(
      (GDestroyNotify)base_widget_attachment_free);
  priv->interval = 1000000;
  priv->dir = GTK_POS_RIGHT;
  priv->rect.x = -1;
  priv->rect.y = -1;
  priv->rect.width = 1;
  priv->rect.height = 1;
  base_widget_set_id(GTK_WIDGET(self), NULL);

  gtk_widget_add_events(GTK_WIDGET(self),
      GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
  if(!gtk_drag_dest_get_target_list(GTK_WIDGET(self)))
    gtk_drag_dest_set(GTK_WIDGET(self), GTK_DEST_DEFAULT_ALL, NULL, 0,
        GDK_ACTION_MOVE);
  gtk_drag_dest_set_track_motion(GTK_WIDGET(self), TRUE);
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

  priv->next_poll = ctime + priv->interval;
}

gint64 base_widget_get_next_poll ( GtkWidget *self )
{
  BaseWidgetPrivate *priv;

  g_return_val_if_fail(IS_BASE_WIDGET(self), G_MAXINT64);
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
  base_widget_attachment_t target, *match;
  guint index;

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(
      BASE_WIDGET(base_widget_get_mirror_parent(self)));

  target.event = n;
  target.mods = mods;
  if(g_ptr_array_find_with_equal_func(priv->actions, &target, 
        base_widget_attachment_comp, &index))
  {
    if( (match = g_ptr_array_index(priv->actions, index)) )
    return match->action;
  }

  return NULL;
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

  g_return_val_if_fail(IS_BASE_WIDGET(self), NULL);
  priv = base_widget_get_instance_private(BASE_WIDGET(self));
  if(!priv->local_state)
    priv = base_widget_get_instance_private(
        BASE_WIDGET(base_widget_get_mirror_parent(self)));

  return priv->value->cache;
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
    priv = base_widget_get_instance_private(BASE_WIDGET(self));
    if(!priv->local_state && priv->mirror_parent)
      priv = base_widget_get_instance_private(
          BASE_WIDGET(base_widget_get_mirror_parent(self)));
    state |= priv->user_state;
  }

  return state;
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
    {
      if(base_widget_get_next_poll(iter->data) <= ctime)
      {
        g_mutex_unlock(&widget_mutex);
        base_widget_update_expressions(iter->data);
        base_widget_set_next_poll(iter->data, ctime);
        g_mutex_lock(&widget_mutex);
      }
      timer = MIN(timer, base_widget_get_next_poll(iter->data));
    }
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
  {
    g_bytes_ref(action);
    vm_run_action(action, self, NULL, wintree_from_id(wintree_get_focus()),
        NULL, priv->store);
  }
}
