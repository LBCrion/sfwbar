/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <glib.h>
#include "expr.h"
#include "sfwbar.h"
#include "wintree.h"
#include "module.h"

static GHashTable *expr_deps;

static gdouble expr_parse_num ( GScanner *scanner, gdouble * );
static gchar *expr_parse_str ( GScanner *scanner, gchar * );
static gchar *expr_parse_root ( GScanner *scanner );

static gdouble expr_str_to_num ( gchar *str )
{
  gdouble val;
 
  if(!str)
    return 0;

  val = strtod(str,NULL);
  g_free(str);
  return val;
}

/* convert a number to a string with specified number of decimals */
gchar *expr_dtostr ( double num, gint dec )
{
  static const gchar *format = "%%0.%df";
  static gchar fbuf[16];
  static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  if(dec<0)
    return g_strdup(g_ascii_dtostr(buf,G_ASCII_DTOSTR_BUF_SIZE,num));

  if(dec>99)
    dec = 99;

  g_snprintf(fbuf,16,format,dec);
  return g_strdup(g_ascii_formatd(buf,G_ASCII_DTOSTR_BUF_SIZE,fbuf,num));
}

static gboolean expr_is_numeric ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if (scanner->next_token == G_TOKEN_FLOAT ||
      scanner->next_token == (gint)G_TOKEN_IDENT ||
      scanner->next_token == '!' ||
      scanner->next_token == '(' ||
      scanner->next_token == '-' ||
      scanner->next_token == (GTokenType)G_TOKEN_LEFT_PAREN)
    return TRUE;

  if(scanner->next_token != G_TOKEN_IDENTIFIER)
    return FALSE;

  if(scanner_is_variable(scanner->next_value.v_identifier))
    return (*(scanner->next_value.v_identifier) != '$');
  if(module_is_function(scanner->next_value.v_identifier))
    return module_check_flag(scanner->next_value.v_identifier,
        MODULE_EXPR_NUMERIC);
  return FALSE;
}

static gboolean expr_is_string ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if(scanner->next_token == G_TOKEN_STRING ||
      scanner->next_token == (GTokenType)G_TOKEN_MAP ||
      scanner->next_token == (GTokenType)G_TOKEN_LOOKUP )
    return TRUE;

  if(scanner->next_token != G_TOKEN_IDENTIFIER)
    return FALSE;

  if(scanner_is_variable(scanner->next_value.v_identifier))
    return (*(scanner->next_value.v_identifier) == '$');
  if(module_is_function(scanner->next_value.v_identifier))
    return !module_check_flag(scanner->next_value.v_identifier,
        MODULE_EXPR_NUMERIC);
  return FALSE;
}

static gboolean expr_is_variant ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if( scanner->next_token == (GTokenType)G_TOKEN_IF ||
      scanner->next_token == (GTokenType)G_TOKEN_CACHED)
    return TRUE;

  if( scanner->next_token != G_TOKEN_IDENTIFIER ||
      scanner_is_variable(scanner->next_value.v_identifier) ||
      module_is_function(scanner->next_value.v_identifier) )
    return FALSE;
  else
    return TRUE;
}

static gboolean parser_expect_symbol ( GScanner *scanner, gint symbol,
    gchar *expr )
{
  gchar *err;

  if(g_scanner_get_next_token(scanner)==symbol)
    return TRUE;

  err = g_strdup_printf("%s [%d:%d]",expr,scanner->line,scanner->position);
  g_scanner_unexp_token(scanner, symbol, NULL, NULL, "", err , TRUE);
  g_free(err);
  return FALSE;
}

