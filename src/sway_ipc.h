#ifndef __SWAY_IPC_H__
#define __SWAY_IPC_H__

void sway_ipc_init ( void );
GdkRectangle sway_ipc_parse_rect ( struct json_object *obj );
gboolean sway_ipc_active ( void );
json_object *sway_ipc_poll ( gint sock, gint32 *etype );
int sway_ipc_open (int to);
int sway_ipc_send ( gint sock, gint32 type, gchar *command );
void sway_ipc_command ( gchar *cmd, ... );
int sway_ipc_subscribe ( gint sock );
gboolean sway_ipc_event ( GIOChannel *, GIOCondition , gpointer );
void sway_ipc_bar_id ( gchar *id );
void sway_ipc_client_init ( ScanFile *file );
void sway_ipc_pager_populate ( void );

#endif
