
#include "vm/vm.h"

static gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code );

static gint parser_emit_jump ( GByteArray *code, guint8 op )
{
  guint8 data[1+sizeof(gint)];

  data[0] = op;
  g_byte_array_append(code, data, sizeof(gint)+1);

  return code->len - sizeof(gint);
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
  value_t *value = (value_t *)(data+1);

  data[0] = EXPR_OP_IMMEDIATE;
  *value = value_new_numeric(numeric);
  g_byte_array_append(code, data, sizeof(value_t)+1);
}

static void parser_emit_na ( GByteArray *code )
{
  guchar data[sizeof(value_t)+1];
  value_t *value = (value_t *)(data+1);

  data[0] = EXPR_OP_IMMEDIATE;
  *value = value_na;
  g_byte_array_append(code, data, sizeof(value_t)+1);
}

static void parser_jump_backpatch ( GByteArray *code, gint olen, gint clen )
{
  gint data = clen - olen;
  memcpy(code->data + olen, &data, sizeof(gint));
}

static const gchar *parser_identifier_lookup ( gchar *identifier )
{
  gchar *lower;
  const gchar *result;

  lower = g_ascii_strdown(identifier, -1);
  result = g_intern_string(lower);
  g_free(lower);

  return result;
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
  parser_jump_backpatch(code, alen, code->len + 1);
  alen = parser_emit_jump(code, EXPR_OP_JMP);

  if(!parser_expr_parse(scanner, code))
    return FALSE;
  /* JMP over len change */
  parser_jump_backpatch(code, alen, code->len - sizeof(gint));

  if(g_scanner_get_next_token(scanner)!=')')
    return FALSE;

  return TRUE;
}

static gboolean parser_identifier ( GScanner *scanner, GByteArray *code )
{
  guint8 np, data[sizeof(gpointer)+2];
  gconstpointer ptr;

  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "if"))
    return parser_if(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "cached"))
    return parser_cached(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "true"))
  {
    parser_emit_numeric(code, TRUE);
    return TRUE;
  }
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "false"))
  {
    parser_emit_numeric(code, FALSE);
    return TRUE;
  }
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "ident"))
    scanner->config->identifier_2_string = TRUE;
  if(g_scanner_peek_next_token(scanner)=='(')
  {
    ptr = parser_identifier_lookup(scanner->value.v_identifier);
    memcpy(data+2, &ptr, sizeof(gpointer));
    g_scanner_get_next_token(scanner);

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

    data[0]=EXPR_OP_FUNCTION;
    data[1]=np;
    g_byte_array_append(code, data, sizeof(gpointer)+2);
    scanner->config->identifier_2_string = FALSE;
    return TRUE;
  }
  else
  {
    ptr = parser_identifier_lookup(scanner->value.v_identifier);
    memcpy(data+1, &ptr, sizeof(gpointer));
    data[0] = EXPR_OP_VARIABLE;
    g_byte_array_append(code, data, sizeof(gpointer)+1);
    scanner->config->identifier_2_string = FALSE;
    return TRUE;
  }
  return FALSE;
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
  static const gchar *ne = "=!";
  guchar op;

  if(!expr_ops_list[l])
    return parser_value(scanner, code);

  if(!parser_ops(scanner, code, l+1))
    return FALSE;
  while(strchr(expr_ops_list[l], g_scanner_peek_next_token(scanner)))
  {
    if(g_scanner_eof(scanner))
      return TRUE;
    if( !(op = g_scanner_get_next_token(scanner)) )
      return TRUE;

    if(op=='!' && g_scanner_peek_next_token(scanner)!='=')
      return FALSE;
    else if(op=='!')
      g_scanner_get_next_token(scanner);

    if(!parser_ops(scanner, code, l+1))
      return FALSE;

    if(op!='!')
      g_byte_array_append(code, &op, 1);
    else
      g_byte_array_append(code, (guchar *)ne, 2);
  }
  return TRUE;
}

static gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code )
{
  return parser_ops(scanner, code, 0);
}

static GScanner *parser_scanner_new ( void )
{
  GScanner *scanner;

  scanner = g_scanner_new(NULL);
  scanner->config->scan_octal = 0;
  scanner->config->scan_identifier_1char = 1;
  scanner->config->symbol_2_token = 1;
  scanner->config->case_sensitive = 0;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  scanner->config->cset_identifier_nth = g_strconcat(".",
      scanner->config->cset_identifier_nth, NULL);
  scanner->config->cset_identifier_first = g_strconcat("$",
      scanner->config->cset_identifier_first, NULL);
  g_scanner_set_scope(scanner, 0);

  return scanner;
}

static void parser_scanner_free ( GScanner *scanner )
{
  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy(scanner);
}

GByteArray *parser_expr_compile ( gchar *expr )
{
  GScanner *scanner;
  GByteArray *code;

  if(!expr)
    return NULL;

  scanner = parser_scanner_new();
  g_scanner_input_text(scanner, expr, strlen(expr));

  code = g_byte_array_new();
  if(!parser_expr_parse(scanner, code))
  {
    g_byte_array_free(code, TRUE);
    code = NULL;
  }

  parser_scanner_free(scanner);

  return code;
}

GByteArray *parser_action_compat ( gchar *action, gchar *expr1, gchar *expr2,
    guint16 cond, guint16 ncond )
{
  gint np = !!expr1 + !!expr2;
  GByteArray *code;
  GScanner *scanner;
  guint8 data[sizeof(gpointer)+2];
  gconstpointer ptr;
  gint alen;

  code = g_byte_array_new();

  if(cond || ncond)
  {
    parser_emit_numeric(code, cond);
    parser_emit_numeric(code, ncond);

    ptr = parser_identifier_lookup("checkstate");
    memcpy(data+2, &ptr, sizeof(gpointer));
    data[0]=EXPR_OP_FUNCTION;
    data[1]=2;
    g_byte_array_append(code, data, sizeof(gpointer)+2);
    alen = parser_emit_jump(code, EXPR_OP_JZ);
  }

  if(expr1)
  {
    scanner = parser_scanner_new();
    g_scanner_input_text(scanner, expr1, strlen(expr1));
    if(!parser_expr_parse(scanner, code))
    {
      g_byte_array_free(code, TRUE);
      parser_scanner_free(scanner);
      return NULL;
    }
    parser_scanner_free(scanner);
  }
  if(expr2)
  {
    scanner = parser_scanner_new();
    g_scanner_input_text(scanner, expr2, strlen(expr2));
    if(!parser_expr_parse(scanner, code))
    {
      g_byte_array_free(code, TRUE);
      parser_scanner_free(scanner);
      return NULL;
    }
    parser_scanner_free(scanner);
  }

  ptr = parser_identifier_lookup(action);
  memcpy(data+2, &ptr, sizeof(gpointer));
  data[0]=EXPR_OP_FUNCTION;
  data[1]=np;
  g_byte_array_append(code, data, sizeof(gpointer)+2);

  if(cond || ncond)
  {
    parser_jump_backpatch(code, alen, code->len + 1);
    alen = parser_emit_jump(code, EXPR_OP_JMP);
    parser_emit_na(code);
    parser_jump_backpatch(code, alen, code->len + sizeof(gint));
  }

  return code;
}
