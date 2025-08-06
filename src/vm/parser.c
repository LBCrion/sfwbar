/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "scanner.h"
#include "config/config.h"
#include "util/string.h"
#include "vm/vm.h"

static GHashTable *macros, *parser_instructions;

static gboolean parser_block_parse ( GScanner *scanner, GByteArray * );
static gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code );

static GByteArray *parser_new ( guint8 api2 )
{
  GByteArray *code = g_byte_array_sized_new(2);
  static guint8 count = 0;

  g_byte_array_append(code, &count, 1);
  g_byte_array_append(code, &api2, 1);

  return code;
}

static GBytes *parser_free ( GByteArray *code )
{
  guint8 count, api2, i, data[sizeof(value_t)+1];

  g_return_val_if_fail(code && code->len>0, NULL);

  data[0] = EXPR_OP_IMMEDIATE;
  memcpy(data+1, &value_na, sizeof(value_t));

  count = code->data[0];
  api2 = code->data[1];

  g_byte_array_remove_range(code, 0, 2);

  for(i=0; i<count; i++)
    g_byte_array_prepend(code, data, sizeof(value_t)+1);
  g_byte_array_prepend(code, &api2, 1);

  return g_byte_array_free_to_bytes(code);
}

static gint parser_emit_jump ( GByteArray *code, guint8 op )
{
  guint8 data[1+sizeof(gint)];

  data[0] = op;
  g_byte_array_append(code, data, sizeof(gint)+1);

  return code->len - sizeof(gint);
}

static void parser_emit_local ( GByteArray *code, guint16 pos,
   guint8 op )
{
  guint8 data[sizeof(guint16)+1];

  data[0] = op;
  memcpy(data+1, &pos, sizeof(guint16));
  g_byte_array_append(code, data, sizeof(guint16)+1);
}

static void parser_emit_var_id ( GByteArray *code, gchar *id, guint8 op )
{
  GQuark quark;
  guint8 data[sizeof(gpointer)+sizeof(GQuark)+1], ftype;

  data[0] = op;
  quark = scanner_parse_identifier(id, &ftype);
  memcpy(data+1, &ftype, 1);
  memcpy(data+2, &quark, sizeof(GQuark));
  g_byte_array_append(code, data, sizeof(GQuark)+2);
}

static void parser_emit_quark_op ( GByteArray *code, GQuark quark, guint8 op )
{
  guint8 data[sizeof(GQuark)+1];

  data[0] = op;
  memcpy(data+1, &quark, sizeof(GQuark));
  g_byte_array_append(code, data, sizeof(GQuark)+1);
}

static void parser_emit_string ( GByteArray *code, gchar *str )
{
  guchar data[2];

  data[0] = EXPR_OP_IMMEDIATE;
  data[1] = EXPR_TYPE_STRING;
  g_byte_array_append(code, data, 2);
  g_byte_array_append(code, (guint8 *)str, strlen(str)+1);
}

static void parser_emit_numeric ( GByteArray *code, gdouble numeric )
{
  guchar data[sizeof(value_t)+1];
  value_t value;

  data[0] = EXPR_OP_IMMEDIATE;
  value = value_new_numeric(numeric);
  memcpy(data+1, &value, sizeof(value_t));
  g_byte_array_append(code, data, sizeof(value_t)+1);
}

static void parser_emit_na ( GByteArray *code )
{
  guchar data[sizeof(value_t)+1];

  data[0] = EXPR_OP_IMMEDIATE;
  memcpy(data+1, &value_na, sizeof(value_t));
  g_byte_array_append(code, data, sizeof(value_t)+1);
}

static void parser_emit_function ( GByteArray *code, const void *f, guint8 np )
{
  guint8 data[sizeof(gpointer)+2];

  memcpy(data+2, &f, sizeof(gpointer));
  data[0]=EXPR_OP_FUNCTION;
  data[1]=np;
  g_byte_array_append(code, data, sizeof(gpointer)+2);
}

static void parser_jump_backpatch ( GByteArray *code, gint olen, gint clen )
{
  gint data = clen - olen - sizeof(gint);
  memcpy(code->data + olen, &data, sizeof(gint));
}

const gchar *parser_identifier_lookup ( gchar *identifier )
{
  gchar *lower;
  const gchar *result;

  lower = g_ascii_strdown(identifier, -1);
  result = g_intern_string(lower);
  g_free(lower);

  return result;
}

