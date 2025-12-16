#ifndef __SFWBAR_STRING_H__
#define __SFWBAR_STRING_H__

#include <glib.h>

#define str_get(x) g_atomic_pointer_get(&(x))

guint str_nhash ( gchar *str );
gint64 str_ascii_toll ( const gchar *str, gchar **end, guint base );
guint64 str_ascii_toull ( const gchar *str, gchar **end, guint base );
gboolean str_nequal ( gchar *str1, gchar *str2 );
void str_assign ( gchar **old, gchar *new );
gchar *str_replace ( gchar *str, gchar *old, gchar *new );
gchar *str_escape ( gchar *string );
void *ptr_pass ( void *ptr );
gint strv_index ( gchar **strv, gchar *key );
int md5_file( gchar *path, guchar output[16] );
gchar *numeric_to_string ( double num, gint dec );
gboolean pattern_match ( gchar **dict, gchar *string );
gboolean regex_match_list ( GList *dict, gchar *string );
void regex_list_add ( GList **list, gchar *pattern );

#endif
