/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib.h>

guint str_nhash ( gchar *str )
{
  guint i, ret=5381;

  for(i=0; str[i]; i++)
    ret += g_ascii_toupper(str[i]);

  return ret;
}

gboolean str_nequal ( gchar *str1, gchar *str2 )
{
  return (!g_ascii_strcasecmp(str1, str2));
}

gchar *str_replace ( gchar *str, gchar *old, gchar *new )
{
  gssize olen, nlen;
  gint n;
  gchar *ptr, *pptr, *dptr, *dest;

  if(!str || !old || !new)
    return g_strdup(str);

  olen = strlen(old);
  nlen = strlen(new);

  n=0;
  ptr = strstr(str, old);
  while(ptr)
  {
    n++;
    ptr = strstr(ptr+olen, old);
  }

  if(!n)
    return g_strdup(str);

  dest = g_malloc(strlen(str) + n*(nlen-olen)+1);

  pptr = str;
  ptr = strstr(str, old);
  dptr = dest;
  while(ptr)
  {
    strncpy(dptr, pptr,ptr-pptr);
    dptr += ptr - pptr;
    strcpy(dptr, new);
    dptr += nlen;
    pptr = ptr + olen;
    ptr = strstr(pptr, old);
  }
  strcpy(dptr, pptr);

  return dest;
}

void *ptr_pass ( void *ptr )
{
  return ptr;
}

gboolean pattern_match ( gchar **dict, gchar *string )
{
  gint i;

  if(!dict)
    return FALSE;

  for(i=0; dict[i]; i++)
    if(g_pattern_match_simple(dict[i], string))
      return TRUE;

  return FALSE;
}

gboolean regex_match_list ( GList *dict, gchar *string )
{
  GList *iter;

  for(iter=dict; iter; iter=g_list_next(iter))
    if(g_regex_match(iter->data, string, 0, NULL))
      return TRUE;

  return FALSE;
}
