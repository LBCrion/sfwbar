/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "css.h"
#include "bar.h"
#include "pager.h"
#include "pageritem.h"
#include "util/string.h"
#include "vm/vm.h"

G_DEFINE_TYPE_WITH_CODE (PagerItem, pager_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (PagerItem));

static gboolean pager_item_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  PagerItemPrivate *priv;
  GdkModifierType mods;
  GBytes *action;

  g_return_val_if_fail(IS_PAGER_ITEM(self),FALSE);
  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  if(!base_widget_check_action_slot(priv->pager, slot) && slot != 1)
    return FALSE;

  mods = base_widget_get_modifiers(self);
  action = base_widget_get_action(priv->pager, slot, mods);

  if(!action && !mods && slot==1)
  {
    workspace_activate(priv->ws);
    return TRUE;
  }

  if(!action)
    return FALSE;
  else
    vm_run_action(action, self, ev, wintree_from_id(wintree_get_focus()),
        NULL, base_widget_get_store(priv->pager), NULL);

  return TRUE;
}

void pager_item_update ( GtkWidget *self )
{
  PagerItemPrivate *priv;
  gboolean show_pin, preview;

  g_return_if_fail(IS_PAGER_ITEM(self));
  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  if(!priv->invalid)
    return;

  if(g_strcmp0(gtk_label_get_label(GTK_LABEL(priv->label)), priv->ws->name))
    gtk_label_set_markup(GTK_LABEL(priv->label), priv->ws->name);

  g_object_get(G_OBJECT(priv->pager), "preview", &preview, NULL);
  gtk_widget_set_has_tooltip(priv->button, preview);

  css_set_class(priv->button, "focused", priv->ws->state & WS_STATE_FOCUSED);
  css_set_class(priv->button, "visible", priv->ws->state & WS_STATE_VISIBLE);
  css_set_class(priv->button, "urgent", priv->ws->state & WS_STATE_URGENT);

  gtk_widget_unset_state_flags(base_widget_get_child(self),
      GTK_STATE_FLAG_PRELIGHT);

  show_pin = priv->ws->id != PAGER_PIN_ID || (workspace_get_can_create() &&
        pager_check_pins(priv->pager, priv->ws->name));
  flow_item_set_active(self, workspace_check_monitor(priv->ws->id,
      bar_get_output(priv->pager)) && show_pin);

  priv->invalid = FALSE;
}

workspace_t *pager_item_get_workspace ( GtkWidget *self )
{
  PagerItemPrivate *priv;

  g_return_val_if_fail(IS_PAGER_ITEM(self),NULL);
  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  return priv->ws;
}

static void pager_item_invalidate ( GtkWidget *self )
{
  PagerItemPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_PAGER_ITEM(self));
  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  flow_grid_invalidate(priv->pager);
  priv->invalid = TRUE;
}

static gint pager_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent)
{
  PagerItemPrivate *p1, *p2;
  gchar *e1 = 0, *e2 = 0;
  gint n1, n2;

  g_return_val_if_fail(IS_PAGER_ITEM(a),0);
  g_return_val_if_fail(IS_PAGER_ITEM(b),0);

  p1 = pager_item_get_instance_private(PAGER_ITEM(a));
  p2 = pager_item_get_instance_private(PAGER_ITEM(b));

  n1 = str_ascii_toll(p1->ws->name, &e1, 10);
  n2 = str_ascii_toll(p2->ws->name, &e2, 10);
  if((e1 && *e1) || (e2 && *e2))
    return g_strcmp0(p1->ws->name, p2->ws->name);
  else
    return n1-n2;
}

static void pager_item_class_init ( PagerItemClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->action_exec = pager_item_action_exec;
  FLOW_ITEM_CLASS(kclass)->update = pager_item_update;
  FLOW_ITEM_CLASS(kclass)->get_source =
    (void * (*)(GtkWidget *))pager_item_get_workspace;
  FLOW_ITEM_CLASS(kclass)->compare = pager_item_compare;
  FLOW_ITEM_CLASS(kclass)->invalidate = pager_item_invalidate;
}

static void pager_item_init ( PagerItem *self )
{
}

static gboolean pager_item_draw_preview ( GtkWidget *widget, cairo_t *cr,
    workspace_t *ws )
{
  GtkStyleContext *style;
  GdkRGBA fg;
  guint w,h,i,n;
  gint focus;
  GdkRectangle *wins, spc;

  w = gtk_widget_get_allocated_width (widget);
  h = gtk_widget_get_allocated_height (widget);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_color (style,GTK_STATE_FLAG_NORMAL, &fg);
  cairo_set_line_width(cr,1);

  if( !(n = workspace_get_geometry(NULL, NULL, ws->id, &wins, &spc, &focus)) )
    return TRUE;

  for(i=0;i<n;i++)
  {
    cairo_set_source_rgba(cr, fg.red, fg.blue, fg.green, (i==focus)?1:0.5);
    cairo_rectangle(cr,
        (int)(wins[i].x*w/spc.width),
        (int)(wins[i].y*h/spc.height),
        (int)(wins[i].width*w/spc.width),
        (int)(wins[i].height*h/spc.height));
    cairo_fill(cr);
    gtk_render_frame(style, cr,
        (int)(wins[i].x*w/spc.width),
        (int)(wins[i].y*h/spc.height),
        (int)(wins[i].width*w/spc.width),
        (int)(wins[i].height*h/spc.height));
    cairo_stroke(cr);
  }
  g_free(wins);

  return TRUE;
}

static gboolean pager_item_draw_tooltip ( GtkWidget *widget, gint x, gint y,
    gboolean kbmode, GtkTooltip *tooltip, workspace_t *ws )
{
  GtkWidget *button;

  button = gtk_button_new();
  g_signal_connect(button,"draw",G_CALLBACK(pager_item_draw_preview),ws);
  gtk_widget_set_name(button, "pager_preview");
  gtk_tooltip_set_custom(tooltip,button);
  return TRUE;
}

void pager_item_new( workspace_t *ws, GtkWidget *pager )
{
  GtkWidget *self;
  PagerItemPrivate *priv;

  g_return_if_fail(IS_PAGER(pager));
  if(flow_grid_find_child(pager, ws))
    return;

  self = GTK_WIDGET(g_object_new(pager_item_get_type(), NULL));
  priv = pager_item_get_instance_private(PAGER_ITEM(self));
  priv->ws = ws;
  priv->pager = pager;

  priv->button = gtk_button_new();
  priv->label = gtk_label_new("");
  gtk_label_set_markup(GTK_LABEL(priv->label), ws->name);
  gtk_container_add(GTK_CONTAINER(priv->button), priv->label);
  gtk_container_add(GTK_CONTAINER(self), priv->button);
  gtk_widget_set_name(priv->button, "pager_item");
  g_signal_connect(priv->button,"query-tooltip",
      G_CALLBACK(pager_item_draw_tooltip),ws);
  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(pager, self);
  pager_item_invalidate(self);
}
