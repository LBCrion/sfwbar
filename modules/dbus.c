/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- Sfwbar maintainers
 */

#include <glib.h>
#include "module.h"
#include "trigger.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

static GVariant *dbus_value2variant_handle ( value_t, const GVariantType ** );

static GVariant *dbus_value2variant_basic ( value_t v, const GVariantType *t )
{
  if(g_variant_type_equal(t, G_VARIANT_TYPE_STRING))
    return g_variant_new_string(value_get_string(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_OBJECT_PATH))
    return g_variant_new_string(value_get_string(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_SIGNATURE))
    return g_variant_new_string(value_get_string(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_BOOLEAN))
    return g_variant_new_boolean(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_BYTE))
    return g_variant_new_byte(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_INT16))
    return g_variant_new_int16(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_UINT16))
    return g_variant_new_uint16(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_INT32))
    return g_variant_new_int32(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_UINT32))
    return g_variant_new_uint32(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_INT64))
    return g_variant_new_int64(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_UINT64))
    return g_variant_new_uint64(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_HANDLE))
    return g_variant_new_handle(value_get_numeric(v));
  if(g_variant_type_equal(t, G_VARIANT_TYPE_DOUBLE))
    return g_variant_new_double(value_get_numeric(v));

  g_warning("dbus: invalid variant type '%.*s'",
      (gint)g_variant_type_get_string_length(t),
      g_variant_type_peek_string(t));

  return NULL;
}

static GVariant *dbus_value2variant_array ( value_t v, const GVariantType *t )
{
  const GVariantType *s;
  GVariantBuilder *builder;
  GVariant *var;
  gsize i;

  if(!value_is_array(v))
    return NULL;
  s = g_variant_type_element(t);
  builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

  for(i=0; i<v.value.array->len; i++)
    if( (var = dbus_value2variant_handle(
            g_array_index(v.value.array, value_t, i), &s)) )
      g_variant_builder_add_value(builder, var);
    else
    {
      g_variant_builder_unref(builder);
      return NULL;
    }

  return g_variant_builder_end(builder);
}

static GVariant *dbus_value2variant_tuple ( value_t v, const GVariantType *t )
{
  const GVariantType *s;
  GVariantBuilder *builder;
  GVariant *var;
  gsize i = 0;

  if(!value_is_array(v))
    return NULL;

  builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
  for(s = g_variant_type_first(t); s; s = g_variant_type_next(s))
  {
    if(i>=v.value.array->len)
    {
      g_warning("dbus: variant string is longer than value array");
      return NULL;
    }
    if( (var = dbus_value2variant_handle(
            g_array_index(v.value.array, value_t, i++), &s)) )
      g_variant_builder_add_value(builder, var);
    else
    {
      g_warning("dbus: unable to convert value to variant");
      g_variant_builder_unref(builder);
      return NULL;
    }
  }

  return g_variant_builder_end(builder);
}

static GVariant *dbus_value2variant_handle ( value_t v, const GVariantType **t )
{
  if(g_variant_type_is_basic(*t))
    return dbus_value2variant_basic(v, *t);
  if(g_variant_type_is_tuple(*t))
    return dbus_value2variant_tuple(v, *t);
  if(g_variant_type_is_dict_entry(*t))
    return dbus_value2variant_tuple(v, *t);
  if(g_variant_type_is_array(*t))
    return dbus_value2variant_array(v, *t);
  if(g_variant_type_is_variant(*t))
  {
    if( !(*t = (GVariantType *)g_variant_type_next(*t)) )
    {
      g_warning("dbus: variant underlying type is missing");
      return NULL;
    }
    return g_variant_new_variant(dbus_value2variant_handle(v, t));
  }

  return NULL;
}

static GVariant *dbus_value2variant ( value_t v, gchar *type_str )
{
  GVariantType *type;
  GVariant *var;

  if( !(type = g_variant_type_new(type_str)) )
  {
    g_warning("dbus: Invalid format string '%s'", type_str);
    return NULL;
  }

  if( !(var = dbus_value2variant_handle(v, (const GVariantType **)&type)) )
    g_warning("dbus: variant construction failed");
  g_variant_type_free(type);

  return var;
}

