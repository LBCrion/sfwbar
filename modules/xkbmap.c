/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include <xkbcommon/xkbregistry.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include "input.h"
#include "module.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
static struct rxkb_context *ctx;

static void handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size)
{
  gchar *xkb_string;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  xkb_layout_index_t maxl, l;

  if(format==WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
  {
    xkb_string = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap = xkb_keymap_new_from_string(xkb_context, xkb_string, format,
      XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkb_state = xkb_state_new(xkb_keymap);
    maxl = xkb_keymap_num_layouts(xkb_keymap);
    for(l=0; l<maxl; l++)
      if(xkb_state_layout_index_is_active(xkb_state, l, XKB_STATE_LAYOUT_EFFECTIVE))
        input_layout_set(xkb_keymap_layout_get_name(xkb_keymap, l));
    xkb_state_unref(xkb_state);
    xkb_keymap_unref(xkb_keymap);
    xkb_context_unref(xkb_context);
  }
}

static void handle_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface,
		struct wl_array *keys)
{
}

static void handle_leave ( void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface )
{
}

static void handle_key ( void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t _state )
{
}

static void handle_modifiers ( void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group )
{
}

static void handle_repeat_info ( void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay )
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = handle_keymap,
	.enter = handle_enter,
	.leave = handle_leave,
	.key = handle_key,
	.modifiers = handle_modifiers,
	.repeat_info = handle_repeat_info,
};

void xkb_lang_init ( void )
{
}
static const gchar *xkb_get_value ( struct rxkb_layout *layout, gchar *type )
{
  if(!g_ascii_strcasecmp(type,"description"))
    return rxkb_layout_get_description(layout);
  if(!g_ascii_strcasecmp(type,"name"))
    return rxkb_layout_get_name(layout);
  if(!g_ascii_strcasecmp(type,"variant"))
    return rxkb_layout_get_variant(layout);
  if(!g_ascii_strcasecmp(type,"brief"))
    return rxkb_layout_get_brief(layout);
  return g_strdup_printf("XkbMap: Invalid type: %s",type);
}

static value_t xkb_map_expr_func ( vm_t *vm, value_t p[], gint np )
{
  struct rxkb_layout *layout;

  if(np!=3 || !value_is_string(p[0]) || !value_is_string(p[1]) ||
      !value_is_string(p[2]))
    return value_new_string(g_strdup("XkbMap: missing parameter(s)"));

  for(layout=rxkb_layout_first(ctx); layout; layout=rxkb_layout_next(layout))
  {
    if(!g_strcmp0(value_get_string(p[0]),
          xkb_get_value(layout, value_get_string(p[1]))))
      return value_new_string(g_strdup(
            xkb_get_value(layout, value_get_string(p[2]))));
  }

  return value_new_string(g_strdup_printf("XkbMap: Unknown layout: %s",
      value_get_string(p[0])));
}

gboolean sfwbar_module_init ( void )
{
  if( !(ctx = rxkb_context_new(0)) )
    return FALSE;
//  rxkb_context_include_path_append_default(ctx);
  rxkb_context_parse_default_ruleset(ctx);
  vm_func_add("xkbmap", xkb_map_expr_func, TRUE, TRUE);

  wl_keyboard_add_listener(wl_seat_get_keyboard(gdk_wayland_seat_get_wl_seat(
      gdk_display_get_default_seat(gdk_display_get_default()))),
      &keyboard_listener, NULL);
  return TRUE;
}
