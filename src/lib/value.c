/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "scanner.h"
#include "util/json.h"
#include "util/string.h"

/* extract a substring */
static value_t lib_value_mid ( vm_t *vm, value_t p[], gint np )
{
  gint len, c1, c2;

  vm_param_check_np(vm, np, 3, "mid");
  vm_param_check_string(vm, p, 0, "mid");
  vm_param_check_numeric(vm, p, 1, "mid");
  vm_param_check_numeric(vm, p, 2, "mid");

  c1 = value_get_numeric(p[1]);
  c2 = value_get_numeric(p[2]);
  len = strlen(value_get_string(p[0]));

  /* negative offsets are relative to the end of the string */
  c1 = CLAMP(c1<0? c1+len+1 : c1, 1, len);
  c2 = CLAMP(c2<0? c2+len+1 : c2, 1, len);

  return value_take_string(g_strndup(value_get_string(p[0]) + MIN(c1, c2)-1,
        (ABS(c2-c1)+1)*sizeof(gchar)));
}

/* replace a substring within a string */
static value_t lib_value_replace( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 3, "replace");
  vm_param_check_string(vm, p, 0, "replace");
  vm_param_check_string(vm, p, 1, "replace");
  vm_param_check_string(vm, p, 2, "replace");

  return value_take_string(str_replace(value_get_string(p[0]), value_get_string(p[1]),
        value_get_string(p[2])));
}

static value_t lib_value_replace_all( vm_t *vm, value_t p[], gint np )
{
  value_t result, tmp;
  gint i;

  if(np<1 || !(np%2) || !value_like_string(p[0]) || !value_get_string(p[0]))
    return value_na;

  result = value_new_string(value_get_string(p[0]));
  for(i=1; i<np-1; i+=2)
    if(value_like_string(p[i]) && value_like_string(p[i+1]))
    {
      tmp = result;
      result = value_take_string(str_replace(value_get_string(tmp),
            value_get_string(p[i]), value_get_string(p[i+1])));
      value_free(tmp);
    }

  return result;
}

static value_t lib_value_map( vm_t *vm, value_t p[], gint np )
{
  gint i;

  if(np<2 || np%2)
    return value_na;

  for(i=0; i<np; i++)
    if(!value_like_string(p[i]))
      return value_na;

  for(i=1; i<(np-1); i+=2)
    if(value_get_string(p[i]) &&
        !g_strcmp0(value_get_string(p[0]), value_get_string(p[i])))
      return value_new_string(value_get_string(p[i+1]));
  return value_new_string(value_get_string(p[np-1]));
}

static value_t lib_value_array_map( vm_t *vm, value_t p[], gint np )
{
  gsize i;

  vm_param_check_np_range(vm, np, 3, 4, "arraymap");
  vm_param_check_array(vm, p, 1, "arraymap");
  vm_param_check_array(vm, p, 2, "arraymap");

  if( (p[1].value.array->len != p[2].value.array->len) &&
      (p[1].value.array->len != p[2].value.array->len-1))
  {
    g_warning("ArrayMap: inconsistent array sizes");
    return value_na;
  }
  for(i=0; i<p[1].value.array->len; i++)
    if(value_compare(g_array_index(p[1].value.array, value_t, i), p[0]))
      break;

  if(i!=p[1].value.array->len)
    return value_dup(g_array_index(p[2].value.array, value_t, i));

  if(p[1].value.array->len==p[2].value.array->len-1)
    return value_dup(g_array_index(p[2].value.array, value_t,
          p[2].value.array->len-1));

  return np==4? value_dup(p[3]) : value_na;
}

static value_t lib_value_lookup( vm_t *vm, value_t p[], gint np )
{
  gchar *result = NULL;
  gint i;

  if(np<2 || np%2 || !value_like_numeric(p[0]))
    return value_new_string("");

  for(i=(np-3); i>0; i-=2)
    if(value_like_numeric(p[i]) && value_like_string(p[i+1]) &&
        value_get_numeric(p[i]) < value_get_numeric(p[0]))
      result = value_get_string(p[i+1]);

  if(!result && value_like_string(p[np-1]))
    result = value_get_string(p[np-1]);

  return value_new_string(result?result:"");
}