static void *expr_parse_identifier ( GScanner *scanner )
{
  gint i;
  gchar *err;

  if(g_scanner_peek_next_token(scanner)!='(' &&
      scanner_is_variable(scanner->value.v_identifier))
    return scanner_get_value(scanner->value.v_identifier,
        !E_STATE(scanner)->ignore,E_STATE(scanner)->expr);

  if(g_scanner_peek_next_token(scanner)=='(' &&
      module_is_function(scanner->value.v_identifier) &&
      !E_STATE(scanner)->ignore)
    return module_get_string(scanner);

  expr_dep_add(scanner->value.v_identifier,E_STATE(scanner)->expr);

  if(g_scanner_peek_next_token(scanner)!='(')
    return g_strdup_printf("Undeclared variable: %s",
        scanner->value.v_identifier);
  else
  {
    err = g_strdup_printf("Unknown Function: %s",scanner->value.v_identifier);
    g_scanner_get_next_token(scanner);
    i=1;
    while(i && !g_scanner_eof(scanner))
      switch((gint)g_scanner_get_next_token(scanner))
      {
        case '(':
          i++;
          break;
        case ')':
          i--;
          break;
      }
    return err;
  }
}

static gchar *expr_parse_map ( GScanner *scanner )
{
  gchar *match, *result, *comp;
  guint istate = E_STATE(scanner)->ignore;

  parser_expect_symbol(scanner,'(',"Map(...)");
  match = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Map(value,...)");
  result = NULL;

  while(scanner->token==',' && expr_is_string(scanner))
  {
    comp = expr_parse_str(scanner,NULL);
    if(g_scanner_peek_next_token(scanner)==')')
    {
      if(!result)
        result = comp;
      break;
    }
    parser_expect_symbol(scanner,',',"Map(... match , string ...)");
    if(!g_strcmp0(comp,match))
      result = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = istate;
    }
    g_free(comp);
    if(g_scanner_peek_next_token(scanner)==',')
      g_scanner_get_next_token(scanner);
  }
  g_free(match);
  parser_expect_symbol(scanner,')',"Map(...)");
  return result;
}

static gchar *expr_parse_lookup ( GScanner *scanner )
{
  gchar *str;
  gdouble value, comp;
  guint istate = E_STATE(scanner)->ignore;

  parser_expect_symbol(scanner,'(',"Lookup(...)");
  value = expr_parse_num ( scanner, NULL );
  parser_expect_symbol(scanner,',',"Lookup(value,...)");
  str = NULL;

  while (scanner->token==',' && expr_is_numeric(scanner))
  {
    comp = expr_parse_num(scanner, NULL);
    parser_expect_symbol(scanner,',',"Lookup(... threshold, value ...)");
    if(comp<value && !str)
      str = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = istate;
    }
    if(g_scanner_peek_next_token(scanner)==',')
      g_scanner_get_next_token(scanner);
  }
  if(scanner->token==',')
  {
    if(!str)
      str = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = istate;
    }
  }

  parser_expect_symbol(scanner,')',"Lookup(...)");

  return str?str:g_strdup("");
}

static gchar *expr_parse_cached ( GScanner *scanner )
{
  gchar *ret;
  gint istate;
  
  parser_expect_symbol(scanner,'(',"Cached(Identifier)");
  istate = E_STATE(scanner)->ignore;
  E_STATE(scanner)->ignore = TRUE;

  ret = expr_parse_root(scanner);

  E_STATE(scanner)->ignore = istate;
  parser_expect_symbol(scanner,')',"Cached(Identifier)");

  return ret;
}
  
static gchar *expr_parse_if ( GScanner *scanner )
{
  gboolean condition;
  gchar *str, *str2;
  guint istate = E_STATE(scanner)->ignore;
  gint rtype, stype = E_STATE(scanner)->type;

  parser_expect_symbol(scanner,'(',"If(Condition,Expression,Expression)");
  condition = expr_parse_num(scanner,NULL);
  E_STATE(scanner)->type = stype;

  if(!condition)
    E_STATE(scanner)->ignore = 1;
  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  g_scanner_peek_next_token(scanner);
  str = expr_parse_root(scanner);

  if(condition)
  {
    E_STATE(scanner)->ignore = 1;
    rtype = E_STATE(scanner)->type;
  }
  else
  {
    E_STATE(scanner)->ignore = istate;
    E_STATE(scanner)->type = stype;
  }

  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  str2 = expr_parse_root(scanner);

  E_STATE(scanner)->ignore = istate;
  parser_expect_symbol(scanner,')',"If(Condition,Expression,Expression)");

  if(condition)
  {
    E_STATE(scanner)->type = rtype;
    g_free(str2);
    return str;
  }
  else
  {
    g_free(str);
    return str2;
  }
}

