/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "expr.h"
#include "scanner.h"
#include "module.h"
#include "gui/taskbaritem.h"

const value_t value_na = { .type = EXPR_TYPE_NA };
static value_t vm_run ( vm_t *vm );

static gint vm_op_length[] = {
  [EXPR_OP_IMMEDIATE] = 2,
  [EXPR_OP_JZ] = 1 + sizeof(gint),
  [EXPR_OP_JMP] = 1 + sizeof(gint),
  [EXPR_OP_CACHED] = 2,
  [EXPR_OP_VARIABLE] = 2 + sizeof(GQuark),
  [EXPR_OP_FUNCTION] = 2 + sizeof(gpointer),
  [EXPR_OP_DISCARD] =  1,
  [EXPR_OP_LOCAL] = 1 + sizeof(guint16),
  [EXPR_OP_LOCAL_ASSIGN] = 1 + sizeof(guint16),
  [EXPR_OP_HEAP_ASSIGN] = 1 + sizeof(GQuark),
  [EXPR_OP_RETURN] = 1,
};


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
  while(vm->stack->len>target)
    value_free(vm_pop(vm));
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

static gboolean vm_exec_sync_thread ( vm_call_t *call )
{
  value_t v1;

  v1 = call->func(call->vm, call->p, call->np);
  g_mutex_lock(&call->mutex);
  call->result = v1;
  call->ready = TRUE;
  g_cond_signal(&call->cond);
  g_mutex_unlock(&call->mutex);

  return FALSE;
}

value_t vm_function_native ( vm_t *vm, vm_function_t *func, gint np )
{
  value_t v1;
  vm_call_t *call;

  if(func->flags & VM_FUNC_THREADSAFE ||
        g_main_context_is_owner(g_main_context_default()))
      return func->ptr.function(vm,
          (value_t *)vm->stack->data + vm->stack->len - np, np);
  call = g_malloc0(sizeof(vm_call_t));
  call->func = func->ptr.function;
  call->vm = vm;
  call->p = (value_t *)vm->stack->data + vm->stack->len - np;
  call->np = np;

  g_mutex_lock(&call->mutex);
  g_main_context_invoke(NULL, (GSourceFunc)vm_exec_sync_thread, call);
  while(!call->ready)
    g_cond_wait(&call->cond, &call->mutex);
  g_mutex_unlock(&call->mutex);

  v1 = call->result;
  g_free(call);

  return v1;
}

value_t vm_function_user ( vm_t *vm, GBytes *code, guint8 np )
{
  value_t v1;
  guint8 *saved_code, *saved_ip;
  gsize saved_len, saved_stack, saved_fp;
  GBytes *saved_bytes;

  if(!code)
    return value_na;

  saved_code = vm->code;
  saved_ip = vm->ip;
  saved_len = vm->len;
  saved_stack = vm->stack->len;
  saved_fp = vm->fp;
  saved_bytes = vm->bytes;

  vm->bytes = g_bytes_ref(code);
  vm->code = (guint8 *)g_bytes_get_data(code, &vm->len);
  vm->fp = vm->stack->len - np;
  v1 = vm_run(vm);

  if(vm->expr)
    vm->expr->vstate = TRUE;

  g_bytes_unref(vm->bytes);
  vm->code = saved_code;
  vm->ip = saved_ip;
  vm->len = saved_len;
  vm->fp = saved_fp;
  vm->bytes = saved_bytes;
  vm_stack_unwind(vm, saved_stack);

  return v1;
}

static gboolean vm_function ( vm_t *vm )
{
  vm_function_t *func;
  value_t result;
  guint8 np;
  gint i;

  np = *(vm->ip+1);
  memcpy(&func, vm->ip+2, sizeof(gpointer));
  if(np>vm->stack->len)
  {
    g_warning("vm: not enough parameters in call to '%s'", func->name);
    return FALSE;
  }

  result = value_na;
  if(!(func->flags & VM_FUNC_USERDEFINED) && func->ptr.function)
  {
    result = vm_function_native(vm, func, np);
    if(vm->expr)
      vm->expr->vstate |= !(func->flags & VM_FUNC_DETERMINISTIC);
  }
  else if(func->flags & VM_FUNC_USERDEFINED)
    result = vm_function_user(vm, func->ptr.code, np);
  else
    result = value_na;

  expr_dep_add(g_quark_from_string(func->name), vm->expr);
  if(np>1 && vm->pstack->len>np-1)
    g_ptr_array_remove_range(vm->pstack, vm->pstack->len-np+1, np-1);
  else if(!np)
    g_ptr_array_add(vm->pstack, vm->ip);
  vm->ip += sizeof(gpointer)+1;

  for(i=0; i<np; i++)
    value_free(vm_pop(vm));

  vm_push(vm, result);

  return TRUE;
}

