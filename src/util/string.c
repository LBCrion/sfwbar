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

void str_assign ( gchar **old, gchar *new )
{
  gchar *tmp = g_atomic_pointer_get(old);
  g_atomic_pointer_set(old, new);
  g_free(tmp);
}

gint64 str_ascii_toll ( const gchar *str, gchar **end, guint base )
{
  if(str)
    return g_ascii_strtoll(str, end, base);
  *end = NULL;
  return 0;
}

guint64 str_ascii_toull ( const gchar *str, gchar **end, guint base )
{
  if(str)
    return g_ascii_strtoull(str, end, base);
  *end = NULL;
  return 0;
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
  for(ptr = strstr(str, old); ptr; ptr = strstr(ptr+olen, old))
    n++;

  if(!n)
    return g_strdup(str);

  dest = g_malloc(strlen(str) + n*(nlen-olen)+1);

  pptr = str;
  ptr = strstr(str, old);
  dptr = dest;
  while(ptr)
  {
    strncpy(dptr, pptr, ptr-pptr);
    dptr += ptr - pptr;
    strcpy(dptr, new);
    dptr += nlen;
    pptr = ptr + olen;
    ptr = strstr(pptr, old);
  }
  strcpy(dptr, pptr);

  return dest;
}

gchar *str_escape ( gchar *string )
{
  gint i,j=0,l=0;
  gchar *result;

  for(i=0; string[i]; i++)
    if(string[i] == '"' || string[i] == '\\')
      j++;
  result = g_malloc(i+j+3);

  result[l++]='"';
  for(i=0;string[i];i++)
  {
    if(string[i] == '"' || string[i] == '\\')
      result[l++]='\\';
    result[l++] = string[i];
  }
  result[l++]='"';
  result[l]=0;

  return result;
}

void *ptr_pass ( void *ptr )
{
  return ptr;
}

gint strv_index ( gchar **strv, gchar *key )
{
  gint i;

  if(strv)
    for(i=0; i<g_strv_length(strv); i++)
      if(!g_strcmp0(strv[i], key))
        return i;

  return -1;
}

gchar *numeric_to_string ( double num, gint dec )
{
  static const gchar *format = "%%0.%df";
  gchar fbuf[16];
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  if(dec<0)
    return g_strdup(g_ascii_dtostr(buf, G_ASCII_DTOSTR_BUF_SIZE, num));

  g_snprintf(fbuf , 16, format, MIN(dec, 99));
  return g_strdup(g_ascii_formatd(buf, G_ASCII_DTOSTR_BUF_SIZE, fbuf, num));
}

gboolean pattern_match ( gchar **dict, gchar *string )
{
  gint i;

  if(dict)
    for(i=0; dict[i]; i++)
      if(g_pattern_match_simple(dict[i], string))
        return TRUE;

  return FALSE;
}

void regex_list_add ( GList **list, gchar *pattern )
{
  GList *iter;
  GRegex *regex;

  for(iter=*list; iter; iter=g_list_next(iter))
    if(!g_strcmp0(pattern, g_regex_get_pattern(iter->data)))
      return;

  if( (regex = g_regex_new(pattern, 0, 0, NULL)) )
    *list = g_list_prepend(*list, regex);
}

gboolean regex_match_list ( GList *dict, gchar *string )
{
  GList *iter;

  for(iter=dict; iter; iter=g_list_next(iter))
    if(g_regex_match(iter->data, string, 0, NULL))
      return TRUE;

  return FALSE;
}
