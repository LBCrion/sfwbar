
#include "expr.h"
#include "scanner.h"
#include "module.h"
#include "gui/taskbaritem.h"

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
    result = value_na;

  else if(value_like_string(v1) && value_like_string(v2))
  {
    if(op == '+')
      result = value_new_string(
          g_strconcat(value_get_string(v1), value_get_string(v2), NULL));
    else if(op == '=')
      result = value_new_numeric(
          !g_ascii_strcasecmp(value_get_string(v1), value_get_string(v2)));
    else if(op == '!')
      result = value_new_numeric(
          g_ascii_strcasecmp(value_get_string(v1), value_get_string(v2)));
  }

  else if(value_like_numeric(v1) && value_like_numeric(v2))
  {
    if(op=='+')
      result = value_new_numeric(value_get_numeric(v1)+value_get_numeric(v2));
    else if(op=='-')
      result = value_new_numeric(value_get_numeric(v1)-value_get_numeric(v2));
    else if(op=='*')
      result = value_new_numeric(value_get_numeric(v1)*value_get_numeric(v2));
    else if(op=='/')
      result = value_new_numeric(value_get_numeric(v1)/value_get_numeric(v2));
    else if(op=='%')
      result = value_new_numeric(
          (gint)value_get_numeric(v1)%(gint)value_get_numeric(v2));
    else if(op=='&')
      result = value_new_numeric(value_get_numeric(v1)&&value_get_numeric(v2));
    else if(op=='|')
      result = value_new_numeric(value_get_numeric(v1)||value_get_numeric(v2));
    else if(op=='<')
      result = value_new_numeric(value_get_numeric(v1)<value_get_numeric(v2));
    else if(op=='>')
      result = value_new_numeric(value_get_numeric(v1)>value_get_numeric(v2));
    else if(op=='=')
      result = value_new_numeric(value_get_numeric(v1)==value_get_numeric(v2));
    else if(op=='!')
      result = value_new_numeric(value_get_numeric(v1)!=value_get_numeric(v2));
    else
      result= value_na;

    if((op=='<' || op=='>') && *(vm->ip+1)=='=')
    {
      vm->ip++;
      result.value.numeric = value_get_numeric(result) ||
        (value_get_numeric(v1)==value_get_numeric(v2));
    }
  }

  vm_push(vm, result);
  value_free(v1);
  value_free(v2);
  g_ptr_array_remove_index(vm->pstack, vm->pstack->len-1);

  return TRUE;
}

gint expr_vm_get_func_params ( vm_t *vm, value_t *params[] )
{
  guint8 np = *(vm->ip+1);

  *params = (value_t *)vm->stack->data + vm->stack->len - np;

  return np;
}

static gboolean vm_function ( vm_t *vm )
{
  vm_function_t *func;
  value_t v1, result, *stack;
  guint8 np = *(vm->ip+1);
  gint i;

  if(np>vm->stack->len)
    return FALSE;
  memcpy(&func, vm->ip+2, sizeof(gpointer));
  result = value_na;
  if(func->function)
  {
    stack = (value_t *)vm->stack->data + vm->stack->len - np;
    result = func->function(vm, stack, np);
    if(vm->expr)
      vm->expr->vstate |= (!func->deterministic);
  }
  expr_dep_add(func->name, vm->expr);
  if(np>1)
    g_ptr_array_remove_range(vm->pstack, vm->pstack->len-np+1, np-1);
  else if(!np)
    g_ptr_array_add(vm->pstack, vm->ip);
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
  gchar *name;

  memcpy(&name, vm->ip+1, sizeof(gpointer));
  value = scanner_get_value(name, !vm->use_cached, vm->expr);
  expr_dep_add(name, vm->expr);

  vm_push(vm, value);
  g_ptr_array_add(vm->pstack, vm->ip);
  vm->ip += sizeof(gpointer);
}

