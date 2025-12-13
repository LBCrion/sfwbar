/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <glib.h>
#include "input.h"
#include "trigger.h"
#include "util/string.h"

static gchar *layout;
static gchar **layout_list;
static struct input_api *api;

void input_api_register ( struct input_api *new )
{
  api = new;
}

gboolean input_api_check ( void )
{
  return !!api;
}

void input_layout_set ( const gchar *new_layout )
{
  if(!g_strcmp0(layout, new_layout))
    return;
  str_assign(&layout, g_strdup(new_layout));
  trigger_emit("language");
}

const gchar *input_layout_get ( void )
{
  return layout;
}

void input_layout_prev ( void )
{
  if(api && api->layout_prev)
    api->layout_prev();
}

void input_layout_next ( void )
{
  if(api && api->layout_next)
    api->layout_next();
}

void input_layout_change ( gchar *layout )
{
  if(api && api->layout_set)
    api->layout_set(layout);
}

void input_layout_list_set ( gchar **new )
{
  g_clear_pointer(&layout_list, g_strfreev);
  g_atomic_pointer_set(&layout_list, new);
  trigger_emit("language-list");
}

gchar **input_layout_list_get ( void )
{
  return g_atomic_pointer_get(&layout_list);
}
