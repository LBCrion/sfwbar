/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "gui/bar.h"
#include "gui/basewidget.h"
#include "gui/filter.h"

static GEnumValue filter_enum[] = {
  { 0, "none", "none" },
  { FILTER_FLOATING, "floating", "floating" },
  { FILTER_OUTPUT, "output", "output" },
  { FILTER_WORKSPACE, "workspace", "workspace" },
  { 0, NULL, NULL },
};

GType filter_type;

GType filter_type_get ( void )
{
  if(!filter_type)
    filter_type = g_enum_register_static("window_filter", filter_enum);

  return filter_type;
}

gboolean filter_window_check ( GtkWidget *parent, window_t *win )
{
  gint filter;

  g_return_val_if_fail(GTK_IS_WIDGET(parent), FALSE);
  g_return_val_if_fail(win, FALSE);

  g_object_get(G_OBJECT(parent), "filter", &filter, NULL);

  if(filter & FILTER_FLOATING && !win->floating)
    return FALSE;
  if(filter & FILTER_OUTPUT && win->outputs && !g_list_find_custom(
        win->outputs, bar_get_output(parent), (GCompareFunc)g_strcmp0))
    return FALSE;
  if(filter & FILTER_WORKSPACE && win->workspace &&
      win->workspace->id != workspace_get_active(parent))
    return FALSE;
  return !wintree_is_filtered(win);
}
