#include "sfwbar.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "xdg-output-unstable-v1.h"
#include "idle-inhibit-unstable-v1.h"

#define WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION 3

typedef struct zwlr_foreign_toplevel_handle_v1 wlr_fth;

static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;
static struct zxdg_output_manager_v1 *xdg_output_manager;
static struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
static gint64 wt_counter;
static struct wl_seat *seat;

static void toplevel_handle_app_id(void *data, wlr_fth *tl, const gchar *app_id)
{
  struct wt_window *win;

  if(sway_ipc_active())
    return;

  win = wintree_from_id(tl);
  if(!win)
    return;
  str_assign(&(win->appid), (gchar *)app_id);
  taskbar_invalidate_all();
  switcher_invalidate();
}

static void toplevel_handle_title(void *data, wlr_fth *tl, const gchar *title)
{
  struct wt_window *win;

  if(sway_ipc_active())
    return;

  win = wintree_from_id(tl);
  if(!win)
    return;
  str_assign(&(win->title), (gchar *)title);
  wintree_set_active(win->title);
  taskbar_set_label_for_all(win,win->title);
  switcher_set_label(win,win->title);
  taskbar_invalidate_all();
  switcher_invalidate();
}

static void toplevel_handle_closed(void *data, wlr_fth *tl)
{
  if(!sway_ipc_active())
    wintree_window_delete(tl);
  zwlr_foreign_toplevel_handle_v1_destroy(tl);
}

static void toplevel_handle_done(void *data, wlr_fth *tl)
{
  struct wt_window *win;

  if(sway_ipc_active())
    return;

  win = wintree_from_id(tl);
  if(!win)
    return;
  if(win->title == NULL)
    str_assign(&(win->title), win->appid);
  wintree_set_active(win->title);
  if(win->valid)
  {
    taskbar_set_label_for_all(win,win->title);
    switcher_set_label(win,win->title);
  }
  else
    wintree_window_append(win);
}

static void toplevel_handle_state(void *data, wlr_fth *tl,
                struct wl_array *state)
{
  uint32_t *entry;
  struct wt_window *win;

  if(sway_ipc_active())
    return;

  win = wintree_from_id(tl);
  if(!win)
    return;

  win->state = 0;

  wl_array_for_each(entry, state)
    switch(*entry)
    {
    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
      win->state |= WS_MINIMIZED;
      break;
    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
      win->state |= WS_MAXIMIZED;
      break;
    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
      win->state |= WS_FULLSCREEN;
      break;
    case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
      wintree_set_active(win->title);
      wintree_set_focus(win->uid);
      taskbar_invalidate_all();
      break;
    }
}

static void toplevel_handle_parent(void *data, wlr_fth *tl, wlr_fth *pt) {}
static void toplevel_handle_output_leave(void *data, wlr_fth *toplevel, struct wl_output *output) 
{
  if(sway_ipc_active())
    sway_ipc_rescan();
}

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

  if(!sway_ipc_active())
  {
    win = wintree_window_init();
    win->uid = tl;
    win->wid = wt_counter++;
    wintree_window_append(win);
  }

  zwlr_foreign_toplevel_handle_v1_add_listener(tl, &toplevel_impl, NULL);
}

void foreign_toplevel_activate ( gpointer tl )
{
  if(sway_ipc_active())
    return;

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
  GdkMonitor *monitor = data;

  g_free(g_object_get_data(G_OBJECT(monitor),"xdg_name"));
  g_object_set_data(G_OBJECT(monitor),"xdg_name",strdup(name));
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = noop,
  .logical_size = noop,
  .done = noop,
  .name = xdg_output_handle_name,
  .description = noop,
};

void wayland_set_idle_inhibitor ( GtkWidget *widget, gboolean inhibit )
{
  struct wl_surface *surface;
  struct zwp_idle_inhibitor_v1 *inhibitor;

  if(!idle_inhibit_manager)
    return;

  surface = gdk_wayland_window_get_wl_surface(
      gtk_widget_get_window(widget));
  inhibitor = g_object_get_data(G_OBJECT(widget),"inhibitor");

  if(!surface)
    return;

  if(inhibit && !inhibitor)
  {
    inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        idle_inhibit_manager, surface );
    g_object_set_data(G_OBJECT(widget),"inhibitor",inhibitor);
  }
  else if( !inhibit && inhibitor )
  {
    g_object_set_data(G_OBJECT(widget),"inhibitor",NULL);
    zwp_idle_inhibitor_v1_destroy(inhibitor);
  }
}

void wayland_reset_inhibitors ( GtkWidget *w, gpointer data )
{
  if(!idle_inhibit_manager)
    return;

  if(GTK_IS_CONTAINER(w))
    gtk_container_foreach(GTK_CONTAINER(w),wayland_reset_inhibitors,data);

  if(g_object_get_data(G_OBJECT(w),"inhibitor"))
  {
    wayland_set_idle_inhibitor(w,FALSE);
    wayland_set_idle_inhibitor(w,TRUE);
  }

}

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  if (!g_strcmp0(interface,zwlr_foreign_toplevel_manager_v1_interface.name))
  {
    toplevel_manager = wl_registry_bind(registry, name,
      &zwlr_foreign_toplevel_manager_v1_interface,
      WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION);

    zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
      &toplevel_manager_impl, data);
  } 
  else if (!g_strcmp0(interface,zwp_idle_inhibit_manager_v1_interface.name))
  {
    idle_inhibit_manager = wl_registry_bind(registry,name,
        &zwp_idle_inhibit_manager_v1_interface,1);
  }
  else if (!g_strcmp0(interface,zxdg_output_manager_v1_interface.name))
  {
    xdg_output_manager = wl_registry_bind(registry, name,
        &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
  }
  else if (!g_strcmp0(interface, wl_seat_interface.name) && !seat)
    seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
};

void wayland_output_new ( GdkMonitor *gmon )
{
  struct wl_output *output;
  struct zxdg_output_v1 *xdg;

  if(!gmon)
    return;

  output = gdk_wayland_monitor_get_wl_output(gmon);

  if(!output)
    return;

  xdg = zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output);

  if(xdg)
  {
    zxdg_output_v1_add_listener(xdg,&xdg_output_listener,gmon);
    g_object_set_data(G_OBJECT(gmon),"xdg_output",xdg);
  }
}

void wayland_output_destroy ( GdkMonitor *gmon )
{
  struct zxdg_output_v1 *xdg;

  if(!gmon)
    return;

  xdg = g_object_get_data(G_OBJECT(gmon),"xdg_output");

  if(xdg)
    zxdg_output_v1_destroy(xdg);
}

void wayland_init ( void )
{
  GdkDisplay *gdisp;
  struct wl_display *wdisp;
  struct wl_registry *registry;
  gint nmon,i;

  gdisp = gdk_display_get_default();
  wdisp = gdk_wayland_display_get_wl_display(gdisp);
  if(wdisp == NULL)
    g_error("Can't get wayland display\n");

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);

  if (toplevel_manager == NULL)
    g_error("wlr-foreign-toplevel not available\n");

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);

  if(!xdg_output_manager)
    return;

  nmon = gdk_display_get_n_monitors(gdisp);
  for(i=0;i<nmon;i++)
    wayland_output_new(gdk_display_get_monitor(gdisp,i));
  wl_display_roundtrip(wdisp);
}
