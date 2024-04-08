#ifndef __APPINFO_H__
#define __APPINFO_H__


void app_info_init ( void );
void app_icon_map_add ( gchar *appid, gchar *icon );
gchar *app_info_icon_get ( const gchar *app_id, GtkIconTheme *theme );
gchar *app_info_icon_lookup ( gchar *app_id, gboolean prefer_symbolic );

#endif
