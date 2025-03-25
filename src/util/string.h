#ifndef __SFWBAR_STRING_H__
#define __SFWBAR_STRING_H__

#include <glib.h>

guint str_nhash ( gchar *str );
gboolean str_nequal ( gchar *str1, gchar *str2 );
gchar *str_replace ( gchar *str, gchar *old, gchar *new );
gchar *str_escape ( gchar *string );
void *ptr_pass ( void *ptr );
int md5_file( gchar *path, guchar output[16] );
gchar *numeric_to_string ( double num, gint dec );
gboolean pattern_match ( gchar **dict, gchar *string );
gboolean regex_match_list ( GList *dict, gchar *string );
void regex_list_add ( GList **list, gchar *pattern );

#endif