static void vm_variable ( vm_t *vm )
{
  value_t value;
  vm_var_t *var;
  GQuark quark;
  guint8 ftype;

  memcpy(&ftype, vm->ip+1, 1);
  memcpy(&quark, vm->ip+2, sizeof(GQuark));
  g_debug("vm: heap lookup '%s' in %p", g_quark_to_string(quark), vm->store);
  if( (var = vm_store_lookup(vm->store, quark)) )
  {
    value = value_dup(var->value);
    if(vm->expr)
      vm->expr->vstate = TRUE;
  }
  else
  {
    value = scanner_get_value(quark, ftype, !vm->use_cached, vm->expr);
    expr_dep_add(quark, vm->expr);
  }

  vm_push(vm, value);
  g_ptr_array_add(vm->pstack, vm->ip);
  vm->ip += sizeof(GQuark)+1;
}

static void vm_local ( vm_t *vm )
{
  guint16 pos;

  memcpy(&pos, vm->ip+1, sizeof(guint16));

  vm_push(vm, value_dup(((value_t *)(vm->stack->data))[vm->fp+pos-1]));
  g_ptr_array_add(vm->pstack, vm->ip);
  vm->ip += sizeof(guint16);
  if(vm->expr)
    vm->expr->vstate = TRUE;
}

static void vm_local_assign ( vm_t *vm )
{
  guint16 pos;

  memcpy(&pos, vm->ip+1, sizeof(guint16));

  value_free(((value_t *)(vm->stack->data))[vm->fp+pos-1]);
  ((value_t *)(vm->stack->data))[vm->fp+pos-1] = vm_pop(vm);
  vm->ip += sizeof(guint16);
}

static void vm_heap_assign ( vm_t *vm )
{
  GQuark quark;
  vm_var_t *var;

  memcpy(&quark, vm->ip+1, sizeof(GQuark));
  if( (var = vm_store_lookup(vm->store, quark)) )
  {
    value_free(var->value);
    var->value = vm_pop(vm);
  }
  else
    value_free(vm_pop(vm));
  vm->ip += sizeof(GQuark);
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
    v1 = value_new_string(g_strdup((gchar *)vm->ip+2));
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

  if(!vm->stack)
    vm->stack = g_array_sized_new(FALSE, FALSE, sizeof(value_t),
        MAX(1, vm->expr? vm->expr->stack_depth : 1));
  if(!vm->pstack)
    vm->pstack = g_ptr_array_sized_new(
        MAX(1, vm->expr? vm->expr->stack_depth : 1));

  for(vm->ip = vm->code; (vm->ip-vm->code)<vm->len; vm->ip++)
  {
    //g_message("stack %d, op %d", vm->stack->len, *vm->ip);
    if(*vm->ip < EXPR_OP_LAST &&
        (vm->code+vm->len-vm->ip < vm_op_length[*vm->ip]))
    {
      g_warning("vm: truncated operator %d", *vm->ip);
      return value_na;
    }
    if(*vm->ip == EXPR_OP_IMMEDIATE)
      vm_immediate(vm);
    else if(*vm->ip == EXPR_OP_CACHED)
      vm->use_cached = *(++vm->ip);
    else if(*vm->ip == EXPR_OP_VARIABLE)
      vm_variable(vm);
    else if(*vm->ip == EXPR_OP_LOCAL)
      vm_local(vm);
    else if(*vm->ip == EXPR_OP_LOCAL_ASSIGN)
      vm_local_assign(vm);
    else if(*vm->ip == EXPR_OP_HEAP_ASSIGN)
      vm_heap_assign(vm);
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
      value_free(vm_pop(vm));
    else if(*vm->ip == '!')
    {
      v1 = vm_pop(vm);
      if(value_is_numeric(v1))
        vm_push(vm, value_new_numeric(!v1.value.numeric));
      else
        vm_push(vm, value_na);
      value_free(v1);
    }
    else if(!vm_op_binary(vm))
    {
      g_warning("invalid op %d at %ld (stack %u)", *vm->ip, vm->ip - vm->code, vm->stack->len);
      for(gint i=0;i<vm->len;i++)
        g_warning("%d: %d", i, vm->code[i]);
      break;
    }
  }

  return vm_pop(vm);
}

