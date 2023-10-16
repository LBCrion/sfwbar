/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "bar.h"
#include "trayitem.h"
#include "flowgrid.h"
#include "scaleimage.h"

G_DEFINE_TYPE_WITH_CODE (TrayItem, tray_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (TrayItem));

void tray_item_update ( GtkWidget *self )
{
  TrayItemPrivate *priv;
  gint icon=-1, pix=-1;

  g_return_if_fail(IS_TRAY_ITEM(self));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  if(!priv->invalid)
    return;
  priv->invalid = FALSE;

  if(priv->sni->string[SNI_PROP_STATUS])
  {
    switch(priv->sni->string[SNI_PROP_STATUS][0])
    {
      case 'A':
        gtk_widget_set_name(priv->button,"tray_active");
        icon = SNI_PROP_ICON;
        pix = SNI_PROP_ICONPIX;
        break;
      case 'N':
        gtk_widget_set_name(priv->button,"tray_attention");
        icon = SNI_PROP_ATTN;
        pix = SNI_PROP_ATTNPIX;
        break;
      case 'P':
        gtk_widget_set_name(priv->button,"tray_passive");
        icon = SNI_PROP_ICON;
        pix = SNI_PROP_ICONPIX;
    }
  }

  if(icon==-1)
    scale_image_set_image(priv->icon, NULL, NULL);
  else if(priv->sni->string[icon] && *(priv->sni->string[icon]))
    scale_image_set_image(priv->icon, priv->sni->string[icon],
        priv->sni->string[SNI_PROP_THEME]);
  else if(priv->sni->pixbuf[pix-SNI_PROP_ICONPIX])
    scale_image_set_pixbuf(priv->icon,
        priv->sni->pixbuf[pix-SNI_PROP_ICONPIX]);

  if(priv->sni->string[SNI_PROP_LABEL] &&
      *(priv->sni->string[SNI_PROP_LABEL]))
  {
    gtk_label_set_markup(GTK_LABEL(priv->label),
      priv->sni->string[SNI_PROP_LABEL]);
    if( priv->sni->string[SNI_PROP_LGUIDE] &&
        *(priv->sni->string[SNI_PROP_LGUIDE]) )
      gtk_label_set_width_chars(GTK_LABEL(priv->label),
        strlen(priv->sni->string[SNI_PROP_LGUIDE]));
    css_remove_class(priv->label, "hidden");
  }
  else
    css_add_class(priv->label, "hidden");
}

SniItem *tray_item_get_sni ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_val_if_fail(IS_TRAY_ITEM(self),NULL);
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  return priv->sni;
}