static value_t dbus_variant2value_basic ( GVariant *variant )
{
  const GVariantType *type = g_variant_get_type(variant);

  if(!g_variant_type_is_basic(type))
    return value_na;

  if(g_variant_type_equal(type, G_VARIANT_TYPE_BOOLEAN))
    return value_new_numeric(g_variant_get_boolean(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
    return value_new_numeric(g_variant_get_byte(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
    return value_new_numeric(g_variant_get_int16(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
    return value_new_numeric(g_variant_get_uint16(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
    return value_new_numeric(g_variant_get_int32(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
    return value_new_numeric(g_variant_get_uint32(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
    return value_new_numeric(g_variant_get_int64(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
    return value_new_numeric(g_variant_get_uint64(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_HANDLE))
    return value_new_numeric(g_variant_get_handle(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_DOUBLE))
    return value_new_numeric(g_variant_get_double(variant));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_STRING))
    return value_new_string(g_strdup(g_variant_get_string(variant, NULL)));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_OBJECT_PATH))
    return value_new_string(g_strdup(g_variant_get_string(variant, NULL)));
  if(g_variant_type_equal(type, G_VARIANT_TYPE_SIGNATURE))
    return value_new_string(g_strdup(g_variant_get_string(variant, NULL)));
  return value_na;
}

static value_t dbus_variant2value ( GVariant *variant )
{
  GVariantIter *iter;
  GVariant *var;
  GArray *array;
  value_t v;

  if(!variant)
    return value_na;

  if(g_variant_is_of_type(variant, G_VARIANT_TYPE_VARIANT))
    return dbus_variant2value(g_variant_get_variant(variant));
  if(!g_variant_is_container(variant))
    return dbus_variant2value_basic(variant);

  array = g_array_new(FALSE, FALSE, sizeof(value_t));
  g_array_set_clear_func(array, (GDestroyNotify)value_free);

  iter = g_variant_iter_new(variant);
  while((var = g_variant_iter_next_value(iter)))
  {
    v = dbus_variant2value(var);
    g_array_append_val(array, v);
  }

  return value_new_array(array);
}

static GDBusConnection *dbus_conn_get ( value_t v )
{
  return g_bus_get_sync(g_ascii_strcasecmp(value_get_string(v), "system")?
      G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM, NULL, NULL);
}

static value_t dbus_action_call (vm_t *vm, value_t p[], gint np)
{
  GVariant *variant;
  value_t result;
  GError *err = NULL;
  GArray *array;

  vm_param_check_np_range(vm, np, 2, 4, "DbusCall");
  if( !(array = value_get_array(p[0])) || array->len != 4 )
    return value_na;

  variant = g_dbus_connection_call_sync(
      dbus_conn_get(g_array_index(array, value_t, 0)),
      value_get_string(g_array_index(array, value_t, 1)),
      value_get_string(g_array_index(array, value_t, 2)),
      value_get_string(g_array_index(array, value_t, 3)),
      value_get_string(p[1]),
      np==4? dbus_value2variant(p[3], value_get_string(p[2])) : NULL,
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

  if(err)
  {
    g_message("err: %s", err->message);
    g_error_free(err);
    return value_na;
  }

  result = dbus_variant2value(variant);
  g_variant_unref(variant);

  return result;
}

static void dbus_signal_cb ( GDBusConnection *con, const gchar *sender,
    const gchar *obj, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  vm_store_t *store;

  if(!params)
    return;

  store = vm_store_new(NULL, FALSE);
  vm_store_insert_full(store, "DbusSignal", dbus_variant2value(params));
  trigger_emit_with_data((gchar *)data, store);
  vm_store_unref(store);
}

static value_t dbus_action_subscribe (vm_t *vm, value_t p[], gint np)
{
  GArray *array;

  vm_param_check_np_range(vm, np, 3, 4, "DbusSubscribe");
  if( !(array = value_get_array(p[0])) || array->len != 4 )
    return value_na;

  g_dbus_connection_signal_subscribe(
      dbus_conn_get(g_array_index(array, value_t, 0)),
      value_get_string(g_array_index(array, value_t, 1)),
      value_get_string(g_array_index(array, value_t, 3)),
      value_get_string(p[1]),
      value_get_string(g_array_index(array, value_t, 2)),
      np==4? value_get_string(p[3]) : NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, dbus_signal_cb,
      g_strdup(value_get_string(p[2])), g_free);

  return value_na;
}

static void dbus_watch_appeared ( GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer trigger )
{
  trigger_emit_with_string((gchar *)trigger, "DbusStatus", g_strdup("appeared"));
}

static void dbus_watch_vanished ( GDBusConnection *con, const gchar *name,
    gpointer trigger )
{
  trigger_emit_with_string((gchar *)trigger, "DbusStatus", g_strdup("vanished"));
}

static value_t dbus_action_watch (vm_t *vm, value_t p[], gint np)
{
  vm_param_check_np(vm, np, 3, "DbusWatch");

  g_bus_watch_name(g_ascii_strcasecmp(value_get_string(p[0]), "system")?
      G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM, value_get_string(p[1]), 0,
      dbus_watch_appeared, dbus_watch_vanished,
      g_strdup(value_get_string(p[2])), g_free);

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  vm_func_add("DbusCall", dbus_action_call, TRUE, TRUE);
  vm_func_add("DbusSubscribe", dbus_action_subscribe, TRUE, TRUE);
  vm_func_add("DbusWatch", dbus_action_watch, TRUE, TRUE);

  return TRUE;
}
