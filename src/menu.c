/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "menu.h"

static GHashTable *menus;

GtkWidget *menu_from_name ( gchar *name )
{
  if(!menus || !name)
    return NULL;
  return g_hash_table_lookup(menus, name);
}

void menu_remove ( gchar *name )
{
  if(name)
    g_hash_table_remove(menus,name);
}

void menu_add ( gchar *name, GtkWidget *menu )
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

void menu_popup( GtkWidget *widget, GtkWidget *menu, GdkEvent *event,
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

gboolean menu_action_cb ( GtkWidget *w ,action_t *action )
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
