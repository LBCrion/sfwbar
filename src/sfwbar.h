#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>

#if GLIB_MINOR_VERSION < 68
#define g_memdup2(x,y) g_memdup(x,y)
#endif

#define set_bit(var, mask, state) { var = state?var|(mask):var&(~mask); }

enum ipc_type {
  IPC_SWAY    = 1,
  IPC_HYPR    = 2,
  IPC_WAYFIRE = 3,
  IPC_WAYLAND = 4
};

void signal_subscribe ( void );

struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

void hypr_ipc_init ( void );
void wayfire_ipc_init ( void );
enum ipc_type ipc_get ( void );
void ipc_set ( enum ipc_type new );

#endif
