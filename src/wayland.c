#include "sfwbar.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#define WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION 3

static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;
static struct wl_output *pref_output = NULL;
static uint32_t pref_output_id = UINT32_MAX;
struct wl_seat *seat = NULL;

GList *wlr_ft_find_win ( GList *list, wlr_fth *tl )
{
  GList *l;
  for(l=list;l!=NULL;l=g_list_next(l))
    if(l->data != NULL)
      if(AS_WINDOW(l->data)->wlr == tl)
        return l;
  return NULL;
}

static void toplevel_handle_app_id(void *data, wlr_fth *tl, const gchar *app_id)
{
  struct wt_window *win = NULL;
  GList *l;
  l = wlr_ft_find_win(context->wt_list,tl);
  if(l==NULL)
    return;
  win = l->data;
  str_assign(&(win->appid), (gchar *)app_id);
  context->wt_dirty = 1;
}

static void toplevel_handle_title(void *data, wlr_fth *tl, const gchar *title)
{
  struct wt_window *win = NULL;
  GList *l;
  l = wlr_ft_find_win(context->wt_list,tl);
  if(l==NULL)
    return;
  win = l->data;
  str_assign(&(win->title), (gchar *)title);
  context->wt_dirty = 1;
}

static void toplevel_handle_closed(void *data, wlr_fth *tl)
{
  struct wt_window *win = NULL;
  GList *l;
  l = wlr_ft_find_win(context->wt_list,tl);
  if(l==NULL)
    return;
  win = l->data;
  if(context->features & F_TASKBAR)
  {
    gtk_widget_destroy(win->button);
    g_object_unref(G_OBJECT(win->button));
  }
  if(context->features & F_SWITCHER)
  {
    gtk_widget_destroy(win->switcher);
    g_object_unref(G_OBJECT(win->switcher));
  }
  str_assign(&(win->appid),NULL);
  str_assign(&(win->title),NULL);
  context->wt_list = g_list_delete_link(context->wt_list,l);
  zwlr_foreign_toplevel_handle_v1_destroy(tl);
}

static void toplevel_handle_done(void *data, wlr_fth *tl)
{
  struct wt_window *win = NULL;
  GList *l;
  l = wlr_ft_find_win(context->wt_list,tl);
  if(l==NULL)
    return;
  win = l->data;
  if(win==NULL)
    return;
  if(win->title == NULL)
    str_assign(&(win->title), win->appid);
  if(win->button!=NULL)
  {
    if(context->features & F_TB_LABEL)
      gtk_label_set_text(GTK_LABEL(win->label),win->title);
    return;
  }
  wintree_window_append(win);
}

static void toplevel_handle_state(void *data, wlr_fth *tl,
                struct wl_array *state)
{
  uint32_t *entry;
  wl_array_for_each(entry, state)
    if(*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
      return;
  wl_array_for_each(entry, state)
    if(*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
    {
      struct wt_window *win = NULL;
      GList *l;
      l = wlr_ft_find_win(context->wt_list,tl);
      if(l!=NULL)
      {
        win = l->data;
        context->tb_focus = win->wid;
        context->wt_dirty = 1;
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
  win->wlr = tl;
  win->wid = context->wt_counter++;
  context->wt_list = g_list_append (context->wt_list,win);

  zwlr_foreign_toplevel_handle_v1_add_listener(tl, &toplevel_impl, context);
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

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  if (strcmp(interface, wl_output_interface.name) == 0) 
  {
    if (name == pref_output_id)
      pref_output = wl_registry_bind(registry,name,&wl_output_interface, version);
  } else if (strcmp(interface,zwlr_foreign_toplevel_manager_v1_interface.name)==0)
  {
    toplevel_manager = wl_registry_bind(registry, name,
      &zwlr_foreign_toplevel_manager_v1_interface,
      WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION);

    zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
      &toplevel_manager_impl, data);
  } else if (strcmp(interface, wl_seat_interface.name) == 0 && seat == NULL)
    seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
}

static void handle_global_remove(void *data, struct wl_registry *registry,
  uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove,
};

void wlr_ft_init ( void )
{
  GdkDisplay *gdisp;
  struct wl_display *wdisp;
  struct wl_registry *registry;

  gdisp = gdk_screen_get_display(gtk_window_get_screen(GTK_WINDOW(context->window)));
  wdisp = gdk_wayland_display_get_wl_display(gdisp);
  if(wdisp == NULL)
  {
    fprintf(stderr, "can't get wayland display\n");
    return;
  }

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, context);
  wl_display_roundtrip(wdisp);

  if (toplevel_manager == NULL)
  {
    fprintf(stderr, "wlr-foreign-toplevel not available\n");
      return;
  }
  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