gdouble expr_parse_ident ( GScanner *scanner )
{
  gdouble result;

  parser_expect_symbol(scanner,'(',"Ident(Identifier)");
  if(!parser_expect_symbol(scanner,G_TOKEN_IDENTIFIER,"Ident(Identifier)"))
    return FALSE;

  result = scanner_is_variable(scanner->value.v_identifier) ||
    module_is_function(scanner->value.v_identifier);
  if(!result)
    expr_dep_add(scanner->value.v_identifier,E_STATE(scanner)->expr);

  parser_expect_symbol(scanner,')',"Ident(iIdentifier)");

  return result;
}

/* compare operator for strigs and variants */
static gdouble expr_parse_compare ( GScanner *scanner, gchar *prev )
{
  gchar *str1, *str2;
  gboolean negate = FALSE;
  gdouble res;

  if(!prev)
    str1 = expr_parse_str(scanner,NULL);
  else
    str1 = prev;

  if(g_scanner_peek_next_token(scanner)=='!')
  {
    negate = TRUE;
    g_scanner_get_next_token(scanner);
  }
  parser_expect_symbol(scanner,'=',"string = string");
  str2 = expr_parse_str(scanner,NULL);

  res = !g_strcmp0(str1,str2);
  if(negate)
    res = !res;

  g_free(str1);
  g_free(str2);

  E_STATE(scanner)->type = EXPR_NUMERIC;
  return res;
}

static gchar *expr_parse_variant_token ( GScanner *scanner )
{
  E_STATE(scanner)->type = EXPR_VARIANT;
  switch((gint)g_scanner_peek_next_token(scanner))
  {
    case G_TOKEN_IF:
      g_scanner_get_next_token(scanner);
      return expr_parse_if(scanner);
    case G_TOKEN_CACHED:
      g_scanner_get_next_token(scanner);
      return expr_parse_cached(scanner);
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token(scanner);
      return expr_parse_identifier(scanner);
    default:
      return g_strdup("");
  }
}

static gchar *expr_parse_variant ( GScanner *scanner )
{
  gchar *str;

  str = expr_parse_variant_token(scanner);

  while(E_STATE(scanner)->type==EXPR_VARIANT &&
       g_scanner_peek_next_token(scanner) == '+')
  {
    g_scanner_get_next_token(scanner);
    g_free(str);
    str = expr_parse_root(scanner);
  }

  if(E_STATE(scanner)->type == EXPR_STRING)
    return expr_parse_str(scanner,str);

  return str;
}

static gchar *expr_parse_str_l1 ( GScanner *scanner )
{
  gchar *str;
  gint spos;

  if(expr_is_variant(scanner))
  {
    spos = scanner->position;
    E_STATE(scanner)->type = EXPR_STRING;
    str = expr_parse_variant_token(scanner);
    if(E_STATE(scanner)->type == EXPR_NUMERIC)
      g_scanner_warn(scanner,
          "Unexpected variant at position %d, expected a string",spos);
    return str;
  }

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_STRING:
      return g_strdup(scanner->value.v_string);
    case G_TOKEN_LOOKUP:
      return expr_parse_lookup ( scanner );
    case G_TOKEN_MAP:
      return expr_parse_map ( scanner );
    case G_TOKEN_IDENTIFIER:
      return expr_parse_identifier(scanner);
    default:
      g_scanner_warn(scanner,
          "Unexpected token %c at position %u, expected a string",scanner->token,
          g_scanner_cur_position(scanner));
      return g_strdup("");
  }
}