static vm_t *vm_new ( GBytes *code )
{
  vm_t *vm;

  if(!code)
    return NULL;
  vm = g_malloc0(sizeof(vm_t));

  vm->bytes = g_bytes_ref(code);
  vm->code = (gpointer)g_bytes_get_data(code, &vm->len);

  return vm;
}

void vm_widget_set ( vm_t *vm, GtkWidget *widget )
{
  if( (vm->widget = widget) )
    g_object_weak_ref(G_OBJECT(vm->widget), (GWeakNotify)g_nullify_pointer,
        &vm->widget);
}

void vm_widget_unset ( vm_t *vm )
{
  if(vm->widget)
    g_object_weak_unref(G_OBJECT(vm->widget), (GWeakNotify)g_nullify_pointer,
        &vm->widget);
  vm->widget = NULL;
}

static void vm_free ( vm_t *vm )
{
  if(vm->stack)
  {
    if(vm->stack->len)
      g_warning("stack too long");

    vm_stack_unwind(vm, 0);

    if(vm->expr)
      vm->expr->stack_depth = MAX(vm->expr->stack_depth, vm->max_stack);

    g_array_unref(vm->stack);
  }

  if(vm->pstack)
    g_ptr_array_free(vm->pstack, TRUE);

  vm_store_unref(vm->store);
  vm_widget_unset(vm);
  g_bytes_unref(vm->bytes);
  g_free(vm);
}

value_t vm_code_eval ( GBytes *code, GtkWidget *widget )
{
  value_t v1;
  vm_t *vm;

  vm = vm_new(code);
  vm_widget_set(vm, widget);
  vm->wstate = base_widget_state_build(vm->widget, NULL);
  vm->store = vm_store_ref(widget? base_widget_get_store(widget) : NULL);

  v1 = vm_run(vm);
  vm_free(vm);

  return v1;
}

value_t vm_expr_eval ( expr_cache_t *expr )
{
  value_t v1;
  vm_t *vm;

  if(!expr || !expr->code)
    return value_na;
  vm = vm_new(expr->code);

  vm_widget_set(vm, expr->widget);
  vm->event = expr->event;
  vm->expr = expr;
  vm->wstate = base_widget_state_build(vm->widget, vm->win);
  vm->store = expr->store? expr->store :
    (expr->widget? base_widget_get_store(expr->widget) : NULL);
  vm_store_ref(vm->store);

  v1 = vm_run(vm);
  vm_free(vm);

  return v1;
}

static gboolean vm_run_action_thread ( vm_t *vm )
{
  value_free(vm_run(vm));
  if(vm->event)
    gdk_event_free(vm->event);
  vm_free(vm);

  return FALSE;
}

static gpointer vm_action_thread ( GMainContext *context )
{
  GMainLoop *loop;

  g_main_context_push_thread_default(context);
  loop = g_main_loop_new(context, FALSE);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  g_main_context_pop_thread_default(context);
  g_main_context_unref(context);

  return NULL;
}

void vm_run_action ( GBytes *code, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state, vm_store_t *store )
{
  static GMainContext *action_context;
  vm_t *vm;

  g_return_if_fail(code);

  vm = vm_new(code);
  vm_widget_set(vm, widget);
  if(event)
    vm->event = gdk_event_copy(event);
  vm->win = win;
  vm->wstate = state? *state : base_widget_state_build(vm->widget, vm->win);
  vm->store = store? store : (widget? base_widget_get_store(widget) : NULL);
  vm_store_ref(vm->store);

  if(!action_context)
  {
    action_context = g_main_context_new();
    g_thread_new("action", (GThreadFunc)vm_action_thread,
        g_main_context_ref(action_context));
  }

  g_main_context_invoke(action_context, (GSourceFunc)vm_run_action_thread, vm);
}

void vm_run_user_defined ( gchar *name, GtkWidget *widget, GdkEvent *event,
    window_t *win, guint16 *state, vm_store_t *store )
{
  vm_function_t *func;

  func = vm_func_lookup(name);
  if(func->flags & VM_FUNC_USERDEFINED)
    vm_run_action(func->ptr.code, widget, event, win, state, store);
  else
    g_warning("vm: invalid user defined function: %s", name);
}
