
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
    val.type = EXPR_TYPE_NA;
  else
  {
    val = ((value_t *)(vm->stack->data))[vm->stack->len-1];
    g_array_remove_index(vm->stack, vm->stack->len-1);
  }

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

  result.type = EXPR_TYPE_NA;

  if(v1.type==EXPR_TYPE_STRING && v2.type==EXPR_TYPE_STRING)
  {
    if(op == '+')
    {
      result.type = EXPR_TYPE_STRING;
      result.value.string = g_strconcat(v1.value.string, v2.value.string, NULL);
    }
    else if(op == '=')
    {
      result.type = EXPR_TYPE_NUMERIC;
      result.value.numeric = !g_ascii_strcasecmp(v1.value.string, v2.value.string);
    }
  }

  else if(v1.type==EXPR_TYPE_NUMERIC && v2.type==EXPR_TYPE_NUMERIC)
  {
    result.type = EXPR_TYPE_NUMERIC;
    if(op=='+')
      result.value.numeric = v1.value.numeric + v2.value.numeric;
    else if(op=='-')
      result.value.numeric = v1.value.numeric - v2.value.numeric;
    else if(op=='*')
      result.value.numeric = v1.value.numeric * v2.value.numeric;
    else if(op=='/')
      result.value.numeric = v1.value.numeric / v2.value.numeric;
    else if(op=='%')
      result.value.numeric = (gint)v1.value.numeric %- (gint)v2.value.numeric;
    else if(op=='&')
      result.value.numeric = v1.value.numeric && v2.value.numeric;
    else if(op=='|')
      result.value.numeric = v1.value.numeric || v2.value.numeric;
    else if(op=='<')
      result.value.numeric = v1.value.numeric < v2.value.numeric;
    else if(op=='>')
      result.value.numeric = v1.value.numeric > v2.value.numeric;
    else if(op=='=')
      result.value.numeric = v1.value.numeric == v2.value.numeric;
    else if(op=='!')
      result.value.numeric = v1.value.numeric != v2.value.numeric;
    else
      result.type = EXPR_TYPE_NA;
  }

  vm_push(vm, result);
  if(v1.type == EXPR_TYPE_STRING)
    g_free(v1.value.string);
  if(v2.type == EXPR_TYPE_STRING)
    g_free(v2.value.string);
  return TRUE;
}

static value_t vm_ptr_to_value ( gpointer ptr, gboolean str )
{
  value_t value;

  if(!ptr)
  {
    value.type = EXPR_TYPE_NA;
  }
  else if(str)
  {
    value.type = EXPR_TYPE_STRING;
    value.value.string = ptr?ptr:g_strdup("");
  }
  else
  {
    value.type = EXPR_TYPE_NUMERIC;
    value.value.numeric = *((gdouble *)ptr);
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
    if(g_ascii_tolower(spec[i])=='n' && v1->type==EXPR_TYPE_NUMERIC)
      params[j++] = &(v1->value.numeric);
    else if(g_ascii_tolower(spec[i])=='s' && v1->type==EXPR_TYPE_STRING)
      params[j++] = v1->value.string;
    else if(!g_ascii_islower(spec[i]))
      break;
  }

  return params;
}

static gboolean vm_function ( vm_t *vm )
{
  ModuleExpressionHandlerV1 *handler;
  value_t v1, result;
  gchar *name = (gchar *)(vm->ip+2);
  guint8 np = *(vm->ip+1);
  gpointer *params, *ptr;
  gint i;

  if(np>vm->stack->len)
    return FALSE;
  result.type = EXPR_TYPE_NA;
  if( (handler = module_expr_func_get(name)) )
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
  vm->ip += strlen(name)+2;

  for(i=0; i<np; i++)
  {
    v1 = vm_pop(vm);
    if(v1.type == EXPR_TYPE_STRING)
      g_free(v1.value.string);
  }
  vm_push(vm, result);

  return TRUE;
}

static void vm_variable ( vm_t *vm )
{
  value_t value;
  gchar *name = (gchar *)(vm->ip+1);

  value = scanner_get_value(name, !vm->use_cached, vm->cache);
  expr_dep_add(name, vm->cache);

  vm_push(vm, value);
  vm->ip+=strlen(name)+1;
}

value_t vm_run ( expr_cache_t *expr )
{
  GByteArray *code = expr->code;
  vm_t *vm;
  value_t v1;

  if(!code)
    return value_na;

  vm = g_malloc0(sizeof(vm_t));
  vm->stack = g_array_sized_new(FALSE, FALSE, MAX(1, expr->stack_depth),
        sizeof(value_t));
  vm->cache = expr;

  for(vm->ip = code->data; (vm->ip-code->data)<code->len; vm->ip++)
  {
    //g_message("stack %d, op %d", vm->stack->len, *vm->ip);
    if(*vm->ip == EXPR_OP_IMMEDIATE)
    {
      v1 = *((value_t *)(vm->ip+1));
      if(v1.type==EXPR_TYPE_STRING)
      {
        v1.value.string=g_strdup((gchar *)vm->ip+1+sizeof(value_t));
        vm->ip+=strlen(v1.value.string)+1;
      }
      vm_push(vm, v1);
      vm->ip+=sizeof(value_t);
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
      if(v1.type != EXPR_TYPE_NUMERIC || !v1.value.numeric)
        vm->ip+=*((gint *)(vm->ip+1));
      if(v1.type == EXPR_TYPE_STRING)
        g_free(v1.value.string);
      vm->ip+=sizeof(gint);
    }
    else if(*vm->ip == '!')
    {
      v1 = vm_pop(vm);
      if(v1.type==EXPR_TYPE_NUMERIC)
        v1.value.numeric = !v1.value.numeric;
      else
      {
        if(v1.type == EXPR_TYPE_STRING)
          g_free(v1.value.string);
        v1.type = EXPR_TYPE_NA;
      }
      vm_push(vm, v1);
    }
    else if(!vm_op_binary(vm))
      g_message("invalid op");
  }

  if(vm->stack->len!=1)
    g_message("stack too long");

  v1.type = EXPR_TYPE_NA;
  while(vm->stack->len>0)
  {
    v1 = vm_pop(vm);
    if(v1.type==EXPR_TYPE_STRING && vm->stack->len)
      g_free(v1.value.string);
  }
  expr->stack_depth = MAX(expr->stack_depth, vm->max_stack);
  g_array_unref(vm->stack);
  g_free(vm);
  return v1;
}