static gchar *expr_parse_str ( GScanner *scanner, gchar *prev )
{
  gchar *str,*next,*tmp;

  E_STATE(scanner)->type = EXPR_STRING;
  if(prev)
    str = prev;
  else
    str = expr_parse_str_l1( scanner );

  while(g_scanner_peek_next_token( scanner )=='+' &&
      E_STATE(scanner)->type != EXPR_NUMERIC)
  {
    g_scanner_get_next_token( scanner );
    next = expr_parse_str_l1( scanner );
    tmp = g_strconcat(str,next,NULL);
    g_free(str);
    g_free(next);
    str=tmp;
  }

  E_STATE(scanner)->type = EXPR_STRING;
  return str;
}

static gdouble expr_parse_num_value ( GScanner *scanner, gdouble *prev )
{
  gdouble val, *ptr;
  gchar *str;

  if(prev)
    return *prev;

  if(expr_is_string(scanner))
    return expr_parse_compare(scanner,NULL);

  if(expr_is_variant(scanner))
  {
    E_STATE(scanner)->type = EXPR_NUMERIC;
    str = expr_parse_variant_token(scanner);
    if(E_STATE(scanner)->type == EXPR_NUMERIC)
      return expr_str_to_num(str);
    /* if type is string, assume it's there for comparison */
    else if(E_STATE(scanner)->type == EXPR_STRING ||
        g_scanner_peek_next_token(scanner) == '=' ||
        g_scanner_peek_next_token(scanner) == '!' )
      return expr_parse_compare(scanner,str);
    else /* if the variant type is unresolved, cast to numeric zero */
    {
      E_STATE(scanner)->type = EXPR_NUMERIC;
      return 0;
    }
  }

  switch((gint)g_scanner_get_next_token(scanner) )
  {
    case '+':
      return expr_parse_num_value ( scanner, NULL );
    case '-':
      return -expr_parse_num_value ( scanner, NULL );
    case '!':
      return !expr_parse_num ( scanner, NULL );
    case G_TOKEN_FLOAT: 
      return scanner->value.v_float;
    case G_TOKEN_IDENT:
      return expr_parse_ident(scanner);
    case '(':
      val = expr_parse_num ( scanner, NULL );
      parser_expect_symbol(scanner, ')',"(Number)");
      return val;
    case G_TOKEN_IDENTIFIER:
      ptr = expr_parse_identifier(scanner);
      val = *ptr;
      g_free(ptr);
      return val;
    default:
      g_scanner_warn(scanner,
          "%d: Unexpected token %d (%c), expected a number",
          g_scanner_cur_position(scanner),
          scanner->next_token, scanner->next_token);
      return 0;
  }
}

