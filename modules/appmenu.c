/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- Sfwbar maintainers
 */

#include <glib.h>
#include "../src/module.h"
#include "../src/sfwbar.h"
#include "../src/appinfo.h"
#include "../src/menu.h"
#include "../src/scaleimage.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
extern ModuleInterfaceV1 sfwbar_interface;

static GHashTable *app_menu_items;
static GtkWidget *app_menu_main;
static gchar *app_menu_name = "app_menu_system";

typedef struct _app_menu_map_entry {
  gchar *from;
  gchar *to;
  gboolean authorative;
} app_menu_map_entry_t;

typedef struct _app_menu_dir {
  gchar *category;
  gchar *title;
  gchar *icon;
  GtkWidget *widget;
} app_menu_dir_t;

typedef struct _app_menu_item_t {
  app_menu_dir_t *cat;
  gchar *id;
  gchar *name;
  gchar *icon;
  GtkWidget *widget;
} app_menu_item_t;

app_menu_dir_t app_menu_map[] = {
  {"Settings", "System Settings", "preferences-system"},
  {"AudioVideo", "Sound & Video", "applications-multimedia"},
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
  {"Midi", "AudioVideo", TRUE},
  {"Mixer", "AudioVideo", TRUE},
  {"Sequencer", "AudioVideo", TRUE},
  {"Tuner", "AudioVideo", TRUE},
  {"TV", "AudioVideo", TRUE},
  {"AudioVideoEditing", "AudioVideo", TRUE},
  {"Player", "AudioVideo", TRUE},
  {"Recorder", "AudioVideo", TRUE},
  {"DiscBurning", "AudioVideo", TRUE},
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
  {"Music", "AudioVideo", FALSE},
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
    return NULL;

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
  return result;
}

static void app_menu_item_free ( app_menu_item_t *item )
{
  g_hash_table_remove(app_menu_items, item->id);
  g_free(item->name);
  g_free(item->id);
  g_free(item->icon);
  g_free(item);
}

static GtkWidget *app_menu_item_build ( gchar *title, gchar *icon )
{
  GtkWidget *menu_item, *grid, *label, *image;

  menu_item = gtk_menu_item_new();
  gtk_widget_set_name(menu_item, "menu_item");
  grid = gtk_grid_new();

  image = scale_image_new();
  scale_image_set_image(image, icon, NULL);
  gtk_grid_attach(GTK_GRID(grid), image, 1, 1, 1, 1);

  if(title)
  {
    label = gtk_label_new_with_mnemonic(title);
    gtk_grid_attach(GTK_GRID(grid), label, 2, 1, 1, 1);
  }
  gtk_container_add(GTK_CONTAINER(menu_item), grid);
  g_object_set_data(G_OBJECT(menu_item), "title", title);

  return menu_item;
}

static void app_menu_item_insert ( GtkWidget *menu, GtkWidget *item )
{
  GList *list, *iter;
  gchar *title;
  gint count = 0;

  title = g_object_get_data(G_OBJECT(item), "title");
  list = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=list; iter; iter=g_list_next(iter))
  {
    if(g_ascii_strcasecmp(title, g_object_get_data(iter->data, "title"))<=0)
      break;
    count++;
  }
  g_list_free(list);
  gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, count);
}

static void app_menu_activate_cb ( GtkWidget *w, gchar *id )
{
  GDesktopAppInfo *app;

  if( !(app = g_desktop_app_info_new(id)) )
    return;
  g_app_info_launch((GAppInfo *)app, NULL, NULL, NULL);
  g_object_unref(app);
}

static void app_menu_handle_add ( const gchar *id )
{
  GDesktopAppInfo *app;
  GtkWidget *submenu;
  app_menu_dir_t *cat;
  app_menu_item_t *item;

  if(g_hash_table_lookup(app_menu_items, id))
    return;
  if( !(app = g_desktop_app_info_new(id)) )
    return;
  if(!(cat = app_menu_cat_lookup(g_desktop_app_info_get_categories(app))))
    return;
  if(!g_desktop_app_info_get_nodisplay(app))
  {
    item = g_malloc0(sizeof(app_menu_item_t));
    if( !(item->icon = g_strdup(g_desktop_app_info_get_string(app, "Icon"))) )
      item->icon = g_strdup(cat->icon);
    item->name = g_strdup(g_app_info_get_display_name((GAppInfo *)app));
    item->cat = cat;
    item->id = g_strdup(id);
    item->widget = app_menu_item_build(item->name, item->icon);

    g_hash_table_insert(app_menu_items, item->id, item);
    g_object_set_data_full(G_OBJECT(item->widget), "iteminfo", item,
        (GDestroyNotify)app_menu_item_free);
    g_signal_connect(G_OBJECT(item->widget), "activate",
        G_CALLBACK(app_menu_activate_cb), item->id);

    if(!cat->widget)
    {
      cat->widget = app_menu_item_build(cat->title, cat->icon);
      submenu = gtk_menu_new();
      gtk_menu_set_reserve_toggle_size(GTK_MENU(submenu), FALSE);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(cat->widget), submenu);
      app_menu_item_insert(app_menu_main, cat->widget);
    }

    app_menu_item_insert(
        gtk_menu_item_get_submenu(GTK_MENU_ITEM(cat->widget)), item->widget);

    g_debug("appmenu item: '%s', title: '%s', icon: '%s', cat: %s'", item->id,
        item->name, item->icon, item->cat?item->cat->title:"null");
  }
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

gboolean sfwbar_module_init ( void )
{
  app_menu_items = g_hash_table_new(g_str_hash, g_str_equal);
  app_menu_main = menu_new(app_menu_name);
  app_info_add_handlers(app_menu_handle_add, app_menu_handle_delete);
  
  return TRUE;
}
