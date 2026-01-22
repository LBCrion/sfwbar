/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "gui/grid.h"
#include "gui/css.h"
#include "util/string.h"

static GHashTable *toplevel_list;

static GtkWidget *toplevel_from_name ( gchar *name )
{
  if(!toplevel_list || !name)
    return NULL;
  
  return g_hash_table_lookup(toplevel_list, name);
}

GtkWidget *toplevel_new ( gchar *name )
{
  GtkWidget *self, *grid;

  if( (self = toplevel_from_name(name)) )
    return self;

  self = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(self), GDK_WINDOW_TYPE_HINT_DIALOG);
  grid = grid_new();
  gtk_container_add(GTK_CONTAINER(self), grid);
  gtk_widget_set_name(self, name);
  gtk_widget_set_name(grid, name);
  gtk_window_set_accept_focus(GTK_WINDOW(self), TRUE);

  if(!toplevel_list)
    toplevel_list = g_hash_table_new_full((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal, g_free, (GDestroyNotify)gtk_widget_destroy);
  g_hash_table_insert(toplevel_list, g_strdup(name), self);

  return self;
}

void toplevel_hide ( gchar *name )
{
  GtkWidget *toplevel;

  if( !(toplevel = toplevel_from_name(name)) )
    return;

  gtk_widget_hide(toplevel);
}

void toplevel_show ( gchar *name )
{
  GtkWidget *toplevel;

  if( !(toplevel = toplevel_from_name(name)) )
    return;

  css_widget_cascade(gtk_bin_get_child(GTK_BIN(toplevel)), NULL);
  gtk_widget_show(toplevel);
}

void toplevel_toggle ( gchar *name )
{
  GtkWidget *toplevel;

  if( !(toplevel = toplevel_from_name(name)) )
    return;

  if(gtk_widget_is_visible(toplevel))
    toplevel_hide(name);
  else
    toplevel_show(name);
}
