/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- Sfwbar maintainers
 */

#include <glib.h>
#include "locale1.h"
#include "trigger.h"
#include "module.h"
#include "meson.h"
#include "sfwbar.h"
#include "appinfo.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/scaleimage.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
extern ModuleInterfaceV1 sfwbar_interface;

static GHashTable *app_menu_items;
static GHashTable *app_menu_filter;
static GtkWidget *app_menu_main;
static gchar *app_menu_name = "app_menu_system";
static gboolean app_menu_flat;

typedef struct _app_menu_map_entry {
  gchar *from;
  gchar *to;
  gboolean authorative;
} app_menu_map_entry_t;

typedef struct _app_menu_dir {
  gchar *category;
  gchar *title;
  gchar *icon;
  gchar *local_title;
  gchar *icon_override;
  GtkWidget *widget;
} app_menu_dir_t;

typedef struct _app_menu_item_t {
  app_menu_dir_t *cat;
  gchar *id;
  GtkWidget *widget;
} app_menu_item_t;

#define CATEGORY_DEFAULT 10
app_menu_dir_t app_menu_map[] = {
  {"Settings", "Settings", "preferences-system"},
  {"Multimedia", "Sound & Video", "applications-multimedia"},
  {"Accessibility", "Universal Access", "preferences-desktop-accessibility"},
  {"Development", "Programming", "applications-development"},
  {"Education", "Education", "applications-science"},
  {"Games", "Games", "applications-games"},
  {"Graphics", "Graphics", "applications-graphics"},
  {"Network", "Internet", "applications-internet"},
  {"Office", "Office", "applications-office"},
  {"System", "System Tools", "applications-system"},
  {"Utility", "Accessories", "applications-accessories"},
  {"Core", "Accessories", "applications-accessories"},
  {NULL, NULL}
};