static guint16 parser_local_lookup ( GScanner *scanner )
{
  if(!SCANNER_DATA(scanner)->locals || scanner->token!=G_TOKEN_IDENTIFIER)
    return 0;
  return GPOINTER_TO_INT(g_hash_table_lookup(
          SCANNER_DATA(scanner)->locals, scanner->value.v_identifier));
}

static vm_var_t *parser_heap_lookup ( GScanner *scanner )
{
  if(!SCANNER_STORE(scanner) || scanner->token!=G_TOKEN_IDENTIFIER)
    return NULL;
  return vm_store_lookup_string(SCANNER_STORE(scanner),
      scanner->value.v_identifier);
}

static gboolean parser_cached ( GScanner *scanner, GByteArray *code )
{
  guchar data[2];

  data[0] = EXPR_OP_CACHED;
  data[1] = TRUE;
  g_byte_array_append(code, data, 2);

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "Expect '(' after Cached",
      SEQ_REQ, -2, parser_expr_parse, code, "missing expression in Cached",
      SEQ_REQ, ')', NULL, NULL, "Expect ')' after Cached",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(scanner->max_parse_errors)
    return FALSE;

  data[1] = FALSE;
  g_byte_array_append(code, data, 2);

  return TRUE;
}

static gboolean parser_if ( GScanner *scanner, GByteArray *code )
{
  gint alen;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "Expect '(' after If",
      SEQ_REQ, -2, parser_expr_parse, code, "Missing condition in If()",
      SEQ_REQ, ',', NULL, NULL, "Expect ',' after If condition",
      SEQ_END);
  if(scanner->max_parse_errors)
    return FALSE;

  alen = parser_emit_jump(code, EXPR_OP_JZ);

  config_parse_sequence(scanner,
      SEQ_REQ, -2, parser_expr_parse, code, "Missing true expression in If()",
      SEQ_REQ, ',', NULL, NULL, "Expect ',' after true condition in If()",
      SEQ_END);

  /* JZ over len change + JMP instruction (1 + sizeof(gint)) */
  parser_jump_backpatch(code, alen, code->len + sizeof(gint) + 1);
  alen = parser_emit_jump(code, EXPR_OP_JMP);

  config_parse_sequence(scanner,
      SEQ_REQ, -2, parser_expr_parse, code, "Missing true expression in If()",
      SEQ_REQ, ')', NULL, NULL, "Expect ')' at the end of If()",
      SEQ_END);
  /* JMP over len change */
  parser_jump_backpatch(code, alen, code->len);

  return !scanner->max_parse_errors;
}

static gboolean parser_function ( GScanner *scanner, GByteArray *code )
{
  gconstpointer ptr;
  guint8 np;

  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "ident"))
    scanner->config->identifier_2_string = TRUE;

  ptr = vm_func_lookup(scanner->value.v_identifier);
  g_scanner_get_next_token(scanner); // consume '('

  np = 0;
  if(g_scanner_peek_next_token(scanner)!=')')
    do
    {
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      np++;
    } while(g_scanner_get_next_token(scanner)==',' && np<255);
  else
    g_scanner_get_next_token(scanner);

  if(scanner->token!=')')
    g_scanner_error(scanner, "Expecting ')' at the end of a function call");

  parser_emit_function(code, ptr, np);
  scanner->config->identifier_2_string = FALSE;

  return scanner->token == ')';
}

static gboolean parser_index_parse( GScanner *scanner, GByteArray *code )
{
  if(!parser_expr_parse(scanner, code))
    return FALSE;
  if(!config_expect_token(scanner, ']', "Expect ']' after array index"))
    return FALSE;

  parser_emit_function(code, vm_func_lookup("arrayindex"), 2);
  return TRUE;
}

static gboolean parser_variable ( GScanner *scanner, GByteArray *code )
{
  guint16 pos;

  if( (pos = parser_local_lookup(scanner)) )
    parser_emit_local(code, pos, EXPR_OP_LOCAL);
  else
    parser_emit_var_id(code, scanner->value.v_identifier, EXPR_OP_VARIABLE);

  while(config_check_and_consume(scanner, '['))
    if(!parser_index_parse(scanner, code))
      return FALSE;
  return TRUE;
}