static value_t lib_value_array_lookup( vm_t *vm, value_t p[], gint np )
{
  gsize i;

  vm_param_check_np_range(vm, np, 3, 4, "arraylookup");
  vm_param_check_numeric(vm, p, 0, "arraylookup");
  vm_param_check_array(vm, p, 1, "arraylookup");
  vm_param_check_array(vm, p, 2, "arraylookup");

  if( (p[1].value.array->len != p[2].value.array->len) &&
      (p[1].value.array->len != p[2].value.array->len-1))
  {
    g_warning("ArrayLookup: inconsistent array sizes");
    return value_na;
  }
  for(i=0; i<p[1].value.array->len; i++)
    if(value_as_numeric(g_array_index(p[1].value.array, value_t, i)) <
        value_get_numeric(p[0]))
      break;

  if(i!=p[1].value.array->len)
    return value_dup(g_array_index(p[2].value.array, value_t, i));

  if(p[1].value.array->len==p[2].value.array->len-1)
    return value_dup(g_array_index(p[2].value.array, value_t,
          p[2].value.array->len-1));

  return np==4? value_dup(p[3]) : value_na;
}

/* Extract substring using regex */
static value_t lib_value_extract( vm_t *vm, value_t p[], gint np )
{
  value_t res;
  GRegex *regex;
  GMatchInfo *match;

  vm_param_check_np(vm, np, 2, "extract");
  vm_param_check_string(vm, p, 0, "extract");
  vm_param_check_string(vm, p, 1, "extract");

  if( !(regex = g_regex_new(value_get_string(p[1]), 0, 0, NULL)) )
    return value_na;

  if(g_regex_match (regex, value_get_string(p[0]), 0, &match) && match)
    res = value_take_string(g_match_info_fetch (match, 1));
  else
    res = value_na;

  if(match)
    g_match_info_free(match);
  if(regex)
    g_regex_unref(regex);

  return res;
}

static value_t lib_value_extract_json( vm_t *vm, value_t p[], gint np )
{
  struct json_object *obj;
  value_t result;

  vm_param_check_np(vm, np, 2, "extractjson");
  vm_param_check_string(vm, p, 0, "extractjson");
  vm_param_check_string(vm, p, 1, "extractjson");

  if( !(obj = json_tokener_parse(value_get_string(p[0]))) )
    return value_na;
  result = value_from_json(jpath_parse(value_get_string(p[1]), obj));
  json_object_put(obj);

  return result;
}

static value_t lib_value_pad ( vm_t *vm, value_t p[], gint np )
{
  gchar *result, *ptr;
  gint n, len, sign;
  gchar padchar;

  vm_param_check_np_range(vm, np, 2, 3, "pad");
  vm_param_check_string(vm, p, 0, "pad");
  vm_param_check_numeric(vm, p, 1, "pad");
  if(np==3)
    vm_param_check_string(vm, p, 2, "pad");

  padchar = (np==3)?  *(value_get_string(p[2])) : ' ';

  len = strlen(value_get_string(p[0]));
  n = value_get_numeric(p[1]);
  sign = n>=0;
  n = MAX(ABS(n), len);

  result = g_malloc(n+1);
  if(sign)
  {
    memset(result, padchar, n-len);
    strcpy(result+n-len, value_get_string(p[0]));
  }
  else
  {
    ptr = g_stpcpy(result, value_get_string(p[0]));
    memset(ptr, padchar, n-len);
    *(result+n) = '\0';
  }

  return value_take_string(result);
}

