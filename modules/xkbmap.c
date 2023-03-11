/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <xkbcommon/xkbregistry.h>
#include "../src/module.h"

static ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;
static struct rxkb_context *ctx;

void sfwbar_module_init ( ModuleApiV1 *api )
{
  sfwbar_module_api = api;

  ctx = rxkb_context_new (RXKB_CONTEXT_NO_DEFAULT_INCLUDES);
  rxkb_context_include_path_append_default(ctx);
  rxkb_context_parse_default_ruleset(ctx);
}

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

void *xkb_map_expr_func ( void **params )
{
  struct rxkb_layout *layout;

  if(!params || !params[0] || !params[1] || !params[2])
    return g_strdup("XkbMap: missing parameter(s)");

  for(layout=rxkb_layout_first(ctx);layout;layout=rxkb_layout_next(layout))
  {
    if(!g_strcmp0(params[0],xkb_get_value(layout,params[1])))
      return g_strdup(xkb_get_value(layout,params[2]));
  }

  return g_strdup_printf("XkbMap: Unknown layout: %s",(gchar *)params[0]);
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = 0,
  .name = "XkbMap",
  .parameters = "SSS",
  .function = xkb_map_expr_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};