static gboolean parser_macro_handle ( GScanner *scanner, GByteArray *code )
{
  GBytes *bytes;
  gsize len;
  gconstpointer data;

  if(!macros ||
      !(bytes = g_hash_table_lookup(macros, scanner->value.v_identifier)) )
    return FALSE;

  data = g_bytes_get_data(bytes, &len);
  g_byte_array_append(code, data+1, len-1);
  return TRUE;
}

static gboolean parser_identifier ( GScanner *scanner, GByteArray *code )
{
  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "if"))
    return parser_if(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "cached"))
    return parser_cached(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "true"))
    parser_emit_numeric(code, TRUE);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "false"))
    parser_emit_numeric(code, FALSE);
  else if(g_scanner_peek_next_token(scanner)=='(')
    return parser_function(scanner, code);
  else if(!parser_macro_handle(scanner, code))
    return parser_variable(scanner, code);

  return TRUE;
}

static gboolean parser_array_handle ( GScanner *scanner, GByteArray *code )
{
  guint8 np = 0;

  if(g_scanner_peek_next_token(scanner)!=']')
    do {
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      np++;
    } while(config_check_and_consume(scanner, ','));

  if(!config_expect_token(scanner, ']', "Expected ']' at the end of the list"))
    return FALSE;

  parser_emit_function(code, vm_func_lookup("arraybuild"), np);

  return TRUE;
}

static gboolean parser_value ( GScanner *scanner, GByteArray *code )
{
  guchar data;
  gint token;

  if(g_scanner_eof(scanner))
    return FALSE;

  token = g_scanner_get_next_token(scanner);
  if(token == G_TOKEN_FLOAT)
    parser_emit_numeric(code, scanner->value.v_float);
  else if(token == G_TOKEN_STRING)
    parser_emit_string(code, scanner->value.v_string);
  else if(token == '[')
    parser_array_handle(scanner, code);
  else if(token == G_TOKEN_IDENTIFIER)
    return parser_identifier(scanner, code);
  else if(token == '+')
    return parser_value(scanner, code);
  else if(token == '-')
  {
    if(!parser_value(scanner, code))
      return FALSE;
    parser_emit_numeric(code, -1);
    data = '*';
    g_byte_array_append(code, &data, 1);
    return TRUE;
  }
  else if(token == '!')
  {
    if(!parser_value(scanner, code))
      return FALSE;
    data = '!';
    g_byte_array_append(code, &data, 1);
    return TRUE;
  }
  else if(token == '(')
    return parser_expr_parse(scanner, code) &&
      (g_scanner_get_next_token(scanner)==')');
  else
  {
    g_scanner_error(scanner,
        "Unexpected token in expression. Expected a value.");
    return FALSE;
  }

  return TRUE;
}

static gboolean parser_ops ( GScanner *scanner, GByteArray *code, gint l )
{
  static gchar *expr_ops_list[] = { "&|", "!<>=", "+-", "*/%", NULL };
  static const guchar equal = '=';
  gboolean or_equal;
  guchar op;

  if(!expr_ops_list[l])
    return parser_value(scanner, code);

  if(!parser_ops(scanner, code, l+1))
    return FALSE;
  while(strchr(expr_ops_list[l], g_scanner_peek_next_token(scanner)))
  {
    or_equal = FALSE;
    if(g_scanner_eof(scanner))
      return TRUE;
    if( !(op = g_scanner_get_next_token(scanner)) )
      return TRUE;

    if(op=='!' && !config_expect_token(scanner,'=', "Missing = in !="))
      return FALSE;

    if((op=='>' || op=='<') && config_check_and_consume(scanner, '='))
      or_equal = TRUE;

    if(!parser_ops(scanner, code, l+1))
      return FALSE;

    if(op=='!')
      g_byte_array_append(code, &equal, 1);

    g_byte_array_append(code, &op, 1);

    if(or_equal)
      g_byte_array_append(code, &equal, 1);
  }
  return TRUE;
}

static gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code )
{
  return parser_ops(scanner, code, 0);
}

gboolean parser_macro_add ( GScanner *scanner )
{
  GByteArray *code;
  gchar *name;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &name,
        "Expected and identifier after define",
      SEQ_REQ, '=', NULL, NULL, "Expected '=' after define",
      SEQ_REQ, -2, config_expr, &code, NULL,
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);
  if(scanner->max_parse_errors)
    return FALSE;

  if(!macros)
    macros = g_hash_table_new_full( (GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, (GDestroyNotify)g_bytes_unref);

  g_hash_table_insert(macros, name, code);

  return TRUE;
}

