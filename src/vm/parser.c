
#include "config/config.h"
#include "util/string.h"
#include "vm/vm.h"

static GHashTable *macros;

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
  if(!scanner->user_data || scanner->token!=G_TOKEN_IDENTIFIER)
    return 0;
  return GPOINTER_TO_INT(g_hash_table_lookup(
          scanner->user_data, scanner->value.v_identifier));
}

static gboolean parser_cached ( GScanner *scanner, GByteArray *code )
{
  guchar data[2];

  if(g_scanner_get_next_token(scanner)!='(')
    return FALSE;

  data[0] = EXPR_OP_CACHED;
  data[1] = TRUE;
  g_byte_array_append(code, data, 2);

  if(!parser_expr_parse(scanner, code))
    return FALSE;

  data[1] = FALSE;
  g_byte_array_append(code, data, 2);

  if(g_scanner_get_next_token(scanner)!=')')
    return FALSE;

  return TRUE;
}

static gboolean parser_if ( GScanner *scanner, GByteArray *code )
{
  gint alen;

  if(g_scanner_get_next_token(scanner)!='(')
    return FALSE;
  if(!parser_expr_parse(scanner, code))
    return FALSE;
  if(g_scanner_get_next_token(scanner)!=',')
    return FALSE;

  alen = parser_emit_jump(code, EXPR_OP_JZ);

  if(!parser_expr_parse(scanner, code))
    return FALSE;
  if(g_scanner_get_next_token(scanner)!=',')
    return FALSE;

  /* JZ over len change + JMP instruction (1 + sizeof(gint)) */
  parser_jump_backpatch(code, alen, code->len + sizeof(gint) + 1);
  alen = parser_emit_jump(code, EXPR_OP_JMP);

  if(!parser_expr_parse(scanner, code))
    return FALSE;
  /* JMP over len change */
  parser_jump_backpatch(code, alen, code->len);

  if(g_scanner_get_next_token(scanner)!=')')
    return FALSE;

  return TRUE;
}

static void parser_emit_function ( GByteArray *code, const void *f, guint8 np )
{
  guint8 data[sizeof(gpointer)+2];

  memcpy(data+2, &f, sizeof(gpointer));
  data[0]=EXPR_OP_FUNCTION;
  data[1]=np;
  g_byte_array_append(code, data, sizeof(gpointer)+2);
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

  if(scanner->token != ')')
    return FALSE;

  parser_emit_function(code, ptr, np);
  scanner->config->identifier_2_string = FALSE;

  return TRUE;
}

static gboolean parser_variable ( GScanner *scanner, GByteArray *code )
{
  guint8 data[sizeof(gpointer)+1];
  gconstpointer ptr;
  guint16 pos;

  if( (pos = parser_local_lookup(scanner)) )
  {
    if(config_check_and_consume(scanner, '['))
    {
      parser_emit_local(code, pos, EXPR_OP_LOCAL);
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      if(!config_expect_token(scanner, ']', "Expect ']' after array index"))
        return FALSE;

      parser_emit_function(code, vm_func_lookup("arrayindex"), 2);
    }
    else
      parser_emit_local(code, pos, EXPR_OP_LOCAL);
    return TRUE;
  }

  ptr = parser_identifier_lookup(scanner->value.v_identifier);
  memcpy(data+1, &ptr, sizeof(gpointer));
  data[0] = EXPR_OP_VARIABLE;
  g_byte_array_append(code, data, sizeof(gpointer)+1);

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
  g_byte_array_append(code, data, len);
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

static gboolean parser_list_handle ( GScanner *scanner, GByteArray *code )
{
  guint8 data[sizeof(gpointer)+2], np = 0;
  gconstpointer ptr;

  if(g_scanner_peek_next_token(scanner)!=']')
    do {
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      np++;
    } while(config_check_and_consume(scanner, ','));

  if(!config_expect_token(scanner, ']', "Expected ']' at the end of the list"))
    return FALSE;

  data[0]=EXPR_OP_FUNCTION;
  data[1]=np;
  ptr = vm_func_lookup("arraybuild");
  memcpy(data+2, &ptr, sizeof(gpointer));
  g_byte_array_append(code, data, sizeof(gpointer)+2);

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
    parser_list_handle(scanner, code);
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
    return FALSE;

  return TRUE;
}

static gboolean parser_ops ( GScanner *scanner, GByteArray *code, gint l )
{
  static gchar *expr_ops_list[] = { "&|", "!<>=", "+-", "*/%", NULL };
  static const guchar *ne = (guchar *)"=!", equal = '=';
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

    if(op=='!' && g_scanner_peek_next_token(scanner)!='=')
      return FALSE;
    else if(op=='!')
      g_scanner_get_next_token(scanner);

    if((op=='>' || op=='<') && g_scanner_peek_next_token(scanner)=='=')
    {
      or_equal = TRUE;
      g_scanner_get_next_token(scanner);
    }
    if(!parser_ops(scanner, code, l+1))
      return FALSE;

    if(op!='!')
      g_byte_array_append(code, &op, 1);
    else
      g_byte_array_append(code, ne, 2);

    if(or_equal)
      g_byte_array_append(code, &equal, 1);
  }
  return TRUE;
}

gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code )
{
  return parser_ops(scanner, code, 0);
}

gboolean parser_macro_add ( GScanner *scanner )
{
  GByteArray *code;
  gchar *name;

  if(g_scanner_get_next_token(scanner) != G_TOKEN_IDENTIFIER)
    return FALSE;
  name = g_strdup(scanner->value.v_identifier);
  if(g_scanner_get_next_token(scanner) != '=')
  {
    g_free(name);
    return FALSE;
  }

  code = g_byte_array_new();

  if(!parser_expr_parse(scanner, code))
  {
    g_byte_array_free(code, TRUE);
    g_free(name);
    return FALSE;
  }

  if(!macros)
    macros = g_hash_table_new_full( (GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, (GDestroyNotify)g_bytes_unref);

  g_hash_table_insert(macros, name, g_byte_array_free_to_bytes(code));

  return TRUE;
}

static gboolean parser_assign_parse ( GScanner *scanner, GByteArray *code )
{
  guint16 pos;

  if(!(pos = parser_local_lookup(scanner)) )
    return FALSE;

  if(config_check_and_consume(scanner, '['))
  {
    parser_emit_local(code, pos, EXPR_OP_LOCAL);
    if(!parser_expr_parse(scanner, code))
      return FALSE;
    if(!config_expect_token(scanner, ']', "Expect ']' after array index"))
      return FALSE;
    if(!config_expect_token(scanner, '=', "Expect '=' after a variable"))
      return FALSE;
    if(!parser_expr_parse(scanner, code))
      return FALSE;
    parser_emit_function(code, vm_func_lookup("arrayassign"), 3);
    parser_emit_local(code, pos, EXPR_OP_ASSIGN);
    return TRUE;
  }
  
  if(!config_expect_token(scanner, '=', "Expect '=' after a variable"))
    return FALSE;

  if(!parser_expr_parse(scanner, code))
    return FALSE;

  parser_emit_local(code, pos, EXPR_OP_ASSIGN);
  return TRUE;
}

static gboolean parser_action_parse ( GScanner *scanner, GByteArray *code )
{
  gconstpointer ptr;
  gboolean neg;
  guint8 data[sizeof(gpointer)+2];
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
    if(!config_check_and_consume(scanner, ']'))
      return FALSE;
  }

  if(cond)
  {
    parser_emit_numeric(code, cond & 0xff);
    parser_emit_numeric(code, cond>>8);

    ptr = vm_func_lookup("checkstate");
    memcpy(data+2, &ptr, sizeof(gpointer));
    data[0]=EXPR_OP_FUNCTION;
    data[1]=2;
    g_byte_array_append(code, data, sizeof(gpointer)+2);
    alen = parser_emit_jump(code, EXPR_OP_JZ);
  }

  if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
    ptr = vm_func_lookup("exec");
  else if(config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
    ptr = vm_func_lookup(scanner->value.v_identifier);
  else
  {
    g_message("action name missing");
    g_scanner_get_next_token(scanner);
    return FALSE;
  }

  if(!cond && parser_local_lookup(scanner))
    return parser_assign_parse(scanner, code);
/*  if(!cond && scanner->token==G_TOKEN_IDENTIFIER &&
      g_scanner_peek_next_token(scanner)=='=')
    return parser_assign_parse(scanner, code);*/

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

  config_check_and_consume(scanner, ';');

  memcpy(data+2, &ptr, sizeof(gpointer));
  data[0]=EXPR_OP_FUNCTION;
  data[1]=np;
  g_byte_array_append(code, data, sizeof(gpointer)+2);

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

static gboolean parser_var_decl_parse ( GScanner *scanner, GByteArray *code )
{
  guint16 pos = g_hash_table_size(scanner->user_data) + 1;

  while(config_check_identifier(scanner, "var"))
  {
    g_scanner_get_next_token(scanner);
    do
    {
      if(!config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
        return FALSE;
      parser_emit_na(code);
      g_hash_table_insert(scanner->user_data,
          g_strdup(scanner->value.v_identifier), GUINT_TO_POINTER(pos++));

    } while(config_check_and_consume(scanner, ','));
    config_check_and_consume(scanner, ';');
  }

  return TRUE;
}

static gboolean parser_block_parse_in ( GScanner *scanner, GByteArray *code )
{
  guint8 return_op = EXPR_OP_RETURN;
  gint alen, rlen;

  while(!config_check_and_consume(scanner, '}'))
    if(config_check_identifier(scanner, "if"))
    {
      g_scanner_get_next_token(scanner);
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      alen = parser_emit_jump(code, EXPR_OP_JZ);
      if(!parser_block_parse(scanner, code, FALSE))
        return FALSE;
      if(config_check_identifier(scanner, "else"))
      {
        g_scanner_get_next_token(scanner);
        parser_jump_backpatch(code, alen, code->len + sizeof(gint) + 1);
        alen = parser_emit_jump(code, EXPR_OP_JMP);
        if(!parser_block_parse(scanner, code, FALSE))
          return FALSE;
        parser_jump_backpatch(code, alen, code->len);
      }
      else
        parser_jump_backpatch(code, alen, code->len);
    }
    else if(config_check_identifier(scanner, "while"))
    {
      g_scanner_get_next_token(scanner);
      rlen = code->len;
      if(!parser_expr_parse(scanner, code))
        return FALSE;
      alen = parser_emit_jump(code, EXPR_OP_JZ);
      if(!parser_block_parse(scanner, code, FALSE))
        return FALSE;
      parser_emit_jump(code, EXPR_OP_JMP);
      parser_jump_backpatch(code, code->len - sizeof(int), rlen);
      parser_jump_backpatch(code, alen, code->len);
    }
    else if(config_check_identifier(scanner, "return"))
    {
      g_scanner_get_next_token(scanner);
      if(config_check_and_consume(scanner, ';'))
        parser_emit_na(code);
      else
      {
        if(!parser_expr_parse(scanner, code))
          return FALSE;
        config_check_and_consume(scanner, ';');
      }
      g_byte_array_append(code, &return_op, 1);
    }
    else if(!parser_block_parse(scanner, code, FALSE))
      return FALSE;

  return TRUE;
}

gboolean parser_block_parse ( GScanner *scanner, GByteArray *code,
    gboolean add_vars )
{
  gboolean ret;

  if(!config_check_and_consume(scanner, '{'))
    return parser_action_parse(scanner, code);

  scanner->user_data = scanner->user_data? scanner->user_data :
    g_hash_table_new_full( (GHashFunc)str_nhash, (GEqualFunc)str_nequal,
        g_free, NULL);

  if(add_vars)
    parser_var_decl_parse(scanner, code);

  ret = parser_block_parse_in(scanner, code);

  if(add_vars)
    g_clear_pointer(&scanner->user_data, g_hash_table_destroy);

  return ret;
}

gboolean parser_function_parse( GScanner *scanner )
{
  GByteArray *acode;
  GHashTable *inputs;
  gchar *name;
  guint16 n = 1;

  if(!config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
    return FALSE;

  name = g_strdup(scanner->value.v_identifier);

  if(!config_check_and_consume(scanner, '('))
  {
    g_free(name);
    return FALSE;
  }
  inputs = g_hash_table_new_full( (GHashFunc)str_nhash, (GEqualFunc)str_nequal,
        g_free, NULL);

  while (config_check_and_consume(scanner, G_TOKEN_IDENTIFIER)) {
      g_hash_table_insert(inputs, g_strdup(scanner->value.v_identifier),
            GINT_TO_POINTER(n));
    if(!config_check_and_consume(scanner, ','))
      break;
  }

  acode = g_byte_array_new();
  scanner->user_data = inputs;

  if(!config_check_and_consume(scanner, ')'))
   return FALSE;
  if(
      !parser_block_parse(scanner, acode, TRUE))
  {
    g_free(name);
    g_byte_array_unref(acode);
    return FALSE;
  }

  vm_func_add_user(name, g_byte_array_free_to_bytes(acode));

  g_free(name);

  return TRUE;
}