static value_t lib_value_elapsed_str ( vm_t *vm, value_t p[], gint np )
{
  if(np!=1 || !value_like_numeric(p[0]))
    return value_na;

  if(value_get_numeric(p[0])>3600*24)
    return value_take_string(g_strdup_printf("%d days ago",
          (gint)(value_get_numeric(p[0])/(3600*24))));
  if(value_get_numeric(p[0])>3600)
    return value_take_string(g_strdup_printf("%d hours ago",
        (gint)(value_get_numeric(p[0])/3600)));
  if(value_get_numeric(p[0])>60)
    return value_take_string(g_strdup_printf("%d minutes ago",
        (gint)(value_get_numeric(p[0])/60)));
  return value_take_string(g_strdup("Just now"));
}

static value_t lib_value_max ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "max");
  vm_param_check_numeric(vm, p, 0, "max");
  vm_param_check_numeric(vm, p, 1, "max");

  return value_new_numeric(MAX(value_get_numeric(p[0]), value_get_numeric(p[1])));
}

static value_t lib_value_min ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "min");
  vm_param_check_numeric(vm, p, 0, "min");
  vm_param_check_numeric(vm, p, 1, "min");

  return value_new_numeric(MIN(value_get_numeric(p[0]), value_get_numeric(p[1])));
}

static value_t lib_value_val ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "val");
  vm_param_check_string(vm, p, 0, "val");

  return value_new_numeric(strtod(value_get_string(p[0]), NULL));
}

static value_t lib_value_str ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 1, 2, "str");
  if(np==2)
    vm_param_check_numeric(vm, p, 1, "str");

  return value_take_string(value_to_string(p[0], np==2? value_get_numeric(p[1]) : 0));
}

static value_t lib_value_upper ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "upper");
  vm_param_check_string(vm, p, 0, "upper");

  return value_take_string(g_ascii_strup(value_get_string(p[0]), -1));
}

static value_t lib_value_lower ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "lower");
  vm_param_check_string(vm, p, 0, "lower");

  return value_take_string(g_ascii_strdown(value_get_string(p[0]), -1));
}

static value_t lib_value_markup  ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "Markup");
  vm_param_check_string(vm, p, 0, "Markup");

  return value_take_string(g_markup_escape_text(value_get_string(p[0]), -1));
}

static value_t lib_value_escape ( vm_t *vm, value_t p[], gint np )
{
  gchar *ptr;
  GString *str;

  vm_param_check_np(vm, np, 1, "Escape");
  vm_param_check_string(vm, p, 0, "Escape");
  str = g_string_new(NULL);
  for(ptr=value_get_string(p[0]); *ptr; ptr++)
  {
    if(*ptr == '"' || *ptr == '\\')
      g_string_append_c(str, '\\');
    g_string_append_c(str, *ptr);
  }
  return value_take_string(g_string_free(str, FALSE));
}

static value_t lib_value_ident ( vm_t *vm, value_t p[], int np )
{
  value_t result;
  vm_function_t func;

  vm_param_check_np(vm, np, 1, "Ident");
  vm_param_check_string(vm, p, 0, "Ident");
  if(!value_get_string(p[0]))
    return value_na;

  result = value_new_numeric(
      (vm_func_copy(&func, vm_func_lookup(value_get_string(p[0]))) &&
       func.ptr.function) || scanner_is_variable(value_get_string(p[0])));

  if(!result.value.numeric)
    expr_dep_add(scanner_parse_identifier(value_get_string(p[0]), NULL), vm->expr);

  return result;
}

static value_t lib_value_arraybuild ( vm_t *vm, value_t p[], gint np )
{
  value_t array;
  gint i;

  array = value_array_create(np);

  for(i=0; i<np; i++)
    value_array_append(array, value_dup(p[i]));

  return array;
}

static value_t lib_value_arrayindex ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayIndex");
  vm_param_check_array(vm, p, 0, "ArrayIndex");
  vm_param_check_numeric(vm, p, 1, "ArrayIndex");

  if(!value_is_array(p[0]) || (gssize)value_get_numeric(p[1])<0 ||
      p[0].value.array->len <= ((gsize)value_get_numeric(p[1])))
  return value_na;

  return value_dup(g_array_index(p[0].value.array, value_t,
      (gint)value_get_numeric(p[1])));
}

