/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "ext-image-capture-source-v1.h"
#include "ext-image-copy-capture-v1.h"
#include "trigger.h"
#include "wayland.h"
#include "wintree.h"
#include "gui/capture.h"
#include "gui/monitor.h"
#include "gui/scaleimage.h"
#include "util/string.h"

#define EXT_IMAGE_CAPTURE_SOURCE_VERSION 1
#define EXT_IMAGE_COPY_CAPTURE_VERSION 1

static struct ext_foreign_toplevel_image_capture_source_manager_v1
  *capture_toplevel_source;
static struct ext_output_image_capture_source_manager_v1
  *capture_output_source;
static struct ext_image_copy_capture_manager_v1 *capture_manager;
static gint capture_counter;

typedef struct _capture_node {
  struct ext_image_copy_capture_session_v1 *session;
  struct ext_image_copy_capture_frame_v1 *frame;
  struct ext_image_capture_source_v1 *source;
  guint32 width, height, shm_format;
  wayland_buffer_t *buff;
  gpointer data;
  void (*callback)(gpointer, gchar *);
} capture_node_t;

static void capture_node_free ( capture_node_t *node )
{
  if(node->buff)
    wayland_buffer_free(node->buff);
  if(node->frame)
    ext_image_copy_capture_frame_v1_destroy(node->frame);
  if(node->session)
    ext_image_copy_capture_session_v1_destroy(node->session);
  if(node->source)
    ext_image_capture_source_v1_destroy(node->source);
  g_free(node);
}

static void capture_frame_transform ( void *data,
    struct ext_image_copy_capture_frame_v1 *frame, uint32_t transform)
{
}

static void capture_frame_damage ( void *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    int32_t x, int32_t y, int32_t width, int32_t height)
{
}

static void capture_frame_presentation_time ( void *data,
    struct ext_image_copy_capture_frame_v1 *frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
}

static void capture_frame_ready ( void *data,
    struct ext_image_copy_capture_frame_v1 *frame )
{
  capture_node_t *node = data;
  cairo_surface_t *cs;
  gchar *name;

  cs = cairo_image_surface_create_for_data((guchar *)node->buff->data,
      CAIRO_FORMAT_ARGB32, node->width, node->height,
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, node->width));
  if(cairo_surface_status(cs) == CAIRO_STATUS_SUCCESS)
  {
    name = g_strdup_printf("<pixbufcache/>capture-%u", capture_counter++);
    scale_image_cache_insert(name,
        gdk_pixbuf_get_from_surface(cs, 0, 0, node->width, node->height));
    node->callback(node->data, name);
  }
  cairo_surface_destroy(cs);

  capture_node_free(node);
}

static void capture_frame_failed ( void *data,
    struct ext_image_copy_capture_frame_v1 *frame, uint32_t reason)
{
  capture_node_free(data);
}

static struct ext_image_copy_capture_frame_v1_listener capture_frame_listener = {
  .transform = capture_frame_transform,
  .damage = capture_frame_damage,
  .presentation_time = capture_frame_presentation_time,
  .ready = capture_frame_ready,
  .failed = capture_frame_failed,
};

static void capture_session_buffer_size_handle ( void *data,
    struct ext_image_copy_capture_session_v1 *session,
    uint32_t width, uint32_t height)
{
  capture_node_t *node = data;
  node->width = width;
  node->height = height;
}

static void capture_session_shm_format_handle ( void *data,
    struct ext_image_copy_capture_session_v1 *session, uint32_t format )
{
  capture_node_t *node = data;

  if(format == WL_SHM_FORMAT_ARGB8888)
    node->shm_format = format;
}

static void capture_session_dmabuf_device_handle ( void *data,
    struct ext_image_copy_capture_session_v1 *session, struct wl_array *dev )
{
}

static void capture_session_dmabuf_format_handle ( void *data,
    struct ext_image_copy_capture_session_v1 *session, uint32_t format,
    struct wl_array *modifiers)
{
}

static void capture_session_done ( void *data,
    struct ext_image_copy_capture_session_v1 *session )
{
  capture_node_t *node = data;

  if(node->shm_format != WL_SHM_FORMAT_ARGB8888 ||
    !(node->buff = wayland_buffer_new(node->width, node->height,
        node->shm_format)) )
  {
    capture_node_free(data);
    return;
  }
  node->frame = ext_image_copy_capture_session_v1_create_frame(session);
  ext_image_copy_capture_frame_v1_add_listener(node->frame,
      &capture_frame_listener, node);
  ext_image_copy_capture_frame_v1_attach_buffer(node->frame,
      node->buff->buffer);
  ext_image_copy_capture_frame_v1_damage_buffer(node->frame, 0, 0,
      node->width, node->height);
  ext_image_copy_capture_frame_v1_capture(node->frame);
}

static void capture_session_stopped ( void *data,
    struct ext_image_copy_capture_session_v1 *session )
{
  capture_node_free(data);
}