static void vm_immediate ( vm_t *vm )
{
  value_t v1;

  g_ptr_array_add(vm->pstack, vm->ip);
  if(*(vm->ip+1) != EXPR_TYPE_STRING)
  {
    memcpy(&v1, vm->ip+1, sizeof(value_t));
    vm->ip+=sizeof(value_t);
  }
  else
  {
    v1.type = EXPR_TYPE_STRING;
    v1.value.string = g_strdup((gchar *)vm->ip+2);
    vm->ip += strlen(value_get_string(v1))+2;
  }
  vm_push(vm, v1);
}

static value_t vm_run ( vm_t *vm )
{
  value_t v1;
  gint jmp;

  if(!vm->code)
    return value_na;
  if(IS_TASKBAR_ITEM(vm->widget))
    vm->win = flow_item_get_source(vm->widget);
  vm->wstate = action_state_build(vm->widget, vm->win);

  if(!vm->stack)
    vm->stack = g_array_sized_new(FALSE, FALSE, sizeof(value_t),
        MAX(1, vm->expr? vm->expr->stack_depth : 1));
  if(!vm->pstack)
    vm->pstack = g_ptr_array_sized_new(
        MAX(1, vm->expr? vm->expr->stack_depth: 1));

  for(vm->ip = vm->code; (vm->ip-vm->code)<vm->len; vm->ip++)
  {
    //g_message("stack %d, op %d", vm->stack->len, *vm->ip);
    if(*vm->ip == EXPR_OP_IMMEDIATE)
      vm_immediate(vm);
    else if(*vm->ip == EXPR_OP_CACHED)
      vm->use_cached = *(++vm->ip);
    else if(*vm->ip == EXPR_OP_VARIABLE)
      vm_variable(vm);
    else if(*vm->ip == EXPR_OP_FUNCTION)
      vm_function(vm);
    else if(*vm->ip == EXPR_OP_JMP)
    {
      memcpy(&jmp, vm->ip+1, sizeof(gint));
      vm->ip+=jmp+sizeof(gint);
    }
    else if(*vm->ip == EXPR_OP_JZ)
    {
      g_ptr_array_remove_index(vm->pstack, vm->pstack->len-1);
      v1 = vm_pop(vm);
      if(!value_is_numeric(v1) || !v1.value.numeric)
      {
        memcpy(&jmp, vm->ip+1, sizeof(gint));
        vm->ip+=jmp;
      }
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
    {
      g_message("invalid op");
      break;
    }
  }

  return value_na;
}

static value_t vm_free ( vm_t *vm )
{
  value_t v1;

  if(vm->stack->len>1)
    g_message("stack too long");
  if(vm->stack->len<1)
    g_message("stack too short");

  v1=value_na;
  while(vm->stack->len>0)
  {
    v1=vm_pop(vm);
    if(vm->stack->len)
      value_free(v1);
  }
  if(vm->expr)
    vm->expr->stack_depth = MAX(vm->expr->stack_depth, vm->max_stack);

  g_array_unref(vm->stack);
  g_ptr_array_free(vm->pstack, TRUE);
  g_free(vm);

  return v1;
}

value_t vm_expr_eval ( expr_cache_t *expr )
{
  vm_t *vm;

  vm = g_malloc0(sizeof(vm_t));

  vm->code = (gpointer)g_bytes_get_data(expr->code, &vm->len);
  vm->widget = expr->widget;
  vm->event = expr->event;
  vm->expr = expr;

  vm_run(vm);

  return vm_free(vm);
}

void vm_run_action ( GBytes *code, GtkWidget *widget, GdkEvent *event )
{
  value_t v1;
  vm_t *vm;

  vm = g_malloc0(sizeof(vm_t));
  vm->code = (gpointer)g_bytes_get_data(code, &vm->len);
  vm->widget = widget;
  vm->event = event;

  vm_run(vm);
  v1 = vm_free(vm);
  value_free(v1);
}