app_menu_map_entry_t app_menu_additional[] = {
  {"AudioVideo", "Multimedia", TRUE},
  {"Building", "Development", TRUE},
  {"Debugger", "Development", TRUE},
  {"IDE", "Development", TRUE},
  {"GUIDesigner", "Development", TRUE},
  {"Profiling", "Development", TRUE},
  {"RevisionControl", "Development", TRUE},
  {"Translation", "Development", TRUE},
  {"Calendar", "Office", TRUE},
  {"ContactManagement", "Office", TRUE},
  {"Database", "Office", TRUE},
  {"Dictionary", "Office", TRUE},
  {"Chart", "Office", TRUE},
  {"Email", "Office", FALSE},
  {"Finance", "Office", TRUE},
  {"FlowChart", "Office", TRUE},
  {"PDA", "Office", TRUE},
  {"ProjectManagement", "Office", FALSE},
  {"Presentation", "Office", TRUE},
  {"Spreadsheet", "Office", TRUE},
  {"WordProcessor", "Office", TRUE},
  {"2DGraphics", "Graphics", TRUE},
  {"VectorGraphics", "Graphics", TRUE},
  {"RasterGraphics", "Graphics", TRUE},
  {"3DGraphics", "Graphics", TRUE},
  {"Scanning", "Graphics", TRUE},
  {"OCR", "Graphics", TRUE},
  {"Photography", "Graphics", FALSE},
  {"Publishing", "Graphics", FALSE},
  {"Viewer", "Graphics", FALSE},
  {"TextTools", "Utility", TRUE},
  {"DesktopSettings", "Settings", TRUE},
  {"HardwareSettings", "Settings", TRUE},
  {"Printing", "Settings", TRUE},
  {"PackageManager", "Settings", TRUE},
  {"Dialup", "Network", TRUE},
  {"InstantMessaging", "Network", TRUE},
  {"Chat", "Network", TRUE},
  {"IRCClient", "Network", TRUE},
  {"FileTransfer", "Network", TRUE},
  {"HamRadio", "Network", FALSE},
  {"News", "Network", TRUE},
  {"P2P", "Network", TRUE},
  {"RemoteAccess", "Network", TRUE},
  {"Telephony", "Network", TRUE},
  {"TelephonyTools", "Utility", TRUE},
  {"VideoConference", "Network", TRUE},
  {"WebBrowser", "Network", TRUE},
  {"WebDevelopment", "Development", FALSE},
  {"Midi", "Multimedia", TRUE},
  {"Mixer", "Multimedia", TRUE},
  {"Sequencer", "Multimedia", TRUE},
  {"Tuner", "Multimedia", TRUE},
  {"TV", "Multimedia", TRUE},
  {"AudioVideoEditing", "Multimedia", TRUE},
  {"Player", "Multimedia", TRUE},
  {"Recorder", "Multimedia", TRUE},
  {"DiscBurning", "Multimedia", TRUE},
  {"ActionGame", "Game", TRUE},
  {"AdventureGame", "Game", TRUE},
  {"ArcadeGame", "Game", TRUE},
  {"BoardGame", "Game", TRUE},
  {"BlocksGame", "Game", TRUE},
  {"CardGame", "Game", TRUE},
  {"KidsGame", "Game", TRUE},
  {"LogicGame", "Game", TRUE},
  {"RolePlaying", "Game", TRUE},
  {"Simulation", "Game", TRUE},
  {"SportsGame", "Game", TRUE},
  {"StrategyGame", "Game", TRUE},
  {"Art", "Education", TRUE},
  {"Construction", "Education", TRUE},
  {"Music", "Multimedia", FALSE},
  {"Languages", "Education", TRUE},
  {"Science", "Education", TRUE},
  {"ArtificialIntelligence", "Education", TRUE},
  {"Astronomy", "Education", TRUE},
  {"Biology", "Education", TRUE},
  {"Chemistry", "Education", TRUE},
  {"ComputerScience", "Education", TRUE},
  {"DataVisualization", "Education", TRUE},
  {"Economy", "Education", TRUE},
  {"Electricity", "Education", TRUE},
  {"Geography", "Education", TRUE},
  {"Geology", "Education", TRUE},
  {"Geoscience", "Education", TRUE},
  {"History", "Education", TRUE},
  {"ImageProcessing", "Education", TRUE},
  {"Literature", "Education", TRUE},
  {"Math", "Education", TRUE},
  {"NumericalAnalysis", "Education", TRUE},
  {"MedicalSoftware", "Education", TRUE},
  {"Physics", "Education", TRUE},
  {"Robotics", "Education", TRUE},
  {"Sports", "Education", TRUE},
  {"ParallelComputing", "Education", TRUE},
  {"Amusement", "Game", TRUE},
  {"Archiving", "Utility", TRUE},
  {"Compression", "Utility", TRUE},
  {"Electronics", "Education", TRUE},
  {"Emulator", "System", TRUE},
  {"Engineering", "Education", TRUE},
  {"FileTools", "Utility", FALSE},
  {"FileManager", "System", FALSE},
  {"TerminalEmulator", "System", TRUE},
  {"Filesystem", "System", TRUE},
  {"Monitor", "System", TRUE},
  {"Security", "System", TRUE},
  {"Calculator", "Utility", TRUE},
  {"Clock", "Utility"},
  {"TextEditor", "Utility"},
  {NULL, NULL, FALSE}
};

static app_menu_dir_t *app_menu_main_lookup ( const gchar *cat, gssize len )
{
  gint i;

  if(len<0)
    len = strlen(cat);

  for(i=0; app_menu_map[i].category; i++)
    if(!g_ascii_strncasecmp(cat, app_menu_map[i].category, len))
      return &app_menu_map[i];

  return NULL;
}

static app_menu_dir_t *app_menu_cat_lookup ( const gchar *cats )
{
  app_menu_dir_t *dir, *result = NULL;
  const gchar *ptr, *end;
  gsize len;
  gint i;

  if(!cats)
    return &app_menu_map[CATEGORY_DEFAULT];

  for(ptr=cats; ptr; ptr=end?end+1:NULL)
  {
    if( (end = strchr(ptr, ';')) )
      len = end - ptr;
    else
      len = strlen(ptr);

    if(len)
    {
      if( (dir = app_menu_main_lookup(ptr, len)) )
        return dir;

      for(i=0; app_menu_additional[i].from; i++)
        if(!g_ascii_strncasecmp(ptr, app_menu_additional[i].from, len))
          if( (dir = app_menu_main_lookup(app_menu_additional[i].to, -1)) )
          {
            if(app_menu_additional[i].authorative)
              return dir;
            else
              result = dir;
          }
    }
  }
  return result?result:&app_menu_map[CATEGORY_DEFAULT];
}

static void app_menu_item_free ( app_menu_item_t *item )
{
  g_hash_table_remove(app_menu_items, item->id);
  g_free(item->id);
  g_free(item);
}

