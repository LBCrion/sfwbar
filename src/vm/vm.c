
#include "expr.h"
#include "scanner.h"
#include "module.h"
#include "gui/taskbaritem.h"

const value_t value_na = { .type = EXPR_TYPE_NA };
static value_t vm_run ( vm_t *vm, guint8 np );

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

static void vm_stack_unwind ( vm_t *vm, gsize target )
{
  value_t v1;

  while(vm->stack->len>target)
  {
    v1 = vm_pop(vm);
    value_free(v1);
  }
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

  else if(value_is_array(v1) || value_is_array(v2))
    result = value_array_concat(v1, v2);

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

value_t vm_function_call ( vm_t *vm, GBytes *code, guint8 np )
{
  value_t v1;
  guint8 *saved_code, *saved_ip;
  gsize saved_len, saved_stack;

  if(!code)
    return value_na;

  saved_code = vm->code;
  saved_ip = vm->ip;
  saved_len = vm->len;
  saved_stack = vm->stack->len;

  vm->code = (guint8 *)g_bytes_get_data(code, &vm->len);
  v1 = vm_run(vm, np);

  if(vm->expr)
    vm->expr->vstate = TRUE;

  vm->code = saved_code;
  vm->ip = saved_ip;
  vm->len = saved_len;
  vm_stack_unwind(vm, saved_stack);

  return v1;
}

static gboolean vm_function ( vm_t *vm )
{
  vm_function_t *func;
  value_t v1, result;
  guint8 np = *(vm->ip+1);
  gint i;

  if(np>vm->stack->len)
    return FALSE;

  memcpy(&func, vm->ip+2, sizeof(gpointer));
  if(!(func->flags & VM_FUNC_USERDEFINED) && func->ptr.function)
  {
    result = func->ptr.function(vm,
        (value_t *)vm->stack->data + vm->stack->len - np, np);
    if(vm->expr)
      vm->expr->vstate |= !(func->flags & VM_FUNC_DETERMINISTIC);
  }
  else if(func->flags & VM_FUNC_USERDEFINED)
    result = vm_function_call(vm, func->ptr.code, np);
  else
    result = value_na;

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

static void vm_local ( vm_t *vm )
{
  value_t v1;
  guint16 pos;

  memcpy(&pos, vm->ip+1, sizeof(guint16));

  v1 = value_dup(((value_t *)(vm->stack->data))[vm->fp+pos-1]);

  vm_push(vm, v1);
  g_ptr_array_add(vm->pstack, vm->ip);
  vm->ip += sizeof(guint16);
}

static void vm_assign ( vm_t *vm )
{
  value_t v1;
  guint16 pos;

  memcpy(&pos, vm->ip+1, sizeof(guint16));

  v1 = vm_pop(vm);
  ((value_t *)(vm->stack->data))[vm->fp+pos-1] = value_dup(v1);
  vm->ip += sizeof(guint16)*1;
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

static value_t vm_run ( vm_t *vm, guint8 np )
{
  value_t v1;
  gint jmp, saved_fp;

  if(!vm->code)
    return value_na;
  if(IS_TASKBAR_ITEM(vm->widget))
    vm->win = flow_item_get_source(vm->widget);

  if(!vm->stack)
    vm->stack = g_array_sized_new(FALSE, FALSE, sizeof(value_t),
        MAX(1, vm->expr? vm->expr->stack_depth : 1));
  if(!vm->pstack)
    vm->pstack = g_ptr_array_sized_new(
        MAX(1, vm->expr? vm->expr->stack_depth : 1));

  saved_fp = vm->fp;
  vm->fp = vm->stack->len - np;

  for(vm->ip = vm->code; (vm->ip-vm->code)<vm->len; vm->ip++)
  {
    //g_message("stack %d, op %d", vm->stack->len, *vm->ip);
    if(*vm->ip == EXPR_OP_IMMEDIATE)
      vm_immediate(vm);
    else if(*vm->ip == EXPR_OP_CACHED)
      vm->use_cached = *(++vm->ip);
    else if(*vm->ip == EXPR_OP_VARIABLE)
      vm_variable(vm);
    else if(*vm->ip == EXPR_OP_LOCAL)
      vm_local(vm);
    else if(*vm->ip == EXPR_OP_ASSIGN)
      vm_assign(vm);
    else if(*vm->ip == EXPR_OP_RETURN)
      break;
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
    else if (*vm->ip == EXPR_OP_DISCARD)
    {
      v1 = vm_pop(vm);
      value_free(v1);
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

  vm->fp = saved_fp;

  return vm_pop(vm);
}

static void vm_free ( vm_t *vm )
{
  if(vm->stack)
  {
    if(vm->stack->len)
      g_message("stack too long");

    vm_stack_unwind(vm, 0);

    if(vm->expr)
      vm->expr->stack_depth = MAX(vm->expr->stack_depth, vm->max_stack);

    g_array_unref(vm->stack);
  }

  if(vm->pstack)
    g_ptr_array_free(vm->pstack, TRUE);

  g_free(vm);
}

value_t vm_expr_eval ( expr_cache_t *expr )
{
  value_t v1;
  vm_t *vm;

  vm = g_malloc0(sizeof(vm_t));

  vm->code = (gpointer)g_bytes_get_data(expr->code, &vm->len);
  vm->widget = expr->widget;
  vm->event = expr->event;
  vm->expr = expr;
  vm->wstate = action_state_build(vm->widget, vm->win);

  v1 = vm_run(vm, 0);
  vm_free(vm);

  return v1;
}

void vm_run_action ( GBytes *code, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state )
{
  vm_t *vm;

  if(!code)
    return;
  vm = g_malloc0(sizeof(vm_t));
  vm->code = (gpointer)g_bytes_get_data(code, &vm->len);
  vm->widget = widget;
  vm->event = event;
  vm->win = win;
  vm->wstate = state? *state : action_state_build(vm->widget, vm->win);

  value_free(vm_run(vm, 0));
  vm_free(vm);
}

void vm_run_user_defined ( gchar *name, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state )
{
  vm_function_t *func;

  func = vm_func_lookup(name);
  if(func->flags & VM_FUNC_USERDEFINED)
    vm_run_action(func->ptr.code, widget, event, win, state);
}
