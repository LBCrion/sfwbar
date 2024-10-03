#ifndef __APPINFO_H__
#define __APPINFO_H__

#include <gio/gdesktopappinfo.h>

typedef void (*AppInfoHandler)( const gchar * );

void app_info_init ( void );
void app_info_add_handlers ( AppInfoHandler add, AppInfoHandler del );
void app_info_remove_handlers ( AppInfoHandler add, AppInfoHandler del );
void app_icon_map_add ( gchar *appid, gchar *icon );
gchar *app_info_icon_lookup ( gchar *app_id, gboolean prefer_symbolic );

#endif
