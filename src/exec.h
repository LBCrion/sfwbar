#ifndef __EXEC_H__
#define __EXEC_H__

#include <gio/gdesktopappinfo.h>

void exec_cmd ( const gchar *cmd );
void exec_cmd_in_term ( const gchar *cmd );
void exec_launch ( GDesktopAppInfo *info );
void exec_api_set ( void (*)( const gchar *) );

#endif
