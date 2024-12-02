/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <xkbcommon/xkbregistry.h>
#include "module.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
static struct rxkb_context *ctx;

static const gchar *xkb_get_value ( struct rxkb_layout *layout, gchar *type )
{
  if(!g_ascii_strcasecmp(type,"description"))
    return rxkb_layout_get_description(layout);
  if(!g_ascii_strcasecmp(type,"name"))
    return rxkb_layout_get_name(layout);
  if(!g_ascii_strcasecmp(type,"variant"))
    return rxkb_layout_get_variant(layout);
  if(!g_ascii_strcasecmp(type,"brief"))
    return rxkb_layout_get_brief(layout);
  return g_strdup_printf("XkbMap: Invalid type: %s",type);
}

static value_t xkb_map_expr_func ( vm_t *vm, value_t p[], gint np )
{
  struct rxkb_layout *layout;

  if(np!=3 || !value_is_string(p[0]) || !value_is_string(p[1]) ||
      !value_is_string(p[2]))
    return value_new_string(g_strdup("XkbMap: missing parameter(s)"));

  for(layout=rxkb_layout_first(ctx); layout; layout=rxkb_layout_next(layout))
  {
    if(!g_strcmp0(value_get_string(p[0]),
          xkb_get_value(layout, value_get_string(p[1]))))
      return value_new_string(g_strdup(
            xkb_get_value(layout, value_get_string(p[2]))));
  }

  return value_new_string(g_strdup_printf("XkbMap: Unknown layout: %s",
      value_get_string(p[0])));
}

gboolean sfwbar_module_init ( void )
{
  if( !(ctx = rxkb_context_new (RXKB_CONTEXT_NO_DEFAULT_INCLUDES)) )
    return FALSE;
  vm_func_add("xkbmap", xkb_map_expr_func, TRUE);
  rxkb_context_include_path_append_default(ctx);
  rxkb_context_parse_default_ruleset(ctx);
  return TRUE;
}