static void app_menu_item_insert ( GtkWidget *menu, GtkWidget *item )
{
  GList *list, *iter;
  gint count = 0;

  list = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=list; iter; iter=g_list_next(iter))
  {
    if(menu_item_compare(item, iter->data)<=0)
      break;
    count++;
  }
  g_list_free(list);
  gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, count);
}

static gboolean app_menu_check_boolean ( GDesktopAppInfo *app, gchar *key )
{
  gchar *value;
  gboolean result;

  if( !(value = g_desktop_app_info_get_string(app, key)) )
    return FALSE;

  result = !g_ascii_strncasecmp(value, "true", 4) && !g_ascii_isalpha(value[4]);
  g_free(value);
  return result;
}

static gboolean app_menu_check_present ( GDesktopAppInfo *app, gchar *key )
{
gchar *value;
gboolean result;

if( !(value = g_desktop_app_info_get_string(app, key)) )
  return FALSE;

result = !!value;
g_free(value);
return result;
}

static gboolean app_menu_add ( gchar *id )
{
  GDesktopAppInfo *app;
  GtkWidget *submenu;
  app_menu_dir_t *cat;
  app_menu_item_t *item;

  if(g_hash_table_lookup(app_menu_filter, id))
  {
    g_debug("appmenu item: filter out '%s'", id);
    g_free(id);
    return FALSE;
  }
  if(g_hash_table_lookup(app_menu_items, id) ||
      !(app = g_desktop_app_info_new(id)) )
  {
    g_free(id);
    return FALSE;
  }

  if( !app_menu_check_boolean(app, "Hidden") &&
      !app_menu_check_boolean(app, "NoDisplay") &&
      !app_menu_check_present(app, "NotShowIn") &&
      g_desktop_app_info_get_show_in(app, NULL) &&
      (cat = app_menu_cat_lookup(g_desktop_app_info_get_categories(app))) )
  {
    item = g_malloc0(sizeof(app_menu_item_t));
    item->cat = cat;
    item->id = g_strdup(id);
    item->widget = menu_item_get(NULL, TRUE);
    menu_item_update_from_app(item->widget, app);
    menu_item_set_sort_index(item->widget, 500);

    g_hash_table_insert(app_menu_items, item->id, item);
    g_object_set_data_full(G_OBJECT(item->widget), "iteminfo", item,
        (GDestroyNotify)app_menu_item_free);

    if(!cat->widget && !app_menu_flat)
    {
      cat->widget = menu_item_get(NULL, TRUE);
      menu_item_set_label(cat->widget,
          cat->local_title? cat->local_title : cat->title);
      menu_item_set_icon(cat->widget, cat->icon);
      menu_item_set_sort_index(cat->widget, 500);
      submenu = gtk_menu_new();
      g_object_set_data(G_OBJECT(submenu), "sort", GINT_TO_POINTER(TRUE));
      gtk_menu_set_reserve_toggle_size(GTK_MENU(submenu), FALSE);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(cat->widget), submenu);
      app_menu_item_insert(app_menu_main, cat->widget);
    }

    app_menu_item_insert(app_menu_flat? app_menu_main :
        gtk_menu_item_get_submenu(GTK_MENU_ITEM(cat->widget)), item->widget);

//    g_debug("appmenu item: '%s', title: '%s', icon: '%s', cat: %s'", item->id,
//        item->name, item->icon, item->cat?item->cat->title:"null");
  }
  g_object_unref(app);

  return FALSE;
}

static void app_menu_handle_add ( const gchar *id )
{
  g_timeout_add(1000, (GSourceFunc)app_menu_add, g_strdup(id));
}

static void app_menu_handle_delete ( const gchar *id )
{
  app_menu_item_t *item;
  app_menu_dir_t *cat;
  GList *list;

  if( !(item = g_hash_table_lookup(app_menu_items, id)) )
    return;

  cat = item->cat;
  gtk_widget_destroy(item->widget);

  if(cat && cat->widget)
  {
    if( (list = gtk_container_get_children(GTK_CONTAINER(
          gtk_menu_item_get_submenu(GTK_MENU_ITEM(cat->widget))))) )
      g_list_free(list);
    else
      gtk_widget_destroy(g_steal_pointer(&(cat->widget)));
  }

  g_debug("appmenu item removed: '%s'", id);
}