static gdouble expr_parse_num_factor ( GScanner *scanner, gdouble *prev )
{
  gdouble val;

  val = expr_parse_num_value ( scanner, prev );
  while(strchr("*/%",g_scanner_peek_next_token ( scanner )))
  {
    g_scanner_get_next_token ( scanner );
    if(scanner->token == '*')
      val *= expr_parse_num_value( scanner, NULL );
    if(scanner->token == '/')
      val /= expr_parse_num_value( scanner, NULL );
    if(scanner->token == '%')
      val = (gint)val % (gint)expr_parse_num_value( scanner, NULL );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

static gdouble expr_parse_num_sum ( GScanner *scanner, gdouble *prev )
{
  gdouble val;

  val = expr_parse_num_factor ( scanner, prev );
  while(strchr("+-",g_scanner_peek_next_token( scanner )))
  {
    g_scanner_get_next_token (scanner );
    if(scanner->token == '+')
      val+=expr_parse_num_factor( scanner, NULL );
    if(scanner->token == '-')
      val-=expr_parse_num_factor( scanner, NULL );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

static gdouble expr_parse_num_compare ( GScanner *scanner, gdouble *prev )
{
  gdouble val;

  val = expr_parse_num_sum ( scanner, prev );
  while(strchr("<>=",g_scanner_peek_next_token ( scanner )))
  {
    switch((gint)g_scanner_get_next_token ( scanner ))
    {
      case '>':
        if( g_scanner_peek_next_token( scanner ) == '=' )
        {
          g_scanner_get_next_token( scanner );
          val = (gdouble)(val >= expr_parse_num_sum ( scanner, NULL ));
        }
        else
          val = (gdouble)(val > expr_parse_num_sum ( scanner, NULL ));
        break;
      case '<':
        if( g_scanner_peek_next_token( scanner ) == '=' )
        {
          g_scanner_get_next_token( scanner );
          val = (gdouble)(val <= expr_parse_num_sum ( scanner, NULL ));
        }
        else
          val = (gdouble)(val < expr_parse_num_sum ( scanner, NULL ));
        break;
      case '=':
        val = (gdouble)(val == expr_parse_num_sum ( scanner, NULL ));
        break;
    }
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

static gdouble expr_parse_num( GScanner *scanner, gdouble *prev )
{
  gdouble val;

  E_STATE(scanner)->type = EXPR_NUMERIC;

  val = expr_parse_num_compare ( scanner, prev );

  while(strchr("&|",g_scanner_peek_next_token ( scanner )))
  {
    switch((gint)g_scanner_get_next_token ( scanner ))
    {
      case '&':
        val = expr_parse_num_compare ( scanner, NULL ) && val;
        break;
      case '|':
        val = expr_parse_num_compare ( scanner, NULL ) || val;
        break;
    }
    if(g_scanner_eof(scanner))
      break;
  }
  E_STATE(scanner)->type = EXPR_NUMERIC;
  return val;
}

static gchar *expr_parse_root ( GScanner *scanner )
{
  gchar *str;
  gdouble res;

  if(E_STATE(scanner)->type == EXPR_NUMERIC || expr_is_numeric(scanner))
    return expr_dtostr(expr_parse_num(scanner,NULL),-1);
  else if(E_STATE(scanner)->type == EXPR_STRING || expr_is_string(scanner))
    str = expr_parse_str(scanner,NULL);
  else if(expr_is_variant(scanner))
    str = expr_parse_variant(scanner);
  else
    return g_strdup("");

  if(g_scanner_peek_next_token(scanner)=='=' || scanner->next_token == '!')
  {
    res = expr_parse_compare(scanner,str);
    return expr_dtostr(expr_parse_num(scanner,&res),-1);
  }

  if(E_STATE(scanner)->type != EXPR_STRING)
  {
    res = expr_str_to_num(str);
    return expr_dtostr(expr_parse_num(scanner,&res),-1);
  }

  return str;
}

void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name )
{
  void **params;
  gint i;

  parser_expect_symbol(scanner,'(',name);

  if(!spec)
    params = NULL;
  else
  {
    params = g_malloc0(strlen(spec)*sizeof(gpointer));
    for(i=0;spec[i];i++)
      if(g_scanner_peek_next_token(scanner)!=')')
      {
        if(g_ascii_tolower(spec[i])=='n' && !expr_is_string(scanner))
        {
          params[i] = g_malloc0(sizeof(gdouble));
          *((gdouble *)params[i]) = expr_parse_num(scanner,NULL);
        }
        else if(g_ascii_tolower(spec[i])=='s' && !expr_is_numeric(scanner))
          params[i] = expr_parse_str(scanner,NULL);
        else if(!g_ascii_islower(spec[i]))
          g_scanner_error(scanner,"invalid type in parameter %d of %s",i,name);

        if(params[i] && g_scanner_peek_next_token(scanner)==',')
          g_scanner_get_next_token(scanner);
      }
  }

  parser_expect_symbol(scanner,')',name);
  return params;
}

static GScanner *expr_scanner_new ( void )
{
  GScanner *scanner;

  scanner = g_scanner_new(NULL);
  scanner->config->scan_octal = 0;
  scanner->config->symbol_2_token = 1;
  scanner->config->case_sensitive = 0;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  scanner->config->cset_identifier_nth = g_strconcat(".",
      scanner->config->cset_identifier_nth,NULL);
  scanner->config->cset_identifier_first = g_strconcat("$",
      scanner->config->cset_identifier_first,NULL);

  g_scanner_scope_add_symbol(scanner,0, "If", (gpointer)G_TOKEN_IF );
  g_scanner_scope_add_symbol(scanner,0, "Cached", (gpointer)G_TOKEN_CACHED );
  g_scanner_scope_add_symbol(scanner,0, "Lookup", (gpointer)G_TOKEN_LOOKUP );
  g_scanner_scope_add_symbol(scanner,0, "Map", (gpointer)G_TOKEN_MAP );
  g_scanner_scope_add_symbol(scanner,0, "Ident", (gpointer)G_TOKEN_IDENT );
  g_scanner_set_scope(scanner,0);

  return scanner;
}

gchar *expr_parse( ExprCache *expr )
{
  GScanner *scanner;
  gchar *result;
  ExprState state;

  scanner = expr_scanner_new();

  scanner->input_name = expr->definition;
  scanner->user_data = &state;
  E_STATE(scanner)->expr = expr;
  E_STATE(scanner)->type = EXPR_VARIANT;
  E_STATE(scanner)->error = FALSE;
  E_STATE(scanner)->ignore = FALSE;

  g_scanner_input_text(scanner, expr->definition, strlen(expr->definition));

  result = expr_parse_root(scanner);

  if(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
    g_scanner_error(scanner,
        "Unexpected input at the end of expression [%d:%d]",
        scanner->line,scanner->position);

  g_debug("expr: \"%s\" = \"%s\" (vstate: %d)",expr->definition,result,
      E_STATE(scanner)->expr->vstate);

  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy( scanner );

  return result;
}

gboolean expr_cache_eval ( ExprCache *expr )
{
  gchar *eval;

  if(!expr || !expr->definition || !expr->eval)
    return FALSE;

  expr->vstate = FALSE;
  eval = expr_parse(expr);

  if(!expr->vstate)
    expr->eval = FALSE;

  if(g_strcmp0(eval,expr->cache))
  {
    g_free(expr->cache);
    expr->cache = eval;
    return TRUE;
  }
  else
  {
    g_free(eval);
    return FALSE;
  }
}

ExprCache *expr_cache_new ( void )
{
  return g_malloc0(sizeof(ExprCache));
}

void expr_cache_free ( ExprCache *expr )
{
  if(!expr)
    return;
  g_free(expr->definition);
  g_free(expr->cache);
  g_free(expr);
}

void expr_dep_add ( gchar *ident, ExprCache *expr )
{
  GList *list;
  ExprCache *iter;
  gchar *vname;

  if(!expr_deps)
    expr_deps = g_hash_table_new_full((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal,g_free,NULL);

  vname = scanner_parse_identifier(ident,NULL);
  list = g_hash_table_lookup(expr_deps,vname);
  for(iter=expr;iter;iter=iter->parent)
    if(!g_list_find(list,iter))
      list = g_list_prepend(list,iter);
  g_hash_table_replace(expr_deps,vname,list);
}

void expr_dep_trigger ( gchar *ident )
{
  GList *iter,*list;

  if(!expr_deps)
    return;

  list = g_hash_table_lookup(expr_deps,ident);

  for(iter=list;iter;iter=g_list_next(iter))
    ((ExprCache *)(iter->data))->eval = TRUE;
  g_hash_table_remove(expr_deps,ident);
  g_list_free(list);
}

void expr_dep_dump_each ( void *key, void *value, void *d )
{
  GList *iter;

  for(iter=value;iter;iter=g_list_next(iter))
    g_message("%s: %s", (gchar *)key, ((ExprCache *)iter->data)->definition);
}

void expr_dep_dump ( void )
{
  g_hash_table_foreach(expr_deps,expr_dep_dump_each,NULL);
}
