/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <glib.h>
#include "input.h"
#include "trigger.h"
#include "util/string.h"

static gchar *layout;
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
