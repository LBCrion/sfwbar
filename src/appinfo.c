#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include "appinfo.h"

static GHashTable *app_info_wm_class_map;
static GHashTable *icon_map;
static GtkIconTheme *app_info_theme;
static GList *app_info_add, *app_info_delete;
static GList *app_info_entries;
static time_t app_info_mtime;

void app_icon_map_add ( gchar *appid, gchar *icon )
{
  if(!icon_map)
    icon_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  g_hash_table_insert(icon_map, g_strdup(appid), g_strdup(icon));
}

void app_info_add_handlers ( AppInfoHandler add, AppInfoHandler del )
{
  GList *iter;

  app_info_add = g_list_append(app_info_add, add);
  app_info_delete = g_list_append(app_info_delete, del);

  if(add)
    for(iter=app_info_entries; iter; iter=g_list_next(iter))
      add(iter->data);
}

static time_t app_info_mtime_get ( GDesktopAppInfo *app )
{
  struct stat stattr;
  const gchar *fname;

  if( (fname = g_desktop_app_info_get_filename(app)) && !stat(fname, &stattr))
    return stattr.st_mtime;

  return 0;
}

static void app_info_monitor_cb ( GAppInfoMonitor *mon, gpointer d )
{
  GDesktopAppInfo *app;
  GList *list, *iter, *handler, *removed, *item;
  const gchar *class, *id;

  removed = g_list_copy(app_info_entries);
  list = g_app_info_get_all();
  g_hash_table_remove_all(app_info_wm_class_map);
  for(iter=list; iter; iter=g_list_next(iter))
  {
    if( (id = g_app_info_get_id(iter->data)) &&
        (app = g_desktop_app_info_new(id)) )
    {
      if( (class = g_desktop_app_info_get_startup_wm_class(app)) )
        g_hash_table_insert(app_info_wm_class_map, g_strdup(class),
            g_strdup(id));

      if( (item = g_list_find_custom(removed, id, (GCompareFunc)g_strcmp0)) )
        removed = g_list_remove(removed, item->data);
      if(!g_list_find_custom(app_info_entries, id, (GCompareFunc)g_strcmp0))
      {
        app_info_entries = g_list_append(app_info_entries, g_strdup(id));
        for(handler=app_info_add; handler; handler=g_list_next(handler))
          ((AppInfoHandler)(handler->data))(id);
      }
      else if(app_info_mtime_get(app) > app_info_mtime)
      {
        for(handler=app_info_delete; handler; handler=g_list_next(handler))
          ((AppInfoHandler)(handler->data))(id);
        for(handler=app_info_add; handler; handler=g_list_next(handler))
          ((AppInfoHandler)(handler->data))(id);
      }
      g_object_unref(G_OBJECT(app));
    }
  }
  for(iter=removed; iter; iter=g_list_next(iter))
  {
    for(handler=app_info_delete; handler; handler=g_list_next(handler))
      ((AppInfoHandler)(handler->data))(iter->data);
    if( (item = g_list_find_custom(app_info_entries, iter->data,
            (GCompareFunc)g_strcmp0)) )
      app_info_entries = g_list_remove(app_info_entries, item->data);
  }
  g_list_free(removed);
  g_list_free_full(list, g_object_unref);
  app_info_mtime = g_get_real_time() / 1000000;
}

void app_info_init ( void )
{
  GAppInfoMonitor *mon;

  app_info_wm_class_map = g_hash_table_new_full(g_str_hash, g_str_equal,
      g_free, g_free);
  app_info_theme = gtk_icon_theme_get_default();
  mon = g_app_info_monitor_get();
  g_signal_connect(G_OBJECT(mon), "changed", (GCallback)app_info_monitor_cb,
      NULL);
  app_info_monitor_cb(mon, NULL);
}

gchar *app_info_icon_test ( const gchar *icon, gboolean symbolic_pref )
{
  GtkIconInfo *info;
  gchar *sym_icon;

  if(!icon)
    return NULL;

/* if symbolic icon is preferred test for symbolic first */
  if(symbolic_pref)
  {
    sym_icon = g_strconcat(icon, "-symbolic", NULL);
    if( (info = gtk_icon_theme_lookup_icon(app_info_theme, sym_icon, 16, 0)) )
    {
      g_object_unref(G_OBJECT(info));
      return sym_icon;
    }
    g_free(sym_icon);
  }

/* if symbolic preference isn't set or symbolic icon isn't found */
  if( (info = gtk_icon_theme_lookup_icon(app_info_theme, icon, 16, 0)) )
  {
    g_object_unref(G_OBJECT(info));
    return g_strdup(icon);
  }

/* if symbolic preference isn't set, but non-symbolic icon isn't found */
  if(!symbolic_pref)
  {
    sym_icon = g_strconcat(icon, "-symbolic", NULL);
    if( (info = gtk_icon_theme_lookup_icon(app_info_theme, sym_icon, 16, 0)) )
    {
      g_object_unref(G_OBJECT(info));
      return sym_icon;
    }
    g_free(sym_icon);
  }

  return NULL;
}

gchar *app_info_icon_get ( const gchar *app_id, gboolean symbolic_pref )
{
  GDesktopAppInfo *app;
  char *icon, *icon_name;
  gchar *file;

  if(g_str_has_suffix(app_id, ".desktop"))
    file = g_strdup(app_id);
  else
    file = g_strconcat(app_id, ".desktop", NULL);

  app = g_desktop_app_info_new(file);
  g_free(file);

  if(!app)
    return NULL;

  if(g_desktop_app_info_get_nodisplay(app))
    icon = NULL;
  else
  {
    icon_name = g_desktop_app_info_get_string(app, "Icon");
    icon = app_info_icon_test(icon_name, symbolic_pref);
    g_free(icon_name);
  }

  g_object_unref(G_OBJECT(app));
  return icon;
}

static gchar *app_info_lookup_id ( gchar *app_id, gboolean symbolic_pref )
{
  gchar ***desktop, *wmmap, *icon = NULL;
  gint i,j;

  if( (icon = app_info_icon_test(app_id, symbolic_pref)) )
    return icon;

  if( (icon = app_info_icon_get(app_id, symbolic_pref)) )
    return icon;

  desktop = g_desktop_app_info_search(app_id);
  for(j=0; desktop[j]; j++)
  {
    if(!icon)
      for(i=0; desktop[j][i]; i++)
        if( (icon = app_info_icon_get(desktop[j][i], symbolic_pref)) )
          break;
    g_strfreev(desktop[j]);
  }
  g_free(desktop);

  if(!icon && (wmmap = g_hash_table_lookup(app_info_wm_class_map, app_id)) )
    icon = app_info_icon_get(wmmap, symbolic_pref);

  return icon;
}

gchar *app_info_icon_lookup ( gchar *app_id_in, gboolean symbolic_pref )
{
  gchar *app_id,*clean_app_id, *lower_app_id, *icon;

  if(!icon_map || !(app_id = g_hash_table_lookup(icon_map, app_id_in)) )
    app_id = app_id_in;

  if(g_str_has_suffix(app_id, "-symbolic"))
  {
    symbolic_pref = TRUE;
    clean_app_id = g_strndup(app_id, strlen(app_id) - 9);
  }
  else
    clean_app_id = g_strdup(app_id);

  if( (icon = app_info_lookup_id(clean_app_id, symbolic_pref)) )
  {
    g_free(clean_app_id);
    return icon;
  }
  lower_app_id = g_ascii_strdown(clean_app_id, -1);
  icon = app_info_lookup_id(lower_app_id, symbolic_pref);
  g_free(lower_app_id);
  g_free(clean_app_id);

  return icon;
}