static value_t lib_value_arrayassign ( vm_t *vm, value_t p[], gint np )
{
  value_t *v1, arr;
  gssize n;

  vm_param_check_np(vm, np, 3, "ArrayAssign");
  vm_param_check_array(vm, p, 0, "ArrayAssign");
  vm_param_check_numeric(vm, p, 1, "ArrayAssign");

  n = (gssize)value_get_numeric(p[1]);
  if(!value_is_array(p[0]) || n<0)
    return value_na;

  arr = value_dup(p[0]);
  if((gsize)n >= arr.value.array->len)
    g_array_set_size(arr.value.array, n+1);
  v1 = &g_array_index(arr.value.array, value_t, n);
  value_free(*v1);
  *v1 = value_dup(p[2]);

  return arr;
}

static value_t lib_value_arrayconcat ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayConcat");

  return value_array_concat(p[0], p[1]);
}

static value_t lib_value_arraysize ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "ArraySize");

  return value_new_numeric(value_is_array(p[0])? p[0].value.array->len : 0);
}

static value_t lib_value_arrayfind ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayFind");

  return value_new_numeric(value_array_find(p[0], p[1])!=-1);
}

static value_t lib_value_arrayremove ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayRemove");

  return value_array_remove(p[0], p[1]);
}

static value_t lib_value_na ( vm_t *vm, value_t p[], gint np )
{
  return value_na;
}

void lib_value_init ( void )
{
  vm_func_add("mid", lib_value_mid, TRUE, TRUE);
  vm_func_add("pad", lib_value_pad, TRUE, TRUE);
  vm_func_add("extract", lib_value_extract, TRUE, TRUE);
  vm_func_add("extractjson", lib_value_extract_json, TRUE, TRUE);
  vm_func_add("ident", lib_value_ident, TRUE, TRUE);
  vm_func_add("replace", lib_value_replace, TRUE, TRUE);
  vm_func_add("replaceall", lib_value_replace_all, TRUE, TRUE);
  vm_func_add("map", lib_value_map, TRUE, TRUE);
  vm_func_add("arraymap", lib_value_array_map, TRUE, TRUE);
  vm_func_add("lookup", lib_value_lookup, TRUE, TRUE);
  vm_func_add("arraylookup", lib_value_array_lookup, TRUE, TRUE);
  vm_func_add("max", lib_value_max, TRUE, TRUE);
  vm_func_add("elapsedstr", lib_value_elapsed_str, TRUE, TRUE);
  vm_func_add("min", lib_value_min, TRUE, TRUE);
  vm_func_add("val", lib_value_val, TRUE, TRUE);
  vm_func_add("str", lib_value_str, TRUE, TRUE);
  vm_func_add("upper", lib_value_upper, TRUE, TRUE);
  vm_func_add("lower", lib_value_lower, TRUE, TRUE);
  vm_func_add("markup", lib_value_markup, TRUE, TRUE);
  vm_func_add("escape", lib_value_escape, TRUE, TRUE);
  vm_func_add("arraybuild", lib_value_arraybuild, FALSE, TRUE);
  vm_func_add("arrayindex", lib_value_arrayindex, FALSE, TRUE);
  vm_func_add("arrayassign", lib_value_arrayassign, FALSE, TRUE);
  vm_func_add("arrayconcat", lib_value_arrayconcat, FALSE, TRUE);
  vm_func_add("arraysize", lib_value_arraysize, FALSE, TRUE);
  vm_func_add("arrayfind", lib_value_arrayfind, FALSE, TRUE);
  vm_func_add("arrayremove", lib_value_arrayremove, FALSE, TRUE);
  vm_func_add("na", lib_value_na, TRUE, TRUE);
}
