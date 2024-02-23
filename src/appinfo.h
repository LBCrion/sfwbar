#ifndef __APPINFO_H__
#define __APPINFO_H__


void app_info_monitor_init ( void );
gchar *app_info_icon_get ( const gchar *app_id, GtkIconTheme *theme );
gchar *app_info_icon_lookup ( gchar *app_id, gboolean prefer_symbolic );

#endif