static gboolean parser_assign_parse ( GScanner *scanner, GByteArray *code,
    guint16 pos, vm_var_t *var )
{
  g_return_val_if_fail(pos || var, FALSE);

  if(config_check_and_consume(scanner, '['))
  {
    if(pos)
      parser_emit_local(code, pos, EXPR_OP_LOCAL);
    else
      parser_emit_var_id(code, (gchar *)g_quark_to_string(var->quark),
          EXPR_OP_VARIABLE);
    config_parse_sequence(scanner,
        SEQ_REQ, -2 , parser_expr_parse, code, "Expect index in []",
        SEQ_REQ, ']', NULL, NULL, "Expect ']' after array index",
        SEQ_REQ, '=', NULL, NULL, "Expect '=' after a variable",
        SEQ_REQ, -2, parser_expr_parse, code, "Expect expression after =",
        SEQ_OPT, ';', NULL, NULL, NULL,
        SEQ_END);
    parser_emit_function(code, vm_func_lookup("arrayassign"), 3);
  }
  else
    config_parse_sequence(scanner,
        SEQ_REQ, '=', NULL, NULL, "Expect '=' after a variable",
        SEQ_REQ, -2, parser_expr_parse, code, "Expect expression after =",
        SEQ_OPT, ';', NULL, NULL, NULL,
        SEQ_END);

  if(pos)
    parser_emit_local(code, pos, EXPR_OP_LOCAL_ASSIGN);
  else
    parser_emit_quark_op(code, var->quark, EXPR_OP_HEAP_ASSIGN);

  return !scanner->max_parse_errors;
}

