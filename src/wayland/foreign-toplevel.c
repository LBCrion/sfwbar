/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include "../sfwbar.h"
#include "../sway_ipc.h"
#include "../wintree.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

typedef struct zwlr_foreign_toplevel_handle_v1 wlr_fth;
static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;

gboolean foreign_toplevel_is_active ( void )
{
  return (toplevel_manager!=NULL);
}

void foreign_toplevel_activate ( gpointer tl )
{
  zwlr_foreign_toplevel_handle_v1_unset_minimized(tl);
  zwlr_foreign_toplevel_handle_v1_activate(tl,gdk_wayland_seat_get_wl_seat(
        gdk_display_get_default_seat(gdk_display_get_default())));
}

static void toplevel_handle_app_id(void *data, wlr_fth *tl, const gchar *appid)
{
  wintree_set_app_id(tl,appid);
}

static void toplevel_handle_title(void *data, wlr_fth *tl, const gchar *title)
{
  wintree_set_title(tl,title);
}

static void toplevel_handle_closed(void *data, wlr_fth *tl)
{
  wintree_window_delete(tl);
  zwlr_foreign_toplevel_handle_v1_destroy(tl);
}

static void toplevel_handle_done(void *data, wlr_fth *tl)
{
  wintree_log(tl);
}

static void toplevel_handle_state(void *data, wlr_fth *tl,
                struct wl_array *state)
{
  uint32_t *entry;
  window_t *win;

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
      win->state |= WS_FOCUSED;
      wintree_set_focus(win->uid);
      break;
    }

  if(wintree_is_focused(win->uid) && !(win->state & WS_FOCUSED))
    wintree_set_focus(NULL);
  g_debug("foreign toplevel state for %p: %s%s%s%s", win->uid,
    win->state & WS_FOCUSED ? "Activated, " : "",
    win->state & WS_MINIMIZED ? "Minimized, " : "",
    win->state & WS_MAXIMIZED ? "Maximized, " : "",
    win->state & WS_FULLSCREEN ? "Fullscreen" : ""
    );
}

static void toplevel_handle_parent(void *data, wlr_fth *tl, wlr_fth *pt)
{
}

static void toplevel_handle_output_leave(void *data, wlr_fth *toplevel,
    struct wl_output *output)
{
}

static void toplevel_handle_output_enter(void *data, wlr_fth *toplevel,
    struct wl_output *output)
{
}

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
  window_t *win;

  win = wintree_window_init();
  win->uid = tl;
  wintree_window_append(win);

  zwlr_foreign_toplevel_handle_v1_add_listener(tl, &toplevel_impl, NULL);
}

static void toplevel_manager_handle_finished(void *data,
  struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager)
{
  zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener
  toplevel_manager_impl =
{
  .toplevel = toplevel_manager_handle_toplevel,
  .finished = toplevel_manager_handle_finished,
};

static void foreign_toplevel_focus ( gpointer id )
{
  zwlr_foreign_toplevel_handle_v1_unset_minimized(id);
  foreign_toplevel_activate(id);
}

static struct wintree_api ft_wintree_api = {
  .minimize = (void (*)(void *))
    zwlr_foreign_toplevel_handle_v1_set_minimized,
  .unminimize = (void (*)(void *))
    zwlr_foreign_toplevel_handle_v1_unset_minimized,
  .maximize = (void (*)(void *))
    zwlr_foreign_toplevel_handle_v1_set_maximized,
  .unmaximize = (void (*)(void *))
    zwlr_foreign_toplevel_handle_v1_unset_maximized,
  .close = (void (*)(void *))
    zwlr_foreign_toplevel_handle_v1_close,
  .focus = foreign_toplevel_focus
};

static void foreign_toplevel_register (struct wl_registry *registry,
    uint32_t name)
{
  if(ipc_get())
    return;

  toplevel_manager = wl_registry_bind(registry, name,
    &zwlr_foreign_toplevel_manager_v1_interface,3);

  zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
    &toplevel_manager_impl, NULL);

  wintree_api_register(&ft_wintree_api);
}

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  if (!g_strcmp0(interface,zwlr_foreign_toplevel_manager_v1_interface.name))
    foreign_toplevel_register(registry,name);
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
};

void wayland_ipc_init ( void )
{
  struct wl_display *wdisp;
  struct wl_registry *registry;

  wdisp = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  if(!wdisp)
    g_error("Can't get wayland display\n");

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
