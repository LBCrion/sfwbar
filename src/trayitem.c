/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "bar.h"
#include "trayitem.h"
#include "flowgrid.h"
#include "scaleimage.h"

G_DEFINE_TYPE_WITH_CODE (TrayItem, tray_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (TrayItem));

static void tray_item_destroy ( GtkWidget *self )
{
}

void tray_item_update ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_if_fail(IS_TRAY_ITEM(self));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  if(!priv->invalid)
    return;

  if(priv->sni->string[SNI_PROP_STATUS])
  {
    switch(priv->sni->string[SNI_PROP_STATUS][0])
    {
      case 'A':
        gtk_widget_set_name(priv->button,"tray_active");
        sni_item_set_icon(priv->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
        break;
      case 'N':
        gtk_widget_set_name(priv->button,"tray_attention");
        sni_item_set_icon(priv->sni,SNI_PROP_ATTN,SNI_PROP_ATTNPIX);
        break;
      case 'P':
        gtk_widget_set_name(priv->button,"tray_passive");
        sni_item_set_icon(priv->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
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

  priv->invalid = FALSE;
}

SniItem *tray_item_get_sni ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_val_if_fail(IS_TRAY_ITEM(self),NULL);
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  return priv->sni;
}

static void tray_item_class_init ( TrayItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = tray_item_destroy;
  FLOW_ITEM_CLASS(kclass)->update = tray_item_update;
  FLOW_ITEM_CLASS(kclass)->compare = tray_item_compare;
  FLOW_ITEM_CLASS(kclass)->get_parent = 
    (void *(*)(GtkWidget *))tray_item_get_sni;
}

static void tray_item_init ( TrayItem *self )
{
}

gboolean tray_item_scroll_cb ( GtkWidget *self, GdkEventScroll *event, SniItem *sni )
{
  gchar *dir;
  gint32 delta;

  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_RIGHT))
    delta=1;
  else
    delta=-1;
  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_UP))
    dir = "vertical";
  else
    dir = "horizontal";

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->path,
    sni->host->item_iface, "Scroll", g_variant_new("(is)", delta, dir),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  return TRUE;
}

gboolean tray_item_click_cb (GtkWidget *self, GdkEventButton *ev, SniItem *sni )
{
  gchar *method=NULL;
  GtkAllocation alloc,walloc;
  GdkRectangle geo;
  gint32 x,y;

  if(ev->type != GDK_BUTTON_PRESS || ev->button < 2 || ev->button > 3)
    return FALSE;

  g_debug("sni %s: button: %d",sni->dest,ev->button);
  if(ev->button == 2)
    method = "SecondaryActivate";
  if(ev->button == 3)
    method = "ContextMenu";

  if(!method)
    return FALSE;

  gdk_monitor_get_geometry(gdk_display_get_monitor_at_window(
      gtk_widget_get_display(gtk_widget_get_toplevel(self)),
      gtk_widget_get_window(self)),&geo);
  gtk_widget_get_allocation(self,&alloc);
  gtk_widget_get_allocation(gtk_widget_get_toplevel(self),&walloc);

  switch(bar_get_toplevel_dir(self))
  {
    case GTK_POS_RIGHT:
      x = geo.width - walloc.width + ev->x + alloc.x;
      y = ev->y + alloc.y;
      break;
    case GTK_POS_LEFT:
      x = walloc.width;
      y = ev->y + alloc.y;
      break;
    case GTK_POS_TOP:
      y = walloc.height;
      x = ev->x + alloc.x;
      break;
    default:
      y = geo.height - walloc.height;
      x = ev->x + alloc.x;
  }

  // call event at 0,0 to avoid menu popping up under the bar
  g_debug("sni: calling %s on %s at ( %d, %d )",method,sni->dest,x,y);
  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->path,
    sni->host->item_iface, method, g_variant_new("(ii)", 0, 0),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  
  return TRUE;
}

gboolean tray_item_release_cb (GtkWidget *button, GdkEventButton *ev, GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_val_if_fail(IS_TRAY_ITEM(self),FALSE);
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  if(priv->last_event)
    gdk_event_free(priv->last_event);
  priv->last_event = gdk_event_copy((GdkEvent *)ev);

  return FALSE;
}
static void tray_item_button_cb( GtkWidget *button, GtkWidget *self )
{
  TrayItemPrivate *priv;
  SniItem *sni;
  gchar *method;

  g_return_if_fail(IS_TRAY_ITEM(self));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));
  sni = priv->sni;

  if(sni->menu_path)
  {
    sni_get_menu(sni,priv->last_event);
  }
  else
  {
    if(sni->menu)
      method = "ContextMenu";
    else
      method = "Activate";

    g_debug("sni: calling %s on %s",method,sni->dest);
    g_dbus_connection_call(sni_get_connection(), sni->dest, sni->path,
      sni->host->item_iface, method, g_variant_new("(ii)", 0, 0),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  }
  if(priv->last_event)
    gdk_event_free(priv->last_event);
  priv->last_event = NULL;
}

void tray_item_invalidate ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TRAY_ITEM(self));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  flow_grid_invalidate(priv->tray);
  priv->invalid = TRUE;
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
  gtk_container_add(GTK_CONTAINER(self),priv->button);
  gtk_widget_set_name(priv->button, "tray_active");
  gtk_widget_style_get(priv->button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(priv->button),box);
  flow_grid_child_dnd_enable(tray,self,priv->button);

  priv->icon = scale_image_new();
  priv->label = gtk_label_new("");
  priv->sni = sni;
  priv->sni->image = priv->icon;
  priv->tray = tray;

  gtk_grid_attach_next_to(GTK_GRID(box),priv->icon,NULL,dir,1,1);
  gtk_grid_attach_next_to(GTK_GRID(box),priv->label,priv->icon,dir,1,1);

  g_object_ref(self);
  flow_grid_add_child(tray,self);

  g_signal_connect(priv->button,"clicked",
      G_CALLBACK(tray_item_button_cb),self);
  g_signal_connect(G_OBJECT(self),"button-press-event",
      G_CALLBACK(tray_item_click_cb),priv->sni);
  g_signal_connect(G_OBJECT(priv->button),"button-release-event",
      G_CALLBACK(tray_item_release_cb),self);
  gtk_widget_add_events(GTK_WIDGET(self),GDK_SCROLL_MASK);
  g_signal_connect(G_OBJECT(self),"scroll-event",
      G_CALLBACK(tray_item_scroll_cb),priv->sni);
  return self;
}

gint tray_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  TrayItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_TRAY_ITEM(a),0);
  g_return_val_if_fail(IS_TRAY_ITEM(b),0);

  p1 = tray_item_get_instance_private(TRAY_ITEM(a));
  p2 = tray_item_get_instance_private(TRAY_ITEM(b));
  return g_strcmp0(p1->sni->string[SNI_PROP_TITLE],p2->sni->string[SNI_PROP_TITLE]);
}