static gboolean tray_item_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TrayItemPrivate *priv;
  GtkAllocation alloc,walloc;
  GdkRectangle geo;
  gchar *method = NULL, *dir;
  gint32 x,y, delta;

  g_return_val_if_fail(IS_TRAY_ITEM(self),FALSE);
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  if(!ev || !priv->sni)
    return FALSE;

  if(ev->type == GDK_SCROLL)
  {
    if( ev->scroll.direction == GDK_SCROLL_DOWN ||
        ev->scroll.direction == GDK_SCROLL_RIGHT )
      delta=1;
    else
      delta=-1;
    if( ev->scroll.direction == GDK_SCROLL_DOWN ||
        ev->scroll.direction == GDK_SCROLL_UP )
      dir = "vertical";
    else
      dir = "horizontal";

    g_debug("sni %s: dimension: %s, delta: %d", priv->sni->dest, dir, delta);
    g_dbus_connection_call(sni_get_connection(), priv->sni->dest,
        priv->sni->path, priv->sni->host->item_iface, "Scroll",
        g_variant_new("(is)", delta, dir), NULL, G_DBUS_CALL_FLAGS_NONE, -1,
        NULL, NULL, NULL);
    return TRUE;
  }

  if(ev->type == GDK_BUTTON_RELEASE)
  {
    g_debug("sni %s: button: %d",priv->sni->dest, ev->button.button);
    if((ev->button.button == 1 && priv->sni->menu) || ev->button.button == 3)
    {
      if(priv->sni->menu_path)
        sni_get_menu(self, ev);
      else
        method = "ContextMenu";
    }
    else if(ev->button.button == 1)
      method = "Activate";
    else if(ev->button.button == 2)
      method = "SecondaryActivate";
    else
      return FALSE;

    gdk_monitor_get_geometry(widget_get_monitor(self),&geo);
    gtk_widget_get_allocation(self,&alloc);
    gtk_widget_get_allocation(gtk_widget_get_toplevel(self),&walloc);

    switch(bar_get_toplevel_dir(self))
    {
      case GTK_POS_RIGHT:
        x = geo.width - walloc.width + ev->button.x + alloc.x;
        y = ev->button.y + alloc.y;
        break;
      case GTK_POS_LEFT:
        x = walloc.width;
        y = ev->button.y + alloc.y;
        break;
      case GTK_POS_TOP:
        y = walloc.height;
        x = ev->button.x + alloc.x;
        break;
      default:
        y = geo.height - walloc.height;
        x = ev->button.x + alloc.x;
    }

    // call event at 0,0 to avoid menu popping up under the bar
    if(method)
    {
      g_debug("sni: calling %s on %s at ( %d, %d )", method, priv->sni->dest,
          x,y);
      g_dbus_connection_call(sni_get_connection(), priv->sni->dest,
          priv->sni->path, priv->sni->host->item_iface, method,
          g_variant_new("(ii)", 0, 0), NULL, G_DBUS_CALL_FLAGS_NONE, -1,
          NULL, NULL, NULL);
    }
    return TRUE;
  }
  return FALSE;
}

static gint tray_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  TrayItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_TRAY_ITEM(a), 0);
  g_return_val_if_fail(IS_TRAY_ITEM(b), 0);

  p1 = tray_item_get_instance_private(TRAY_ITEM(a));
  p2 = tray_item_get_instance_private(TRAY_ITEM(b));
  return g_strcmp0(p1->sni->string[SNI_PROP_TITLE],
      p2->sni->string[SNI_PROP_TITLE]);
}

static void tray_item_invalidate ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TRAY_ITEM(self));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  flow_grid_invalidate(priv->tray);
  priv->invalid = TRUE;
}

static void tray_item_class_init ( TrayItemClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->action_exec = tray_item_action_exec;
  FLOW_ITEM_CLASS(kclass)->update = tray_item_update;
  FLOW_ITEM_CLASS(kclass)->compare = tray_item_compare;
  FLOW_ITEM_CLASS(kclass)->invalidate = tray_item_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_source =
    (void *(*)(GtkWidget *))tray_item_get_sni;
}

static void tray_item_init ( TrayItem *self )
{
}

GtkWidget *tray_item_new( SniItem *sni, GtkWidget *tray )
{
  GtkWidget *self;
  GtkWidget *box;
  TrayItemPrivate *priv;
  gint dir;

  g_return_val_if_fail(sni,NULL);
  self = GTK_WIDGET(g_object_new(tray_item_get_type(), NULL));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  priv->button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(self), priv->button);
  gtk_widget_set_name(priv->button, "tray_active");
  gtk_widget_style_get(priv->button, "direction", &dir, NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(priv->button), box);
  flow_grid_child_dnd_enable(tray, self, priv->button);

  priv->icon = scale_image_new();
  priv->label = gtk_label_new("");
  priv->sni = sni;
  priv->tray = tray;
  priv->invalid = TRUE;

  gtk_grid_attach_next_to(GTK_GRID(box),priv->icon, NULL, dir, 1, 1);
  gtk_grid_attach_next_to(GTK_GRID(box), priv->label, priv->icon, dir, 1, 1);

  g_object_ref_sink(self);
  flow_grid_add_child(tray, self);

  gtk_widget_add_events(GTK_WIDGET(self), GDK_SCROLL_MASK);
  return self;
}
