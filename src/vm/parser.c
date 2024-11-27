
#include "vm/vm.h"

static gboolean parser_expr_parse ( GScanner *scanner, GByteArray *code );

static gint parser_emit_jump ( GByteArray *code, guint8 op )
{
  guint8 data[1+sizeof(gint)];

  data[0] = op;
  g_byte_array_append(code, data, sizeof(gint)+1);
  return code->len - sizeof(gint);
}

static void parser_jump_backpatch ( GByteArray *code, gint olen, gint clen )
{
  *((gint *)(code->data + olen)) = (clen - olen);
}

static void parser_emit_string ( GByteArray *code, gchar *str )
{
  guchar data[sizeof(value_t)+1];
  value_t *value = (value_t *)(data+1);

  data[0] = EXPR_OP_IMMEDIATE;
  value->type = EXPR_TYPE_STRING;
  g_byte_array_append(code, data, sizeof(value_t)+1);
  g_byte_array_append(code, (guint8 *)str, strlen(str)+1);
}

static void parser_emit_numeric ( GByteArray *code, gdouble numeric )
{
  guchar data[sizeof(value_t)+1];
  value_t *value = (value_t *)(data+1);

  data[0] = EXPR_OP_IMMEDIATE;
  value->type = EXPR_TYPE_NUMERIC;
  value->value.numeric = numeric;
  g_byte_array_append(code, data, sizeof(value_t)+1);
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

  /* JZ over len change + JMP instruction (1 + sizeof(gint *)) */
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

static gboolean parser_ident ( GScanner *scanner, GByteArray *code )
{
  gchar name[] = "  ident";

  if(g_scanner_get_next_token(scanner)!='(')
    return FALSE;
  if(g_scanner_get_next_token(scanner)!=G_TOKEN_IDENTIFIER)
    return FALSE;
  parser_emit_string(code, scanner->value.v_identifier);
  name[0] = EXPR_OP_FUNCTION;
  name[1] = 1;
  g_byte_array_append(code, (guint8 *)name, 8);
  if(g_scanner_get_next_token(scanner)!=')')
    return FALSE;
  return TRUE;
}

static gboolean parser_identifier ( GScanner *scanner, GByteArray *code )
{
  gchar *data;
  guint8 np;

  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "if"))
    return parser_if(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "cached"))
    return parser_cached(scanner, code);
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "ident"))
    return parser_ident(scanner, code);
  if(g_scanner_peek_next_token(scanner)=='(')
  {
    data = g_strconcat("  ",scanner->value.v_identifier, NULL);
    g_scanner_get_next_token(scanner);

    np = 0;
    if(g_scanner_peek_next_token(scanner)!=')')
      do
      {
        if(!parser_expr_parse(scanner, code))
        {
          g_free(data);
          return FALSE;
        }
        np++;
      }
      while(g_scanner_get_next_token(scanner)==',' && np<255);
    else
      g_scanner_get_next_token(scanner);

    if(scanner->token != ')')
    {
      g_free(data);
      return FALSE;
    }

    data[0]=EXPR_OP_FUNCTION;
    data[1]=np;
    g_byte_array_append(code, (guint8 *)data, strlen(data+2)+3);
    g_free(data);
    return TRUE;
  }
  else
  {
    data = g_strconcat(" ", scanner->value.v_identifier, NULL);
    data[0] = EXPR_OP_VARIABLE;
    g_byte_array_append(code, (guint8 *)data, strlen(data+1)+2);
    g_free(data);
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
  else if(token == G_TOKEN_XTRUE)
    parser_emit_numeric(code, TRUE);
  else if(token == G_TOKEN_XFALSE)
    parser_emit_numeric(code, FALSE);
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

GByteArray *parser_expr_compile ( gchar *expr )
{
  GScanner *scanner;
  GByteArray *code;

  if(!expr)
    return NULL;

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

  g_scanner_scope_add_symbol(scanner, 0, "True", (gpointer)G_TOKEN_XTRUE );
  g_scanner_scope_add_symbol(scanner, 0, "False", (gpointer)G_TOKEN_XFALSE );
  g_scanner_set_scope(scanner, 0);

  g_scanner_input_text(scanner, expr, strlen(expr));

  code = g_byte_array_new();
  if(!parser_expr_parse(scanner, code))
  {
    g_byte_array_free(code, TRUE);
    code = NULL;
  }
  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy(scanner);

  return code;
}
