#include "sfwbar.h"
#include <glib.h>
#include <gmodule.h>
#include "module.h"

GHashTable *handlers;

gboolean module_load ( gchar *name )
{
  GModule *module;
  ModuleExpressionHandler **handler;
  gint i;
  gint64 *sig;
  gchar *fname, *path;

  fname = g_strconcat(name,".so",NULL);
  path = get_xdg_config_file(fname,"/usr/lib/sfwbar");
  g_free(fname);
  module = g_module_open(path, G_MODULE_BIND_LOCAL);
  g_free(path);

  if(!module)
    return FALSE;

  if(!g_module_symbol(module,"sfwbar_module_signature",(void **)&sig))
    return FALSE;

  if( !sig || *sig != 0x73f4d956a1 )
    return FALSE;

  if(!g_module_symbol(module,"sfwbar_expression_handlers",(void **)&handler))
    return FALSE;

  for(i=0;handler[i];i++)
    if(handler[i]->function && handler[i]->name)
    {
      if(!handlers)
        handlers = g_hash_table_new((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal);
      if(g_hash_table_lookup(handlers,handler[i]->name))
        g_message("Duplicate module function: %s in module %s",
            handler[i]->name,name);
      else
        g_hash_table_insert(handlers,handler[i]->name,handler[i]);
    }

  return TRUE;
}

gboolean module_is_function ( GScanner *scanner )
{
  if(handlers && g_hash_table_lookup(handlers,scanner->value.v_identifier))
    return TRUE;
  return FALSE;
}

gboolean module_is_numeric ( gchar *identifier )
{
  ModuleExpressionHandler *handler;

  if(!handlers)
    return TRUE;

  handler = g_hash_table_lookup(handlers,identifier);
  if(!handler)
    return TRUE;
  return handler->numeric;
}

gchar *module_get_string ( GScanner *scanner )
{
  ModuleExpressionHandler *handler;

  if(!handlers)
    return NULL;

  handler = g_hash_table_lookup(handlers,scanner->value.v_identifier);
  if(!handler)
    return NULL;
  return handler->function(scanner);
}

gdouble module_get_numeric ( GScanner *scanner )
{
  gchar *str;
  gdouble val;
  str = module_get_string(scanner);
  val = g_strtod(str,NULL);
  g_free(str);
  return val;
}
