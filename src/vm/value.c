
#include "vm/value.h"

void value_free ( value_t v1 )
{
  if(value_is_string(v1))
    g_free(v1.value.string);
  else if(value_is_array(v1))
    g_array_unref(v1.value.array);
}

value_t value_array_concat ( value_t v1, value_t v2 )
{
  if(!value_is_array(v1) && !value_is_array(v1))
    return value_na;

  if(!value_is_array(v1))
    return value_new_array(g_array_prepend_vals(g_array_ref(v2.value.array),
          &v1, 1));
  if(!value_is_array(v2))
    return value_new_array(g_array_append_vals(g_array_ref(v1.value.array),
          &v2, 1));
  return value_new_array(g_array_append_vals(g_array_ref(v1.value.array),
          v2.value.array->data, v2.value.array->len));
}

value_t value_dup_array ( value_t v1 )
{
  GArray *array;
  value_t v2;
  gsize i;

  array = g_array_new(FALSE, FALSE, sizeof(value_t));
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
