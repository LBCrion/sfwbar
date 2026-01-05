
#include "vm/value.h"
#include "util/string.h"

void value_free ( value_t v1 )
{
  if(value_is_string(v1))
    g_free(v1.value.string);
  else if(value_is_array(v1))
    g_array_unref(v1.value.array);
}

void value_free_ptr ( value_t *v1 )
{
  if(v1)
    value_free(*v1);
}

value_t value_array_concat ( value_t v1, value_t v2 )
{
  value_t result, new;
  gpointer data;
  gsize len;

  if(!value_is_array(v1) && !value_is_array(v1))
    return value_na;

  if(!value_is_array(v1))
  {
    new = value_dup(v1);
    result = value_dup(v2);
    g_array_prepend_vals(result.value.array, &new, 1);
  }
  else if(!value_is_array(v2))
  {
    new = value_dup(v2);
    result = value_dup(v1);
    g_array_append_vals(result.value.array, &new, 1);
  }
  else
  {
    result = value_array_create(v1.value.array->len + v2.value.array->len);
    data = g_array_steal(v1.value.array, &len);
    g_array_append_vals(result.value.array, data, len);
    data = g_array_steal(v2.value.array, &len);
    g_array_append_vals(result.value.array, data, len);
  }

  return result;
}

value_t value_array_create ( gint size )
{
  GArray *array;

  array = g_array_sized_new(FALSE, FALSE, sizeof(value_t), size);
  g_array_set_clear_func(array, (GDestroyNotify)value_free_ptr);

  return value_new_array(array);
}

void value_array_append ( value_t array, value_t v )
{
  g_array_append_val(array.value.array, v);
}

value_t value_dup_array ( value_t v1 )
{
  value_t array;
  gsize i;

  array = value_array_create(v1.value.array->len);

  for(i=0; i<v1.value.array->len; i++)
    value_array_append(array,
        value_dup(g_array_index(v1.value.array, value_t, i)));
  return array;
}

value_t value_dup ( value_t v1 )
{
  if(value_is_string(v1))
    return value_new_string(g_strdup(v1.value.string));
  if(value_is_array(v1))
    return value_dup_array(v1);
  return v1;
}

gboolean value_compare ( value_t v1, value_t v2 )
{
  gint i;

  if(v1.type != v2.type)
    return FALSE;
  if(value_is_numeric(v1))
    return v1.value.numeric == v2.value.numeric;
  if(value_is_string(v1))
    return !g_strcmp0(value_get_string(v1), value_get_string(v2));
  if(value_is_array(v1))
  {
    if(v1.value.array->len != v2.value.array->len)
      return FALSE;
    for(i=0; i<v1.value.array->len; i++)
      if(!value_compare(g_array_index(v1.value.array, value_t, i),
            g_array_index(v2.value.array, value_t, i)))
        return FALSE;
    return TRUE;
  }
  return FALSE;
}

gchar *value_array_to_string ( value_t value )
{
  GString *result;
  GArray *array;
  value_t element;
  gchar *tmp;
  gsize i;

  if(!value_is_array(value))
    return NULL;

  array = value.value.array;
  result = g_string_new("[");
  for(i=0; i<array->len; i++)
  {
    element = g_array_index(array, value_t, i);
    if(value_is_string(element))
      g_string_append(result, value_get_string(element));
    if(value_is_array(element))
    {
      tmp = value_array_to_string(element);
      g_string_append(result, tmp);
      g_free(tmp);
    }
    if(value_is_numeric(element))
      g_string_append_printf(result, "%lf", value_get_numeric(element));
    if(i != array->len-1)
      g_string_append(result, ", ");
  }
  g_string_append(result, "]");

  return g_string_free(result, FALSE);
}

gchar *value_to_string ( value_t v1, gint prec )
{
  if(value_is_array(v1))
    return value_array_to_string(v1);
  if(value_is_numeric(v1))
    return numeric_to_string(value_get_numeric(v1), prec);
  if(value_is_string(v1))
    return g_strdup(value_get_string(v1));
  return g_strdup("n/a");
}

value_t value_from_string ( gchar *str, gint type )
{
  value_t array;

  if(type == EXPR_TYPE_STRING)
    return value_new_string(g_strdup(str));
  if(type == EXPR_TYPE_NUMERIC)
    return value_new_numeric(str? g_ascii_strtod(str, NULL) : 0);
  if(type == EXPR_TYPE_ARRAY)
  {
    array = value_array_create(1);
    value_array_append(array, value_new_string(g_strdup(str)));
    return array;
  }

  return value_na;
}

value_t value_array_from_strv ( gchar **strv )
{
  value_t array;
  gint i, l;

  if( !strv )
    return value_na;
  l = g_strv_length(strv);
  array = value_array_create(l);

  for(i=0; i<l; i++)
    value_array_append(array, value_new_string(g_strdup(strv[i])));

  return array;
}