static gboolean parser_action_parse ( GScanner *scanner, GByteArray *code )
{
  gconstpointer ptr;
  gboolean neg;
  static guint8 discard = EXPR_OP_DISCARD;
  gint alen, flag, cond = 0, np = 0;

  if(config_check_and_consume(scanner, '['))
  {
    do
    {
      neg = config_check_and_consume(scanner, '!');

      g_scanner_get_next_token(scanner);
      if( (flag = config_lookup_key(scanner, config_act_cond)) )
        cond |= (neg? flag<<8 : flag);
      else
        g_scanner_error(scanner,"invalid condition '%s' in action",
            scanner->token == G_TOKEN_IDENTIFIER? scanner->value.v_identifier :
            "???");

    } while (config_check_and_consume(scanner, '|'));
    if(!config_expect_token(scanner, ']', "Expected ] after condition block"))
      return FALSE;
  }

  if(cond)
  {
    parser_emit_numeric(code, cond & 0xff);
    parser_emit_numeric(code, cond>>8);

    parser_emit_function(code, vm_func_lookup("checkstate"), 2);
    alen = parser_emit_jump(code, EXPR_OP_JZ);
  }

  if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
    ptr = vm_func_lookup("exec");
  else if(config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
    ptr = vm_func_lookup(scanner->value.v_identifier);
  else
  {
    g_scanner_error(scanner, "action name missing. Got 0x%x", scanner->next_token);
    config_skip_statement(scanner);
    return FALSE;
  }

  if(!cond && parser_local_lookup(scanner))
    return parser_assign_parse(scanner, code, parser_local_lookup(scanner),
        NULL);
  if(!cond && parser_heap_lookup(scanner))
    return parser_assign_parse(scanner, code, 0, parser_heap_lookup(scanner));

  if(SCANNER_DATA(scanner)->api2 && !config_check_and_consume(scanner, '('))
  {
    g_scanner_error(scanner, "Missing '(' in function invocation");
    return FALSE;
  }
  if( !config_lookup_next_key(scanner, config_toplevel_keys) &&
      !config_lookup_next_key(scanner, config_prop_keys) &&
      !config_lookup_next_key(scanner, config_flowgrid_props) &&
      (scanner->next_token == G_TOKEN_FLOAT ||
       scanner->next_token == G_TOKEN_STRING ||
       scanner->next_token == G_TOKEN_IDENTIFIER ||
       strchr("+-!(", scanner->next_token)) )
  {
    do
    {
      if(parser_expr_parse(scanner, code))
        np++;
    } while(config_check_and_consume(scanner, ','));
  }
  if(SCANNER_DATA(scanner)->api2 && !config_check_and_consume(scanner, ')'))
  {
    g_scanner_error(scanner, "Missing ')' in function invocation");
    return FALSE;
  }

  config_check_and_consume(scanner, ';');
  parser_emit_function(code, ptr, np);

  if(cond)
  {
    parser_jump_backpatch(code, alen, code->len + sizeof(gint) + 1);
    alen = parser_emit_jump(code, EXPR_OP_JMP);
    parser_emit_na(code);
    parser_jump_backpatch(code, alen, code->len);
  }
  g_byte_array_append(code, &discard, 1);

  return TRUE;
}

static gboolean parser_var_declare ( GScanner *scanner, GByteArray *code,
    gboolean param )
{
  if(!config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
    return FALSE;

  if(!parser_local_lookup(scanner))
  {
    if(param && code->data[0]==G_MAXUINT8)
    {
      g_scanner_error(scanner, "too many local variables");
      return FALSE;
    }

    g_hash_table_insert(SCANNER_DATA(scanner)->locals,
        g_strdup(scanner->value.v_identifier), GUINT_TO_POINTER(
          g_hash_table_size(SCANNER_DATA(scanner)->locals)+1));

    if(!param)
      code->data[0]++;
  }
  else
    g_scanner_error(scanner,
        "parser: duplicate declaration of a local variable '%s'",
        scanner->value.v_identifier);

  if(g_scanner_peek_next_token(scanner) == '=')
    parser_assign_parse(scanner, code, parser_local_lookup(scanner), NULL);

  return TRUE;
}

static gboolean parser_if_instr_parse ( GScanner *scanner, GByteArray *code )
{
  gint alen;

  if(!parser_expr_parse(scanner, code))
    return FALSE;
  alen = parser_emit_jump(code, EXPR_OP_JZ);
  if(!parser_block_parse(scanner, code))
    return FALSE;
  if(config_check_identifier(scanner, "else"))
  {
    g_scanner_get_next_token(scanner);
    parser_jump_backpatch(code, alen, code->len + sizeof(gint) + 1);
    alen = parser_emit_jump(code, EXPR_OP_JMP);
    if(!parser_block_parse(scanner, code))
      return FALSE;
    parser_jump_backpatch(code, alen, code->len);
  }
  else
    parser_jump_backpatch(code, alen, code->len);
  return TRUE;
}

static gboolean parser_while_parse ( GScanner *scanner, GByteArray *code )
{
  gint alen, rlen;

  rlen = code->len;
  if(!parser_expr_parse(scanner, code))
    return FALSE;
  alen = parser_emit_jump(code, EXPR_OP_JZ);
  if(!parser_block_parse(scanner, code))
    return FALSE;
  parser_emit_jump(code, EXPR_OP_JMP);
  parser_jump_backpatch(code, code->len - sizeof(int), rlen);
  parser_jump_backpatch(code, alen, code->len);

  return TRUE;
}

static gboolean parser_var_parse ( GScanner *scanner, GByteArray *code )
{
  do {
    if(!parser_var_declare(scanner, code, FALSE))
      return FALSE;
  } while(config_check_and_consume(scanner, ','));
  config_check_and_consume(scanner, ';');

  return TRUE;
}

static gboolean parser_return_parse ( GScanner *scanner, GByteArray *code )
{
  guint8 return_op = EXPR_OP_RETURN;

  if(config_check_and_consume(scanner, ';'))
    parser_emit_na(code);
  else
  {
    if(!parser_expr_parse(scanner, code))
      return FALSE;
    config_check_and_consume(scanner, ';');
  }
  g_byte_array_append(code, &return_op, 1);
  return TRUE;
}

static gboolean parser_instr_parse ( GScanner *scanner, GByteArray *code )
{
  gboolean (*func)(GScanner *, GByteArray *);

  if( (func = config_lookup_next_ptr(scanner, parser_instructions)) )
  {
    g_scanner_get_next_token(scanner);
    if(!func(scanner, code))
      return FALSE;
  }
  else if(scanner->next_token == '{')
    return parser_block_parse(scanner, code);
  else
    return parser_action_parse(scanner, code);

  return TRUE;
}

static gboolean parser_block_parse ( GScanner *scanner, GByteArray *code )
{
  gboolean result = TRUE, alloc_hash;

  if(!config_check_and_consume(scanner, '{'))
    return parser_instr_parse(scanner, code);

  if( (alloc_hash = !SCANNER_DATA(scanner)->locals) )
    SCANNER_DATA(scanner)->locals = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, NULL);

  while(!config_check_and_consume(scanner, '}'))
    if( !(result = parser_instr_parse(scanner, code)) )
      break;

  config_check_and_consume(scanner, ';');
  if(alloc_hash)
    g_clear_pointer(&SCANNER_DATA(scanner)->locals, g_hash_table_destroy);

  return result;
}

static gboolean parser_params_parse ( GScanner *scanner, GByteArray *code )
{
  while(config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
  {
    g_hash_table_insert(SCANNER_DATA(scanner)->locals,
        g_strdup(scanner->value.v_identifier),
        GINT_TO_POINTER(g_hash_table_size(SCANNER_DATA(scanner)->locals)+1));
    if(!config_check_and_consume(scanner, ','))
      break;
  }

  return TRUE;
}

gboolean parser_function_build ( GScanner *scanner )
{
  GByteArray *code = code;
  gchar *name;

  if(config_check_and_consume(scanner, '('))
  {
    config_parse_sequence(scanner,
        SEQ_REQ, G_TOKEN_STRING, NULL, &name, "missing function name",
        SEQ_REQ, ')', NULL, NULL, "missing ')' after 'function'",
        SEQ_END);

    if(!scanner->max_parse_errors && name)
      vm_func_add_user(name, parser_action_build(scanner));
    g_free(name);

    return !scanner->max_parse_errors;
  }

  code = parser_new(SCANNER_DATA(scanner)->api2);
  SCANNER_DATA(scanner)->locals = g_hash_table_new_full((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal, g_free, NULL);

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &name,
        "Missing identifier in a function declaration",
      SEQ_REQ, '(', NULL, NULL, "Missing '(' in a function declaration",
      SEQ_OPT, -2, parser_params_parse, code, NULL,
      SEQ_REQ, ')', NULL, NULL, "Missing ')' in function declaration",
      SEQ_END);

  if(!scanner->max_parse_errors && name)
  {
    if(parser_block_parse(scanner, code))
      vm_func_add_user(name, parser_free(code));
    else
      g_byte_array_free(code, TRUE);
  }
  g_free(name);
  g_clear_pointer(&SCANNER_DATA(scanner)->locals, g_hash_table_destroy);

  return !scanner->max_parse_errors;
}

GBytes *parser_exec_build ( gchar *cmd )
{
  GByteArray *code;

  if(!cmd)
    return NULL;
  code = parser_new(TRUE);
  parser_emit_string(code, cmd);
  parser_emit_function(code, vm_func_lookup("exec"), 1);
  return parser_free(code);
}

GBytes *parser_string_build ( gchar *str )
{
  GByteArray *code;

  if(!str)
    return NULL;
  code = parser_new(TRUE);
  parser_emit_string(code, str);
  return parser_free(code);
}

GBytes *parser_function_call_build ( gchar *name )
{
  GByteArray *code;
  vm_function_t *func;

  if( !(func = vm_func_lookup(name)) )
    return NULL;

  code = parser_new(TRUE);
  parser_emit_function(code, func, 0);
  return parser_free(code);
}

GBytes *parser_action_build ( GScanner *scanner )
{
  GByteArray *code;

  code = parser_new(SCANNER_DATA(scanner)->api2);
  if(parser_block_parse(scanner, code))
    return parser_free(code);
  g_byte_array_unref(code);
  return NULL;
}

GBytes *parser_expr_build ( GScanner *scanner )
{
  GByteArray *code;

  code = parser_new(SCANNER_DATA(scanner)->api2);
  if(parser_expr_parse(scanner, code))
    return parser_free(code);
  g_byte_array_unref(code);
  return NULL;
}

void parser_init ( void )
{
  parser_instructions = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(parser_instructions, "var", parser_var_parse);
  config_add_key(parser_instructions, "if", parser_if_instr_parse);
  config_add_key(parser_instructions, "while", parser_while_parse);
  config_add_key(parser_instructions, "return", parser_return_parse);
}
