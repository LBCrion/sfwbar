
#include "expr.h"
#include "scanner.h"
#include "module.h"

const value_t value_na = { .type = EXPR_TYPE_NA };

static void vm_push ( vm_t *vm, value_t val )
{
  g_array_append_val(vm->stack, val);
  vm->max_stack = MAX(vm->max_stack, vm->stack->len);
}

static value_t vm_pop ( vm_t *vm )
{
  value_t val;

  if(!vm->stack->len)
    return value_na;

  val = ((value_t *)(vm->stack->data))[vm->stack->len-1];
  g_array_remove_index(vm->stack, vm->stack->len-1);

  return val;
}

static gboolean vm_op_binary ( vm_t *vm )
{
  value_t v1, v2, result;
  guchar op = *(vm->ip);

  if(!strchr("+-*/%=<>!&|", op) || vm->stack->len<2)
    return FALSE;

  v2 = vm_pop(vm);
  v1 = vm_pop(vm);

  result = value_na;

  if(value_is_na(v1) && value_is_na(v2))
  {
    vm_push(vm, value_na);
    return TRUE;
  }
  else if(value_is_na(v1) && !value_is_na(v2))
  {
    if(value_is_string(v2))
      v1 = value_new_string(g_strdup(""));
    else
      v1 = value_new_numeric(0);
  }
  else if(!value_is_na(v1) && value_is_na(v2))
  {
    if(value_is_string(v1))
      v2 = value_new_string(g_strdup(""));
    else
      v2 = value_new_numeric(0);
  }

  if(value_is_string(v1) && value_is_string(v2))
  {
    if(op == '+')
      result = value_new_string(
          g_strconcat(v1.value.string, v2.value.string, NULL));
    else if(op == '=')
      result = value_new_numeric(
          !g_ascii_strcasecmp(v1.value.string, v2.value.string));
  }

  else if(value_is_numeric(v1) && value_is_numeric(v2))
  {
    if(op=='+')
      result = value_new_numeric(v1.value.numeric + v2.value.numeric);
    else if(op=='-')
      result = value_new_numeric(v1.value.numeric - v2.value.numeric);
    else if(op=='*')
      result = value_new_numeric(v1.value.numeric * v2.value.numeric);
    else if(op=='/')
      result = value_new_numeric(v1.value.numeric / v2.value.numeric);
    else if(op=='%')
      result = value_new_numeric(
          (gint)v1.value.numeric % (gint)v2.value.numeric);
    else if(op=='&')
      result = value_new_numeric(v1.value.numeric && v2.value.numeric);
    else if(op=='|')
      result = value_new_numeric(v1.value.numeric || v2.value.numeric);
    else if(op=='<')
      result = value_new_numeric(v1.value.numeric < v2.value.numeric);
    else if(op=='>')
      result = value_new_numeric(v1.value.numeric > v2.value.numeric);
    else if(op=='=')
      result = value_new_numeric(v1.value.numeric == v2.value.numeric);
    else if(op=='!')
      result = value_new_numeric(v1.value.numeric != v2.value.numeric);
    else
      result= value_na;
  }

  vm_push(vm, result);
  value_free(v1);
  value_free(v2);

  return TRUE;
}

static value_t vm_ptr_to_value ( gpointer ptr, gboolean str )
{
  value_t value;

  if(!ptr)
  {
    value = value_na;
  }
  else if(str)
    value = value_new_string(ptr?ptr:g_strdup(""));
  else
  {
    value = value_new_numeric(*((gdouble *)ptr));
    g_free(ptr);
  }

  return value;
}

gint expr_vm_get_func_params ( vm_t *vm, value_t *params[] )
{
  guint8 np = *(vm->ip+1);

  *params = (value_t *)vm->stack->data + vm->stack->len - np;

  return np;
}

static gpointer *vm_function_params_read ( vm_t *vm, gint np,
    gchar *spec )
{
  gpointer *params;
  value_t *v1;
  gint i, j=0;

  if(!spec)
    return NULL;
  params = g_malloc0(sizeof(gpointer)*strlen(spec));
  for(i=0; i<strlen(spec) && j<np; i++)
  {
    v1 = &((value_t *)(vm->stack->data))[vm->stack->len-np+j];
    if(g_ascii_tolower(spec[i])=='n' && value_is_numeric(*v1))
      params[j++] = &(v1->value.numeric);
    else if(g_ascii_tolower(spec[i])=='s' && value_is_string(*v1))
      params[j++] = v1->value.string;
    else if(!g_ascii_islower(spec[i]))
      break;
  }

  return params;
}

