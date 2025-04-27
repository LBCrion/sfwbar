
#include <locale.h>
#include <gio/gio.h>
#include "trigger.h"
#include "vm/expr.h"

static const gchar *locale_iface = "org.freedesktop.locale1";
static const gchar *locale_path = "/org/freedesktop/locale1";
static gchar *locale1;

static void locale1_handle ( GVariantIter *iter )
{
  gchar *lstr, **lstrv, *locale = NULL;

  while(g_variant_iter_next(iter, "&s", &lstr))
  {
    lstrv = g_strsplit(lstr, "=", 2);
    if(!g_strcmp0(lstrv[0], "LC_MESSAGE"))
    {
      g_free(locale);
      locale = g_strdup(lstrv[1]);
    }
    else if(!g_strcmp0(lstrv[0], "LANG") && !locale)
      locale = g_strdup(lstrv[1]);
    g_strfreev(lstrv);
  }
  if(!locale || !g_strcmp0(locale, locale1))
  {
    g_free(locale);
    return;
  }
  g_clear_pointer(&locale1, g_free);
  locale1 = locale;
  setlocale(LC_MESSAGES, locale1);
  trigger_emit("locale1");
  expr_dep_trigger(g_quark_from_static_string(".locale1"));
}

static void locale1_cb ( GDBusConnection *con, GAsyncResult *res,
    void *data)
{
  GVariant *result, *inner;
  GVariantIter *iter;

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
    return;
  g_variant_get(result, "(v)", &inner);
  if(inner && g_variant_is_of_type(inner, G_VARIANT_TYPE("as")))
  {
    g_variant_get(inner, "as", &iter);
    locale1_handle(iter);
    g_variant_iter_free(iter);
  }
  g_variant_unref(result);
}

static void locale1_changed_cb ( GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *signal, GVariant *params, gpointer data )
{
  GVariant *dict;
  GVariantIter *iter;

  g_variant_get(params, "(&s@a{sv}@as)", NULL, &dict, NULL);
  if(!dict)
    return;

  if(g_variant_lookup(dict, "Locale", "as", &iter))
  {
    g_dbus_connection_call(con, locale_iface, locale_path,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", locale_iface, "Locale"), NULL,
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)locale1_cb,
        NULL);
    g_variant_iter_free(iter);
  }
  g_variant_unref(dict);
}

void locale1_init ( void )
{
  GDBusConnection *con;

  locale1 = g_strdup(setlocale(LC_MESSAGES, NULL));
  if( (con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL)) )
  {
    g_dbus_connection_signal_subscribe(con, locale_iface,
        "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL,
        NULL,  G_DBUS_SIGNAL_FLAGS_NONE, locale1_changed_cb,
        NULL, NULL);

    g_dbus_connection_call(con, locale_iface, locale_path,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", locale_iface, "Locale"), NULL,
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)locale1_cb,
        NULL);
  }
}

const gchar *locale1_get_locale ( void )
{
  return locale1;
}