static void app_info_categories_update1 ( const gchar *dir )
{
  GDir *gdir;
  GKeyFile *keyfile;
  gchar *path, *filename, *name, *lname;
  const gchar *file;
  gsize i;

  path = g_build_filename(dir, "desktop-directories", NULL);

  if( (gdir = g_dir_open(path, 0, NULL)) )
  {
    while( (file = g_dir_read_name(gdir)) )
    {
      filename = g_build_filename(path, file, NULL);
      keyfile = g_key_file_new();
      if(g_key_file_load_from_file(keyfile, filename,
            G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
      {
        name = g_key_file_get_locale_string(keyfile, G_KEY_FILE_DESKTOP_GROUP,
            "Name", "C", NULL);
        lname = g_key_file_get_locale_string(keyfile, G_KEY_FILE_DESKTOP_GROUP,
            "Name", locale1_get_locale(), NULL);
        for(i=0; app_menu_map[i].category; i++)
          if(!app_menu_map[i].local_title &&
              !g_strcmp0(name, app_menu_map[i].title) &&
              g_strcmp0(name, lname))
            app_menu_map[i].local_title = g_strdup(lname);
        g_free(name);
        g_free(lname);
      }
      g_key_file_unref(keyfile);
      g_free(filename);
    }
    g_dir_close(gdir);
  }

  g_free(path);
}

static void app_info_categories_update ( void )
{
  const gchar * const *sysdirs;
  gsize i;

  for(i=0; app_menu_map[i].category; i++)
    g_clear_pointer(&app_menu_map[i].local_title, g_free);
  app_info_categories_update1(g_get_user_data_dir());
  sysdirs = g_get_system_data_dirs();
  for(i=0; sysdirs[i]; i++)
    app_info_categories_update1(sysdirs[i]);
  app_info_categories_update1(SYSTEM_CONF_DIR);
}

static gboolean app_info_locale_handle ( gpointer data )
{
  app_info_categories_update();
  app_info_remove_handlers(app_menu_handle_add, app_menu_handle_delete);
  app_info_add_handlers(app_menu_handle_add, app_menu_handle_delete);

  return TRUE;
}

static value_t app_menu_func_filter ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "AppMenuFilter");
  vm_param_check_string(vm, p, 0, "AppMenuFilter");

  g_hash_table_insert(app_menu_filter, g_strdup(value_get_string(p[0])), "");
  app_menu_handle_delete (value_get_string(p[0]));

  return value_na;
}

static value_t app_menu_item_add ( vm_t *vm, value_t p[], gboolean top )
{
  GtkWidget *item;

  if(!vm->pstack->len)
    return value_na;

  item = menu_item_get(NULL, TRUE);
  menu_item_set_label(item, value_get_string(p[0]));
  menu_item_set_action(item, parser_exec_build(value_get_string(p[1])));
  menu_item_set_sort_index(item, top? 1 : 999);
  app_menu_item_insert(app_menu_main, item);

  return value_na;
}

static value_t app_menu_item_top ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "AppMenuItemTop");
  vm_param_check_string(vm, p, 0, "AppMenuItemTop");

  return app_menu_item_add(vm, p, TRUE);
}

static value_t app_menu_item_bottom ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "AppMenuItemBottom");
  vm_param_check_string(vm, p, 0, "AppMenuItemBottom");

  return app_menu_item_add(vm, p, FALSE);
}

static value_t app_menu_set_flat ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "AppMenuSetFlat");
  vm_param_check_numeric(vm, p, 0, "AppMenuSetFlat");

  if(app_menu_flat != !!value_get_numeric(p[0]))
  {
    app_menu_flat = !!value_get_numeric(p[0]);
    app_info_categories_update();
    app_info_remove_handlers(app_menu_handle_add, app_menu_handle_delete);
    app_info_add_handlers(app_menu_handle_add, app_menu_handle_delete);
  }
  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  app_info_categories_update();
  locale1_init();
  trigger_add("locale1", app_info_locale_handle, NULL);

  app_menu_items = g_hash_table_new(g_str_hash, g_str_equal);
  app_menu_filter = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
      NULL);
  app_menu_main = menu_new(app_menu_name);
  g_object_set_data(G_OBJECT(app_menu_main), "sort", GINT_TO_POINTER(TRUE));
  app_info_add_handlers(app_menu_handle_add, app_menu_handle_delete);
  vm_func_add("AppMenuFilter", app_menu_func_filter, TRUE);
  vm_func_add("AppMenuItemTop", app_menu_item_top, TRUE);
  vm_func_add("AppMenuItemBottom", app_menu_item_bottom, TRUE);
  vm_func_add("AppMenuSetFlat", app_menu_set_flat, TRUE);
  
  return TRUE;
}
