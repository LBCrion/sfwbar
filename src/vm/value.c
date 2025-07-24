
#include "vm/value.h"

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
  GArray *result;
  value_t new;
  gpointer data;
  gsize len;

  if(!value_is_array(v1) && !value_is_array(v1))
    return value_na;

  if(!value_is_array(v1))
  {
    new = value_dup(v1);
    result = g_array_prepend_vals(g_array_ref(v2.value.array), &new, 1);
  }
  else if(!value_is_array(v2))
  {
    new = value_dup(v2);
    result = g_array_append_vals(g_array_ref(v1.value.array), &new, 1);
  }
  else
  {
    data = g_array_steal(v2.value.array, &len);
    result = g_array_append_vals(g_array_ref(v1.value.array), data, len);
  }

  return value_new_array(result);
}

value_t value_dup_array ( value_t v1 )
{
  GArray *array;
  value_t v2;
  gsize i;

  array = g_array_sized_new(FALSE, FALSE, sizeof(value_t), v1.value.array->len);
  g_array_set_clear_func(array, (GDestroyNotify)value_free);

  for(i=0; i<v1.value.array->len;i++)
  {
    v2 = value_dup(g_array_index(v1.value.array, value_t, i));
    g_array_append_val(array, v2);
  }
  return value_new_array(array);
}

value_t value_dup ( value_t v1 )
{
  if(value_is_string(v1))
    return value_new_string(g_strdup(v1.value.string));
  if(value_is_array(v1))
    return value_dup_array(v1);
  return v1;
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
