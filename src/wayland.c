#include "sfwbar.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "xdg-output-unstable-v1.h"

#define WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION 3

typedef struct zwlr_foreign_toplevel_handle_v1 wlr_fth;

static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;
static struct zxdg_output_manager_v1 *xdg_output_manager = NULL;
static gint64 wt_counter;
static struct wl_seat *seat = NULL;

static void toplevel_handle_app_id(void *data, wlr_fth *tl, const gchar *app_id)
{
  struct wt_window *win;

  win = wintree_from_id(tl);
  if(!win)
    return;
  str_assign(&(win->appid), (gchar *)app_id);
  taskbar_invalidate();
  switcher_invalidate();
}

static void toplevel_handle_title(void *data, wlr_fth *tl, const gchar *title)
{
  struct wt_window *win;

  win = wintree_from_id(tl);
  if(!win)
    return;
  str_assign(&(win->title), (gchar *)title);
  wintree_set_active(win->title);
  taskbar_set_label(win,win->title);
  switcher_set_label(win,win->title);
  taskbar_invalidate();
  switcher_invalidate();
}

static void toplevel_handle_closed(void *data, wlr_fth *tl)
{
  wintree_window_delete(tl);
  zwlr_foreign_toplevel_handle_v1_destroy(tl);
}

static void toplevel_handle_done(void *data, wlr_fth *tl)
{
  struct wt_window *win;

  win = wintree_from_id(tl);
  if(!win)
    return;
  if(win->title == NULL)
    str_assign(&(win->title), win->appid);
  wintree_set_active(win->title);
  if(win->button!=NULL)
  {
    taskbar_set_label(win,win->title);
    switcher_set_label(win,win->title);
    return;
  }
  wintree_window_append(win);
}

static void toplevel_handle_state(void *data, wlr_fth *tl,
                struct wl_array *state)
{
  uint32_t *entry;
  struct wt_window *win;

  wl_array_for_each(entry, state)
    if(*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
      return;
  wl_array_for_each(entry, state)
    if(*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
    {
      win = wintree_from_id(tl);
      if(win)
      {
        wintree_set_active(win->title);
        wintree_set_focus(win->uid);
        taskbar_invalidate();
        switcher_invalidate();
      }
    }
}

static void toplevel_handle_parent(void *data, wlr_fth *tl, wlr_fth *pt) {}
static void toplevel_handle_output_leave(void *data, wlr_fth *toplevel, struct wl_output *output) {}
static void toplevel_handle_output_enter(void *data, wlr_fth *toplevel, struct wl_output *output) {}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_impl = {
  .title = toplevel_handle_title,
  .app_id = toplevel_handle_app_id,
  .output_enter = toplevel_handle_output_enter,
  .output_leave = toplevel_handle_output_leave,
  .state = toplevel_handle_state,
  .done = toplevel_handle_done,
  .closed = toplevel_handle_closed,
  .parent = toplevel_handle_parent
};

static void toplevel_manager_handle_toplevel(void *data,
  struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager, wlr_fth *tl)
{
  struct wt_window *win;

  win = wintree_window_init();
  win->uid = tl;
  win->wid = wt_counter++;
  wintree_window_append(win);

  zwlr_foreign_toplevel_handle_v1_add_listener(tl, &toplevel_impl, NULL);
}

void foreign_toplevel_activate ( gpointer tl )
{
  zwlr_foreign_toplevel_handle_v1_unset_minimized(tl);
  zwlr_foreign_toplevel_handle_v1_activate(tl,seat);
}

static void toplevel_manager_handle_finished(void *data,
  struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager)
{
  zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
  .toplevel = toplevel_manager_handle_toplevel,
  .finished = toplevel_manager_handle_finished,
};

static void noop ()
{
}

static void xdg_output_handle_name ( void *data,
    struct zxdg_output_v1 *xdg_output, const gchar *name )
{
  gchar **dest = data;
  *dest = strdup(name);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = noop,
  .logical_size = noop,
  .done = noop,
  .name = xdg_output_handle_name,
  .description = noop,
};

gchar *gdk_monitor_get_xdg_name ( GdkMonitor *monitor )
{
  GdkDisplay *gdisp;
  struct wl_display *wdisp;
  struct wl_output *output;
  struct zxdg_output_v1 *xdg;
  gchar *name;

  if(!monitor || !GDK_IS_WAYLAND_MONITOR(monitor))
    return NULL;

  name = NULL;
  output = gdk_wayland_monitor_get_wl_output(monitor);
  gdisp = gdk_monitor_get_display(monitor);
  wdisp = gdk_wayland_display_get_wl_display(gdisp);
  xdg = zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output);
  zxdg_output_v1_add_listener(xdg,&xdg_output_listener,&name);
  wl_display_roundtrip(wdisp);
  zxdg_output_v1_destroy(xdg);

  return name;
}

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  gboolean *wlr_ft = data;
  if ( *wlr_ft && 
      strcmp(interface,zwlr_foreign_toplevel_manager_v1_interface.name)==0)
  {
    toplevel_manager = wl_registry_bind(registry, name,
      &zwlr_foreign_toplevel_manager_v1_interface,
      WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION);

    zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
      &toplevel_manager_impl, data);
  } 
  else if (strcmp(interface,zxdg_output_manager_v1_interface.name)==0)
  {
    xdg_output_manager = wl_registry_bind(registry, name,
        &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
  }
  else if (strcmp(interface, wl_seat_interface.name) == 0 && seat == NULL)
    seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
};

void wayland_init ( GtkWindow *window, gboolean wlr_ft )
{
  GdkDisplay *gdisp;
  struct wl_display *wdisp;
  struct wl_registry *registry;

  gdisp = gdk_screen_get_display(gtk_window_get_screen(window));
  wdisp = gdk_wayland_display_get_wl_display(gdisp);
  if(wdisp == NULL)
    g_error("Can't get wayland display\n");

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, &wlr_ft);
  wl_display_roundtrip(wdisp);

  if (wlr_ft && toplevel_manager == NULL)
    g_error("wlr-foreign-toplevel not available\n");

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