static gboolean vm_function ( vm_t *vm )
{
  ModuleExpressionHandlerV1 *handler;
  vm_function_t *func;
  value_t v1, result;
  gchar *name = *((gchar **)(vm->ip+2));
  guint8 np = *(vm->ip+1);
  gpointer *params, *ptr;
  gint i;

  if(np>vm->stack->len)
    return FALSE;
  result = value_na;
  if( (func = vm_func_lookup(name)) )
  {
    value_t *stack;
    stack = (value_t *)vm->stack->data + vm->stack->len - np;
    result = func->function(vm, stack, np);
    vm->cache->vstate |= (!func->deterministic);
  }
  else if( (handler = module_expr_func_get(name)) )
  {
    if(handler->flags & MODULE_EXPR_RAW)
      ptr = handler->function((void *)vm, vm->cache->widget, vm->cache->event);
    else
    {
      params = vm_function_params_read(vm, np, handler->parameters);
      ptr = handler->function(params, NULL, NULL);
      g_free(params);
    }

/*    if(handler->flags & MODULE_EXPR_NUMERIC)
      g_message("func: %s = %lf", name, *((gdouble *)ptr));
    else
      g_message("func: %s = %s", name, (gchar *)ptr);*/
    result = vm_ptr_to_value(ptr,
        !(handler->flags & MODULE_EXPR_NUMERIC));
    if(!(handler->flags & MODULE_EXPR_DETERMINISTIC))
      vm->cache->vstate = TRUE;
  }
  expr_dep_add(name, vm->cache);
  vm->ip += sizeof(gpointer)+1;

  for(i=0; i<np; i++)
  {
    v1 = vm_pop(vm);
    value_free(v1);
  }
  vm_push(vm, result);

  return TRUE;
}

static void vm_variable ( vm_t *vm )
{
  value_t value;
  gchar *name = *((gchar **)(vm->ip+1));

  value = scanner_get_value(name, !vm->use_cached, vm->cache);
  expr_dep_add(name, vm->cache);

  vm_push(vm, value);
  vm->ip += sizeof(gpointer);
}

value_t vm_run ( expr_cache_t *expr )
{
  GByteArray *code = expr->code;
  vm_t *vm;
  value_t v1;

  if(!code)
    return value_na;

  vm = g_malloc0(sizeof(vm_t));
  vm->stack = g_array_sized_new(FALSE, FALSE, sizeof(value_t),
      MAX(1, expr->stack_depth));
  vm->cache = expr;

  for(vm->ip = code->data; (vm->ip-code->data)<code->len; vm->ip++)
  {
    //g_message("stack %d, op %d", vm->stack->len, *vm->ip);
    if(*vm->ip == EXPR_OP_IMMEDIATE)
    {
      v1 = *((value_t *)(vm->ip+1));
      if(value_is_string(v1))
      {
        v1.value.string=g_strdup((gchar *)vm->ip+2);
        vm->ip+=strlen(v1.value.string)+2;
      }
      else
        vm->ip+=sizeof(value_t);
      vm_push(vm, v1);
    }
    else if(*vm->ip == EXPR_OP_CACHED)
      vm->use_cached = *(++vm->ip);
    else if(*vm->ip == EXPR_OP_VARIABLE)
      vm_variable(vm);
    else if(*vm->ip == EXPR_OP_FUNCTION)
      vm_function(vm);
    else if(*vm->ip == EXPR_OP_JMP)
      vm->ip+=*((guint *)(vm->ip+1))+sizeof(gint);
    else if(*vm->ip == EXPR_OP_JZ)
    {
      v1 = vm_pop(vm);
      if(!value_is_numeric(v1) || !v1.value.numeric)
        vm->ip+=*((gint *)(vm->ip+1));
      value_free(v1);
      vm->ip+=sizeof(gint);
    }
    else if(*vm->ip == '!')
    {
      v1 = vm_pop(vm);
      if(value_is_numeric(v1))
        v1.value.numeric = !v1.value.numeric;
      else
      {
        value_free(v1);
        v1 = value_na;
      }
      vm_push(vm, v1);
    }
    else if(!vm_op_binary(vm))
      g_message("invalid op");
  }

  if(vm->stack->len!=1)
    g_message("stack too long");


  v1=value_na;
  while(vm->stack->len>0)
  {
    v1=vm_pop(vm);
    if(vm->stack->len)
      value_free(v1);
  }
  expr->stack_depth = MAX(expr->stack_depth, vm->max_stack);
  g_array_unref(vm->stack);
  g_free(vm);
  return v1;
}
