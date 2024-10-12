/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include "wintree.h"
#include "wayland.h"
#include "gui/monitor.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

#define FOREIGN_TOPLEVEL_VERSION 3

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

static gchar *toplevel_output_name_get ( struct wl_output *output )
{
  GdkDisplay *disp;
  gint i;

  disp = gdk_display_get_default();

  for(i=0; i<gdk_display_get_n_monitors(disp); i++)
    if(output == gdk_wayland_monitor_get_wl_output(
          gdk_display_get_monitor(disp, i)))
      return monitor_get_name(gdk_display_get_monitor(disp,i));
  return NULL;
}

static void toplevel_handle_output_leave(void *data, wlr_fth *tl,
    struct wl_output *output)
{
  char *name;
  window_t *win;
  GList *link;

  name = toplevel_output_name_get(output);
  if(!name)
    return;
  win = wintree_from_id(tl);
  if(!win)
    return;
  link = g_list_find_custom(win->outputs,name,(GCompareFunc)g_strcmp0);
  if(!link)
    return;
  g_free(link->data);
  win->outputs = g_list_delete_link(win->outputs,link);
  wintree_commit(win);
}

static void toplevel_handle_output_enter(void *data, wlr_fth *tl,
    struct wl_output *output)
{
  char *name;
  window_t *win;

  name = toplevel_output_name_get(output);
  if(!name)
    return;
  win = wintree_from_id(tl);
  if(!win)
    return;
  if(g_list_find_custom(win->outputs,name,(GCompareFunc)g_strcmp0))
    return;
  win->outputs = g_list_prepend(win->outputs,g_strdup(name));
  wintree_commit(win);
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

void foreign_toplevel_init ( void )
{
  if(wintree_api_check())
    return;

  if( !(toplevel_manager = wayland_iface_register(
          zwlr_foreign_toplevel_manager_v1_interface.name, 1,
          FOREIGN_TOPLEVEL_VERSION,
          &zwlr_foreign_toplevel_manager_v1_interface)) )
    return;

  zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
    &toplevel_manager_impl, NULL);

  wintree_api_register(&ft_wintree_api);
}