static struct ext_image_copy_capture_session_v1_listener capture_session_listener = {
  .buffer_size = capture_session_buffer_size_handle,
  .shm_format = capture_session_shm_format_handle,
  .dmabuf_device = capture_session_dmabuf_device_handle,
  .dmabuf_format = capture_session_dmabuf_format_handle,
  .done = capture_session_done,
  .stopped = capture_session_stopped,
};

static void capture_from_source ( struct ext_image_capture_source_v1 *source,
   void (*callback)(gpointer, gchar *), gpointer data )
{
  capture_node_t *node;

  g_return_if_fail(source);
  node = g_malloc0(sizeof(capture_node_t));
  node->data = data;
  node->callback = callback;
  node->source = source;
  node->session = ext_image_copy_capture_manager_v1_create_session(
      capture_manager, source, 0);
  ext_image_copy_capture_session_v1_add_listener(node->session,
      &capture_session_listener, node);
}

static void capture_output_cb ( gpointer data, gchar *name )
{
  trigger_emit_with_string("screenshot", "image", name);
}

void capture_output ( gchar *name )
{
  struct wl_output *output;
  GdkMonitor *mon;

  if(!capture_support_check(CAPTURE_TYPE_OUTPUT)) 
    return;

  if( !(mon = monitor_from_name(name)) )
    return;

  if( !(output = gdk_wayland_monitor_get_wl_output(mon)) )
    return;
  capture_from_source(
      ext_output_image_capture_source_manager_v1_create_source(
        capture_output_source, output), capture_output_cb, NULL);
}

static void capture_toplevel_cb ( gpointer data, gchar *name )
{
  window_t *win;

  if( !(win = wintree_from_id(data)) )
    return;

  scale_image_cache_remove(win->image);
  str_assign(&win->image, name);
  wintree_commit(win);
}

void capture_window ( window_t *win )
{
  gpointer toplevel;

  if(!capture_support_check(CAPTURE_TYPE_WINDOW)) 
    return;
  if( (toplevel = ext_ftl_lookup(win->stable_id)) )
    capture_from_source(
        ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
          capture_toplevel_source, toplevel), capture_toplevel_cb, win->uid);
}

void capture_window_image_set ( GtkWidget *image, window_t *win,
    gboolean preview )
{
  gchar *ptr, *tmp;

  if(capture_support_check(CAPTURE_TYPE_WINDOW) &&
      win->stable_id && !win->image)
    capture_window(win);

  if(win->image && scale_image_set_image(image, win->image, NULL))
    return;

  if(!win->appid || !*(win->appid))
  {
    scale_image_set_image(image, wintree_appid_map_lookup(win->title), NULL);
    return;
  }

  if(scale_image_set_image(image, win->appid, NULL))
    return;
  if( (ptr = strrchr(win->appid, '.')) &&
      scale_image_set_image(image, ptr+1, NULL))
    return;
  if( (ptr = strchr(win->appid, ' ')) )
  {
    tmp = g_strndup(win->appid, ptr - win->appid);
    scale_image_set_image(image, tmp, NULL);
    g_free(tmp);
  }
}

gboolean capture_support_check ( capture_type_t type )
{
  if(type == CAPTURE_TYPE_OUTPUT)
    return !!capture_manager && !!capture_output_source;
  if(type == CAPTURE_TYPE_WINDOW)
    return !!capture_manager && !!capture_toplevel_source;
  return FALSE;
}

static value_t capture_screenshot ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "Screenshot");
  if(np)
    vm_param_check_string(vm, p, 0, "Screenshot");

  capture_output(monitor_get_name(monitor_from_widget(
          vm_widget_get(vm, np? value_get_string(p[0]) : NULL))));

  return value_na;
}

static void capture_window_new_handle ( window_t *win, gpointer d )
{
  capture_window(win);
}

static window_listener_t capture_window_listener = {
  .window_new = capture_window_new_handle,
};

void capture_init ( void )
{
  capture_toplevel_source = wayland_iface_register(
      ext_foreign_toplevel_image_capture_source_manager_v1_interface.name,
      EXT_IMAGE_CAPTURE_SOURCE_VERSION, EXT_IMAGE_CAPTURE_SOURCE_VERSION,
      &ext_foreign_toplevel_image_capture_source_manager_v1_interface);
  capture_output_source = wayland_iface_register(
      ext_output_image_capture_source_manager_v1_interface.name,
      EXT_IMAGE_CAPTURE_SOURCE_VERSION, EXT_IMAGE_CAPTURE_SOURCE_VERSION,
      &ext_output_image_capture_source_manager_v1_interface);
  capture_manager = wayland_iface_register(
      ext_image_copy_capture_manager_v1_interface.name,
      EXT_IMAGE_COPY_CAPTURE_VERSION, EXT_IMAGE_COPY_CAPTURE_VERSION,
      &ext_image_copy_capture_manager_v1_interface);
  if(capture_support_check(CAPTURE_TYPE_OUTPUT))
    vm_func_add("Screenshot", capture_screenshot, TRUE, FALSE);
  if(capture_support_check(CAPTURE_TYPE_WINDOW))
    wintree_listener_register(&capture_window_listener, NULL);
}
