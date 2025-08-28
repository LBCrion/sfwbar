#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <json.h>

#if GLIB_MINOR_VERSION < 68
#define g_memdup2(x,y) g_memdup(x,y)
#endif

void signal_subscribe ( void );
void action_lib_init ( void );
void hypr_ipc_init ( void );
void wayfire_ipc_init ( void );
void scanner_init ( void );

#endif
