/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib-unix.h>
#include <glib/gi18n.h>
#include "sfwbar.h"
#include "appinfo.h"
#include "meson.h"
#include "wayland.h"
#include "config/config.h"
#include "ipc/sway.h"
#include "gui/bar.h"
#include "gui/basewidget.h"
#include "gui/css.h"
#include "gui/menuitem.h"
#include "gui/monitor.h"
#include "gui/popup.h"
#include "vm/vm.h"

extern gchar *confname;
extern gchar *sockname;
extern GtkApplication *application;

static gchar *cssname;
static gchar *monitor;
static gchar *bar_id;
static gchar *dfilter;
static GRegex *rfilter;
static gboolean debug = FALSE;
static gchar **sargv;

static GOptionEntry entries[] = {
  {"config", 'f', 0, G_OPTION_ARG_FILENAME, &confname, "Specify config file"},
  {"css",'c', 0, G_OPTION_ARG_FILENAME, &cssname, "Specify css file"},
  {"socket", 's', 0, G_OPTION_ARG_FILENAME, &sockname,
    "Specify compositor IPC socket file"},
  {"debug", 'd',0, G_OPTION_ARG_NONE, &debug, "Display debug info"},
  {"debug-filter", 'g', 0, G_OPTION_ARG_STRING, &dfilter,
    "Filter debug output for a pattern"},
  {"monitor",'m',0, G_OPTION_ARG_STRING, &monitor,
    "Monitor to display the panel on (use \"-m list\" to list monitors`"},
  {"bar_id",'b',0, G_OPTION_ARG_STRING, &bar_id,
    "default sway bar_id to listen on for sway events"},
  {NULL}};

void parse_command_line ( gint argc, gchar **argv)
{
  GOptionContext *optc;

  optc = g_option_context_new(" - S* Floating Window Bar");
  g_option_context_add_main_entries(optc,entries,NULL);
  g_option_context_add_group(optc, gtk_get_option_group(TRUE));
  g_option_context_parse(optc,&argc,&argv,NULL);
  g_option_context_free(optc);
}

static const gchar *log_colors[] = {
  [G_LOG_LEVEL_ERROR] = "\x1B[1;31m",
  [G_LOG_LEVEL_CRITICAL] = "\x1B[1;31m",
  [G_LOG_LEVEL_WARNING] = "\x1B[1;31m",
  [G_LOG_LEVEL_INFO] = "\x1B[1;34m",
  [G_LOG_LEVEL_MESSAGE] = "\x1B[1;34m",
  [G_LOG_LEVEL_DEBUG] = "\x1B[1;90m",
};

static const gchar *log_labels[] = {
  [G_LOG_LEVEL_ERROR] = "ERROR",
  [G_LOG_LEVEL_CRITICAL] = "CRIT ",
  [G_LOG_LEVEL_WARNING] = "WARN ",
  [G_LOG_LEVEL_INFO] = "INFO ",
  [G_LOG_LEVEL_DEBUG] = "DEBUG",
  [G_LOG_LEVEL_MESSAGE] = "MSG  ",
};

void log_print ( const gchar *log_domain, GLogLevelFlags log_level, 
    const gchar *message, gpointer data )
{
  GDateTime *now;
  FILE *log_fd = stderr;

  if(!debug && (log_level==G_LOG_LEVEL_DEBUG || log_level==G_LOG_LEVEL_INFO) )
    return;

  if(rfilter && !g_regex_match(rfilter, message, 0, NULL))
      return;

  now = g_date_time_new_now_local();

  fprintf(log_fd ,"%02d:%02d:%05.02f [%s%s%s] %s\n",
      g_date_time_get_hour(now),
      g_date_time_get_minute(now),
      g_date_time_get_seconds(now),
      g_log_writer_supports_color(fileno(log_fd))? log_colors[log_level] : "",
      log_labels[log_level],
      g_log_writer_supports_color(fileno(log_fd))? "\x1B[0m" : "",
      message);

  g_date_time_unref(now);
}

static gboolean sfwbar_restart ( gpointer d )
{
  gint i, fdlimit;

  fdlimit = (int)sysconf(_SC_OPEN_MAX);
  g_debug("reload: closing fd's %d to %d",STDERR_FILENO+1,fdlimit);
  for(i=STDERR_FILENO+1; i<fdlimit; i++)
    fcntl(i,F_SETFD,FD_CLOEXEC);
  g_debug("reload: exec: %s",sargv[0]);
  execvp(sargv[0],sargv);
  exit(1);
  return FALSE;
}

static void print_store ( GQuark key, gpointer d, gpointer s)
{
  g_message("%s", g_quark_to_string(key));
}

static void activate (GtkApplication* app, gpointer data )
{
  GtkWidget *panel;
  GList *clist, *iter;

  application = app;
  bindtextdomain("sfwbar", LOCALE_DIR);
  bind_textdomain_codeset("sfwbar", "UTF-8");
  textdomain("sfwbar");
  config_init();
  expr_lib_init();
  action_lib_init();
  wayland_init();
  css_init(cssname);
  monitor_init( monitor );
  sway_ipc_init();
  hypr_ipc_init();
  wayfire_ipc_init();
  foreign_toplevel_init();
  ew_init();
  cw_init();
  app_info_init();

  if(bar_id)
    bar_address_all(NULL, bar_id, bar_set_id);

  if( !(panel = config_parse(confname?confname:"sfwbar.config", NULL, NULL)) )
  {
    g_warning("No panel defined");
    exit(1);
  }

  clist = gtk_window_list_toplevels();
  for(iter = clist; iter; iter = g_list_next(iter) )
    if(IS_BAR(iter->data))
    {
      css_widget_cascade(GTK_WIDGET(iter->data), NULL);
      base_widget_autoexec(iter->data, NULL);
      if(monitor)
        g_object_set(G_OBJECT(iter->data), monitor, NULL);
    }
  g_list_free(clist);

  g_thread_unref(g_thread_new("scanner",
        (GThreadFunc)base_widget_scanner_thread,
        g_main_context_get_thread_default()));

  g_dataset_foreach(base_widget_get_store(panel)->vars, print_store, NULL);
  vm_run_user_defined("SfwBarInit", NULL, NULL, NULL, NULL,
      base_widget_get_store(panel));

  g_unix_signal_add(SIGHUP, (GSourceFunc)sfwbar_restart, NULL);
}

int main (int argc, gchar **argv)
{
  GtkApplication *app;
  gint status, i;

  sargv = g_malloc0(sizeof(gchar *)*(argc+1));
  for(i=0; i<argc; i++)
    sargv[i] = argv[i];

  signal_subscribe();
  g_log_set_handler(NULL, G_LOG_LEVEL_MASK, log_print, NULL);

  parse_command_line(argc, argv);

  if(dfilter)
    rfilter = g_regex_new(dfilter, 0, 0, NULL);

  app = gtk_application_new ("org.hosers.sfwbar", G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
