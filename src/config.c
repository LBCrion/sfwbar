/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <fcntl.h>
#include <sys/stat.h>
#include "sfwbar.h"
#include "config.h"
#include "bar.h"
#include "button.h"
#include "scale.h"
#include "image.h"
#include "label.h"
#include "cchart.h"
#include "grid.h"
#include "taskbar.h"
#include "pager.h"
#include "tray.h"
#include "switcher.h"
#include "action.h"
#include "menu.h"
#include "sway_ipc.h"

typedef gboolean (*parse_func) ( GScanner *, void * );
void config_widget ( GScanner *scanner, GtkWidget *widget );

static GHashTable *defines;

void config_log_error ( GScanner *scanner, gchar *message, gboolean error )
{
  if(error)
  {
    if(!scanner->max_parse_errors)
      g_message("%s:%d: %s",scanner->input_name,scanner->line,message);
    scanner->max_parse_errors = TRUE;
  }
  else
    g_message("%s:%d: %s",scanner->input_name,scanner->line,message);
}

gboolean config_expect_token ( GScanner *scanner, gint token, gchar *fmt, ...)
{
  gchar *errmsg;
  va_list args;

  if( g_scanner_peek_next_token(scanner) == token )
    return TRUE;
 
  va_start(args,fmt);
  errmsg = g_strdup_vprintf(fmt,args);
  va_end(args);
  g_scanner_error(scanner,"%s",errmsg);
  g_free(errmsg);

  return FALSE;
}

void config_optional_semicolon ( GScanner *scanner )
{
  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);
}

enum {
  SEQ_OPT,
  SEQ_CON,
  SEQ_REQ,
  SEQ_END
};

void config_parse_sequence ( GScanner *scanner, ... )
{
  va_list args;
  void *dest;
  parse_func func;
  gchar *err;
  gint type;
  gint req;
  gboolean matched = TRUE;

  scanner->max_parse_errors = FALSE;
  va_start(args,scanner);
  req = va_arg(args, gint );
  while(req!=SEQ_END)
  {
    type = va_arg(args, gint );
    func = va_arg(args, parse_func );
    dest  = va_arg(args, void * );
    err = va_arg(args, char * );
    if( (type < 0 && (matched || req != SEQ_CON)) || 
        g_scanner_peek_next_token(scanner) == type || 
        ( scanner->next_token == G_TOKEN_FLOAT && type == G_TOKEN_INT) )
    {
      if(type != -2)
        g_scanner_get_next_token(scanner);
      else
        if(func && !func(scanner,dest))
          if(err)
            g_scanner_error(scanner,"%s",err);

      matched = TRUE;
      if(dest)
        switch(type)
        {
          case G_TOKEN_STRING:
            *((gchar **)dest) = g_strdup(scanner->value.v_string);
            break;
          case G_TOKEN_IDENTIFIER:
            *((gchar **)dest) = g_strdup(scanner->value.v_identifier);
            break;
          case G_TOKEN_FLOAT:
            *((gdouble *)dest) = scanner->value.v_float;
            break;
          case G_TOKEN_INT:
            *((gint *)dest) = (gint)scanner->value.v_float;
            break;
          case -1:
            *((gint *)dest) = scanner->token;
            break;
          case -2:
            break;
          default:
            *((gboolean *)dest) = TRUE;
            break;
        }
    }
    else
      if(req == SEQ_OPT || (req == SEQ_CON && !matched))
        matched = FALSE;
      else
        g_scanner_error(scanner,"%s",err);
    req = va_arg(args, gint );
  }
  va_end(args);
}

void config_parse_dict ( GScanner *scanner, ... )
{
  va_list args;
  parse_func func;
  void *dest;
  gint match = TRUE;
  gint type;

  while(match)
  {
    g_scanner_peek_next_token(scanner);
    va_start(args,scanner);
    type = va_arg(args, gint );
    match = FALSE;
    while( type >= 0 )
    {
      func = va_arg(args, parse_func);
      dest = va_arg(args, void *);
      if(type == scanner->next_token)
      {
        g_scanner_get_next_token(scanner);
        if(func(scanner,dest))
          match = TRUE;
      }
      type = va_arg(args, gint );
    }
    va_end(args);
  }
}

gboolean config_assign_boolean (GScanner *scanner, gboolean def, gchar *expr)
{
  gboolean result = def;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <boolean>",expr))
    return FALSE;
  g_scanner_get_next_token(scanner);

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_TRUE:
      result = TRUE;
      break;
    case G_TOKEN_FALSE:
      result = FALSE;
      break;
    default:
      g_scanner_error(scanner, "Missing <boolean> in %s = <boolean>", expr);
      break;
  }

  config_optional_semicolon(scanner);

  return result;
}

gchar *config_assign_string ( GScanner *scanner, gchar *expr )
{
  gchar *result;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <string>",expr))
    return NULL;

  g_scanner_get_next_token(scanner);

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing <string> in %s = <string>",expr))
    return NULL;

  g_scanner_get_next_token(scanner);

  result = g_strdup(scanner->value.v_string);

  config_optional_semicolon(scanner);

  return result;
}

gdouble config_assign_number ( GScanner *scanner, gchar *expr )
{
  gdouble result;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <number>",expr))
    return 0;

  g_scanner_get_next_token(scanner);

  if(!config_expect_token(scanner, G_TOKEN_FLOAT,
        "Missing <number> in %s = <number>",expr))
    return 0;
  g_scanner_get_next_token(scanner);

  result = scanner->value.v_float;

  config_optional_semicolon(scanner);

  return result;
}

gboolean config_var_flag ( GScanner *scanner, gint *flag )
{
  if(((gint)g_scanner_peek_next_token(scanner) >= G_TOKEN_SUM) &&
    ((gint)(scanner->next_token) <= G_TOKEN_FIRST))
    *flag = g_scanner_get_next_token(scanner);
  return TRUE;
}

gboolean config_var_type (GScanner *scanner, gint *type )
{
  gint token = g_scanner_get_next_token(scanner);
  if(token == G_TOKEN_REGEX || token == G_TOKEN_JSON || token == G_TOKEN_GRAB)
    *type = token;
  else
    g_scanner_error(scanner,"invalid parser");
  return !scanner->max_parse_errors;
}

void config_var ( GScanner *scanner, ScanFile *file )
{
  gchar *vname = NULL, *pattern = NULL;
  guint type;
  gint flag = G_TOKEN_LASTW;

  config_parse_sequence(scanner,
      SEQ_REQ,G_TOKEN_IDENTIFIER,NULL,&vname,NULL,
      SEQ_REQ,'=',NULL,NULL,"Missing '=' in variable declaration",
      SEQ_REQ,-2,(parse_func)config_var_type,&type,NULL,
      SEQ_REQ,'(',NULL,NULL,"Missing '(' after parser",
      SEQ_END);

  if(scanner->max_parse_errors)
    return g_free(vname);

  switch(type)
  {
    case G_TOKEN_REGEX:
    case G_TOKEN_JSON:
      config_parse_sequence(scanner,
          SEQ_REQ,G_TOKEN_STRING,NULL,&pattern,"Missing pattern in parser",
          SEQ_OPT,',',NULL,NULL,NULL,
          SEQ_CON,-2,(parse_func)config_var_flag,&flag,NULL,
          SEQ_REQ,')',NULL,NULL,"Missing ')' after parser",
          SEQ_OPT,';',NULL,NULL,NULL,
          SEQ_END);
      break;
    case G_TOKEN_GRAB:
      config_parse_sequence(scanner,
          SEQ_OPT,-2,(parse_func)config_var_flag,&flag,NULL,
          SEQ_REQ,')',NULL,NULL,"Missing ')' after parser",
          SEQ_OPT,';',NULL,NULL,NULL,
          SEQ_END);
      break;
    default:
      g_scanner_error(scanner,"invalid parser for variable %s",vname);
  }

  if(scanner->max_parse_errors)
  {
    g_free(vname);
    g_free(pattern);
    return;
  }

  scanner_var_attach(vname,file,pattern,type,flag);
}

gboolean config_source_flags ( GScanner *scanner, gint *flags )
{
  while ( g_scanner_peek_next_token(scanner) == ',' )
  {
    g_scanner_get_next_token(scanner);
    g_scanner_get_next_token(scanner);
    if((gint)scanner->token == G_TOKEN_NOGLOB)
      *flags |= VF_NOGLOB;
    else if((gint)scanner->token == G_TOKEN_CHTIME)
      *flags |= VF_CHTIME;
    else
        g_scanner_error(scanner, "invalid flag in source");
  }
  return !scanner->max_parse_errors;
}

ScanFile *config_source ( GScanner *scanner, gint source )
{
  ScanFile *file;
  gchar *fname = NULL, *trigger = NULL;
  gint flags = 0;

  switch(source)
  {
    case SO_FILE:
      config_parse_sequence(scanner,
          SEQ_REQ,'(',NULL,NULL,"Missing '(' after source",
          SEQ_REQ,G_TOKEN_STRING,NULL,&fname,"Missing file in a source",
          SEQ_OPT,-2,(parse_func)config_source_flags,&flags,NULL,
          SEQ_REQ,')',NULL,NULL,"Missing ')' after source",
          SEQ_REQ,'{',NULL,NULL,"Missing '{' after source",
          SEQ_END);
      break;
    case SO_CLIENT:
      config_parse_sequence(scanner,
          SEQ_REQ,'(',NULL,NULL,"Missing '(' after source",
          SEQ_REQ,G_TOKEN_STRING,NULL,&fname,"Missing file in a source",
          SEQ_OPT,',',NULL,NULL,NULL,
          SEQ_CON,G_TOKEN_STRING,NULL,&trigger,NULL,
          SEQ_REQ,')',NULL,NULL,"Missing ')' after source",
          SEQ_REQ,'{',NULL,NULL,"Missing '{' after source",
          SEQ_END);
      break;
    default:
      config_parse_sequence(scanner,
          SEQ_REQ,'(',NULL,NULL,"Missing '(' after source",
          SEQ_REQ,G_TOKEN_STRING,NULL,&fname,"Missing file in a source",
          SEQ_REQ,')',NULL,NULL,"Missing ')' after source",
          SEQ_REQ,'{',NULL,NULL,"Missing '{' after source",
          SEQ_END);
      break;
  }

  if(scanner->max_parse_errors)
  {
    g_free(fname);
    g_free(trigger);
    return NULL;
  }

  file = scanner_file_new ( source, fname, trigger, flags );

  while(g_scanner_peek_next_token(scanner) == G_TOKEN_IDENTIFIER)
    config_var(scanner, file);

  config_parse_sequence(scanner,
      SEQ_REQ,'}',NULL,NULL,"Expecting a variable declaration or '}'",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);

  return file;
}

void config_scanner ( GScanner *scanner )
{
  ScanFile *file;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{', "Missing '{' after 'scanner'"))
    return;
  g_scanner_get_next_token(scanner);

  while(((gint)g_scanner_peek_next_token(scanner) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_FILE:
        config_source(scanner,SO_FILE);
        break;
      case G_TOKEN_EXEC:
        config_source(scanner,SO_EXEC);
        break;
      case G_TOKEN_MPDCLIENT:
        file = config_source(scanner,SO_CLIENT);
        mpd_ipc_init(file);
        break;
      case G_TOKEN_SWAYCLIENT:
        file = config_source(scanner,SO_CLIENT);
        sway_ipc_client_init(file);
        break;
      case G_TOKEN_EXECCLIENT:
        file = config_source(scanner,SO_CLIENT);
        client_exec(file);
        break;
      case G_TOKEN_SOCKETCLIENT:
        file = config_source(scanner,SO_CLIENT);
        client_socket(file);
        break;
      default:
        g_scanner_error(scanner, "Unexpected declaration in scanner");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

struct rect config_get_loc ( GScanner *scanner )
{
  struct rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.w = 1;
  rect.h = 1;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' afer loc",
      SEQ_REQ,G_TOKEN_INT,NULL,&rect.x,"missing x value in loc",
      SEQ_REQ,',',NULL,NULL,"missing comma afer x value in loc",
      SEQ_REQ,G_TOKEN_INT,NULL,&rect.y,"missing y value in loc",
      SEQ_OPT,',',NULL,NULL,NULL,
      SEQ_CON,G_TOKEN_INT,NULL,&rect.w,"missing w value in loc",
      SEQ_OPT,',',NULL,NULL,NULL,
      SEQ_CON,G_TOKEN_INT,NULL,&rect.h,"missing h value in loc",
      SEQ_REQ,')',NULL,NULL,"missing ')' in loc statement",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END );

  return rect;
}

gchar *config_value_string ( gchar *dest, gchar *string )
{
  gint i,j=0,l;
  gchar *result;

  l = strlen(dest);

  for(i=0;string[i];i++)
    if(string[i] == '"')
      j++;
  result = g_malloc(l+i+j+3);
  memcpy(result,dest,l);

  result[l++]='"';
  for(i=0;string[i];i++)
  {
    if(string[i] == '"')
      result[l++]='\\';
    result[l++] = string[i];
  }
  result[l++]='"';
  result[l]=0;

  g_free(dest);
  return result;
}

gchar *config_get_value ( GScanner *scanner, gchar *prop, gboolean assign,
    gchar **id )
{
  gchar *value, *temp;
  static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  scanner->max_parse_errors = FALSE;
  if(assign)
  {
    if(!config_expect_token(scanner, '=',"expecting %s = expression",prop))
      return NULL;
    g_scanner_get_next_token(scanner);
  }
  if(id && g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
  {
    g_scanner_get_next_token(scanner);
    temp = g_strdup(scanner->value.v_string);
    if(g_scanner_peek_next_token(scanner)==',')
    {
      g_scanner_get_next_token(scanner);
      value = g_strdup("");;
      *id = temp;
    }
    else
    {
      value = config_value_string(g_strdup(""),temp);
      g_free(temp);
    }
  }
  else
    value = g_strdup("");;
  g_scanner_peek_next_token(scanner);
  while(((gint)scanner->next_token<=G_TOKEN_SCANNER)&&
      (scanner->next_token!='}')&&
      (scanner->next_token!=';')&&
      (scanner->next_token!='[')&&
      (scanner->next_token!=G_TOKEN_EOF))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_STRING:
        value = config_value_string(value, scanner->value.v_string);
        break;
      case G_TOKEN_IDENTIFIER:
        temp = value;
        if(defines&&g_hash_table_contains(defines,scanner->value.v_identifier))
          value = g_strconcat(value, 
              g_hash_table_lookup(defines,scanner->value.v_identifier), NULL);
        else
          value = g_strconcat(value, scanner->value.v_identifier, NULL);
        g_free(temp);
        break;
      case G_TOKEN_FLOAT:
        temp = value;
        value = g_strconcat(temp,g_ascii_dtostr(buf,G_ASCII_DTOSTR_BUF_SIZE,
              scanner->value.v_float),NULL);
        g_free(temp);
        break;
      default:
        temp = value;
        buf[0] = scanner->token;
        buf[1] = 0;
        value = g_strconcat(temp,buf,NULL);
        g_free(temp);
        break;
    }
    g_scanner_peek_next_token(scanner);
  }
  config_optional_semicolon(scanner);
  return value;
}

void config_get_pins ( GScanner *scanner, GtkWidget *widget )
{
  scanner->max_parse_errors = FALSE;

  if(!IS_PAGER(widget))
    return g_scanner_error(scanner,"this widget has no property 'pins'");

  if(!config_expect_token(scanner, '=',"expecting pins = string [,string]"))
    return;

  do
  {
    g_scanner_get_next_token(scanner);
    if(!config_expect_token(scanner, G_TOKEN_STRING,
          "expecting a string in pins = string [,string]"))
      break;
    g_scanner_get_next_token(scanner);
    pager_add_pin(widget,g_strdup(scanner->value.v_string));
  } while ( g_scanner_peek_next_token(scanner)==',');
  config_optional_semicolon(scanner);
}

void config_action_conditions ( GScanner *scanner, guchar *cond,
    guchar *ncond )
{
  guchar *ptr;

  if(g_scanner_peek_next_token(scanner) != '[')
    return;

  do
  {
    g_scanner_get_next_token(scanner);

    if(g_scanner_peek_next_token(scanner)=='!')
    {
      g_scanner_get_next_token(scanner);
      ptr = ncond;
    }
    else
      ptr = cond;

    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_FOCUSED:
        *ptr |= WS_FOCUSED;
        break;
      case G_TOKEN_MINIMIZED:
        *ptr |= WS_MINIMIZED;
        break;
      case G_TOKEN_MAXIMIZED:
        *ptr |= WS_MAXIMIZED;
        break;
      case G_TOKEN_FULLSCREEN:
        *ptr |= WS_FULLSCREEN;
        break;
      case G_TOKEN_IDLEINHIBIT:
        *ptr |= WS_INHIBIT;
        break;
      case G_TOKEN_USERSTATE:
        *ptr |= WS_USERSTATE;
        break;
      default:
        g_scanner_error(scanner,"invalid condition in action");
        break;
    }
  } while (g_scanner_peek_next_token(scanner)=='|');
  if(g_scanner_get_next_token(scanner) != ']')
    g_scanner_error(scanner,"missing ']' in conditional action");
}

action_t *config_action ( GScanner *scanner )
{
  action_t *action;

  action = g_malloc0(sizeof(action_t));
  config_action_conditions ( scanner, &action->cond, &action->ncond );

  g_scanner_get_next_token(scanner);
  action->type = scanner->token;
  switch ((gint)scanner->token)
  {
    case G_TOKEN_STRING:
      action->command = g_strdup(scanner->value.v_string);
      action->type = G_TOKEN_EXEC;
      break;
    case G_TOKEN_FOCUS:
    case G_TOKEN_CLOSE:
    case G_TOKEN_MINIMIZE:
    case G_TOKEN_MAXIMIZE:
    case G_TOKEN_UNMINIMIZE:
    case G_TOKEN_UNMAXIMIZE:
      break;
    case G_TOKEN_EXEC:
    case G_TOKEN_MENU:
    case G_TOKEN_MENUCLEAR:
    case G_TOKEN_PIPEREAD:
    case G_TOKEN_SWAYCMD:
    case G_TOKEN_SWAYWIN:
    case G_TOKEN_MPDCMD:
    case G_TOKEN_IDLEINHIBIT:
    case G_TOKEN_USERSTATE:
    case G_TOKEN_CONFIG:
    case G_TOKEN_FUNCTION:
    case G_TOKEN_SETBARID:
    case G_TOKEN_SETMONITOR:
    case G_TOKEN_SETLAYER:
    case G_TOKEN_SETBARSIZE:
    case G_TOKEN_SETEXCLUSIVEZONE:
      config_parse_sequence(scanner,
          SEQ_REQ,G_TOKEN_STRING,NULL,&action->addr,"Missing argument in action",
          SEQ_OPT,',',NULL,NULL,NULL,
          SEQ_CON,G_TOKEN_STRING,NULL,&action->command,"Missing argument after ','",
          SEQ_END);
      if(!action->command)
      {
        action->command = action->addr;
        action->addr = NULL;
      }
      break;
    case G_TOKEN_CLIENTSEND:
      config_parse_sequence(scanner,
          SEQ_REQ,G_TOKEN_STRING,NULL,&action->addr,"Missing address in action",
          SEQ_OPT,',',NULL,NULL,NULL,
          SEQ_CON,G_TOKEN_STRING,NULL,&action->command,"Missing command in action",
          SEQ_END);
      break;
    case G_TOKEN_SETVALUE:
      action->command = config_get_value(scanner,"action value",FALSE,
          &action->addr);
      break;
    case G_TOKEN_SETSTYLE:
      action->command = config_get_value(scanner,"action style",FALSE,
          &action->addr);
      break;
    case G_TOKEN_SETTOOLTIP:
      action->command = config_get_value(scanner,"action tooltip",FALSE,
          &action->addr);
      break;
    default:
      g_scanner_error(scanner,"invalid action");
      break;
  }
  if(scanner->max_parse_errors)
  {
    action_free(action,NULL);
    return NULL;
  }

  return action;
}

void config_widget_action ( GScanner *scanner, GtkWidget *widget )
{
  gint button = 1;

  config_parse_sequence(scanner,
    SEQ_OPT,'[',NULL,NULL,NULL,
    SEQ_CON,G_TOKEN_INT,NULL,&button,"missing in action[<index>]",
    SEQ_CON,']',NULL,NULL,"missing closing ']' in action[<index>]",
    SEQ_REQ,'=',NULL,NULL,"missing '=' after action",
    SEQ_END);

  if(scanner->max_parse_errors)
    return;

  if( button<0 || button >=WIDGET_MAX_BUTTON )
    return g_scanner_error(scanner,"invalid action index %d",button);

  base_widget_set_action(widget,button,config_action(scanner));
  if(!base_widget_get_action(widget,button))
    return g_scanner_error(scanner,"invalid action");

  config_optional_semicolon(scanner);
}

GtkWidget *config_include ( GScanner *scanner )
{
  GtkWidget *widget;
  gchar *fname = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"Missing '(' after include",
      SEQ_REQ,G_TOKEN_STRING,NULL,&fname,"Missing filename in include",
      SEQ_REQ,')',NULL,NULL,"Missing ')',after include",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);

  if(!scanner->max_parse_errors) 
    widget = config_parse(fname, FALSE);
  else
    widget = NULL;

  g_free(fname);

  return widget;
}

gboolean config_widget_property ( GScanner *scanner, GtkWidget *widget )
{
  if(IS_BASE_WIDGET(widget))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_STYLE:
        base_widget_set_style(widget,
            config_get_value(scanner,"style",TRUE,NULL));
        return TRUE;
      case G_TOKEN_CSS:
        css_widget_apply(base_widget_get_child(widget),
            config_assign_string(scanner,"css"));
        return TRUE;
      case G_TOKEN_INTERVAL:
        base_widget_set_interval(widget,
            1000*config_assign_number(scanner, "interval"));
        return TRUE;
      case G_TOKEN_TRIGGER:
        base_widget_set_interval(widget,0);
        base_widget_set_trigger(widget,
            config_assign_string(scanner, "trigger"));
        return TRUE;
      case G_TOKEN_LOC:
        base_widget_set_rect(widget,config_get_loc(scanner));
        return TRUE;
      case G_TOKEN_ACTION:
        config_widget_action(scanner, widget);
        return TRUE;
    }

  if(IS_BASE_WIDGET(widget) && !IS_FLOW_GRID(base_widget_get_child(widget)))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_VALUE:
        base_widget_set_value(widget,
            config_get_value(scanner,"value",TRUE,NULL));
        return TRUE;
      case G_TOKEN_TOOLTIP:
        base_widget_set_tooltip(widget,
            config_get_value(scanner,"tooltip",TRUE,NULL));
        return TRUE;
    }

  if(IS_PAGER(widget))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_PINS:
        config_get_pins( scanner, widget );
        return TRUE;
      case G_TOKEN_PREVIEW:
        g_object_set_data(G_OBJECT(base_widget_get_child(widget)),"preview",
            GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"preview")));
        return TRUE;
      case G_TOKEN_NUMERIC:
        g_object_set_data(G_OBJECT(widget),"sort_numeric",
            GINT_TO_POINTER(config_assign_boolean(scanner,TRUE,"numeric")));
        return TRUE;
    }

  if(IS_TASKBAR(widget))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_PEROUTPUT:
        g_object_set_data(G_OBJECT(widget),"filter_output",GINT_TO_POINTER(
              config_assign_boolean(scanner,FALSE,"filter_output")));
        return TRUE;
      case G_TOKEN_TITLEWIDTH:
        g_object_set_data(G_OBJECT(widget),"title_width",
            GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
        return TRUE;
      case G_TOKEN_SORT:
        g_object_set_data(G_OBJECT(widget),"sort",GINT_TO_POINTER(
              config_assign_boolean(scanner,TRUE,"sort")));
        return TRUE;
      case G_TOKEN_GROUP:
        if(g_scanner_peek_next_token(scanner) == '=')
        {
          g_object_set_data(G_OBJECT(widget),"group",GINT_TO_POINTER(
            config_assign_boolean(scanner,FALSE,"group")));
          return TRUE;
        }
        switch((gint)g_scanner_get_next_token(scanner))
        {
          case G_TOKEN_COLS:
            g_object_set_data(G_OBJECT(widget),"g_cols",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group cols")));
            return TRUE;
          case G_TOKEN_ROWS:
            g_object_set_data(G_OBJECT(widget),"g_rows",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group rows")));
            return TRUE;
          case G_TOKEN_ICONS:
            g_object_set_data(G_OBJECT(widget),"g_icons",
              GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"group icons")));
            return TRUE;
          case G_TOKEN_LABELS:
            g_object_set_data(G_OBJECT(widget),"g_labels",
              GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"group labels")));
            return TRUE;
          case G_TOKEN_CSS:
            g_object_set_data(G_OBJECT(widget),"g_css",
              config_assign_string(scanner,"group css"));
            return TRUE;
          case G_TOKEN_STYLE:
            g_object_set_data(G_OBJECT(widget),"g_style",
              config_assign_string(scanner,"group style"));
            return TRUE;
          case G_TOKEN_TITLEWIDTH:
            g_object_set_data(G_OBJECT(widget),"g_title_with",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group title_width")));
            return TRUE;
          case G_TOKEN_SORT:
            g_object_set_data(G_OBJECT(widget),"g_sort",GINT_TO_POINTER(
              config_assign_boolean(scanner,TRUE,"group sort")));
            return TRUE;
        }
    }

  if(IS_FLOW_GRID(base_widget_get_child(widget)))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_COLS:
        flow_grid_set_cols(base_widget_get_child(widget),
          config_assign_number(scanner, "cols"));
        return TRUE;
      case G_TOKEN_ROWS:
        flow_grid_set_rows(base_widget_get_child(widget),
          config_assign_number(scanner, "rows"));
        return TRUE;
      case G_TOKEN_ICONS:
        g_object_set_data(G_OBJECT(widget),"icons",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"icons")));
        return TRUE;
      case G_TOKEN_LABELS:
        g_object_set_data(G_OBJECT(widget),"labels",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"labels")));
        return TRUE;
    }

  return FALSE;
}

gboolean config_widget_child ( GScanner *scanner, GtkWidget *parent )
{
  GtkWidget *widget;

  if(!IS_GRID(parent))
    return FALSE;

  switch ((gint)scanner->token)
  {
    case G_TOKEN_GRID:
      widget = grid_new();
      break;
    case G_TOKEN_LABEL:
      widget = label_new();
      break;
    case G_TOKEN_IMAGE:
      widget = image_new();
      break;
    case G_TOKEN_BUTTON:
      widget = button_new();
      break;
    case G_TOKEN_SCALE:
      widget = scale_new();
      break;
    case G_TOKEN_CHART:
      widget = cchart_new();
      break;
    case G_TOKEN_INCLUDE:
      widget = config_include( scanner );
      break;
    case G_TOKEN_TASKBAR:
      widget = taskbar_new(TRUE);
      break;
    case G_TOKEN_PAGER:
      widget = pager_new();
      break;
    case G_TOKEN_TRAY:
      widget = tray_new();
      break;
    default:
      return FALSE;
  }
  scanner->max_parse_errors=FALSE;
  config_widget(scanner,widget);
  grid_attach(parent,widget);

  return TRUE;
}

void config_widget ( GScanner *scanner, GtkWidget *widget )
{
  gboolean curly = FALSE;
  gchar *id = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT,G_TOKEN_STRING,NULL,&id,NULL,
      SEQ_OPT,'{',NULL,&curly,NULL,
      SEQ_END);
  if(id)
    base_widget_set_id(widget,id);

  if(curly)
  {
    while ( (gint)g_scanner_get_next_token ( scanner ) != '}' &&
        (gint)scanner->token != G_TOKEN_EOF )
    {
      if(!config_widget_property(scanner,widget))
        if(!config_widget_child(scanner,widget))
          g_scanner_error(scanner,"Invalid property in a widget declaration");
    }
    if(scanner->token != '}')
      g_scanner_error(scanner,"Missing '}' at the end of widget properties");
  }
  config_optional_semicolon(scanner);
}

GtkWidget *config_layout ( GScanner *scanner, GtkWidget *widget )
{
  scanner->max_parse_errors=FALSE;
  
  if(!widget)
  {
    widget = grid_new();
    gtk_widget_set_name(widget,"layout");
  }

  config_widget(scanner,widget);
  return widget;
}

void config_switcher ( GScanner *scanner )
{
  GtkWidget *widget;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{',"Missing '{' after 'switcher'"))
    return;
  g_scanner_get_next_token(scanner);

  widget = switcher_new();

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_INTERVAL: 
        g_object_set_data(G_OBJECT(widget),"interval",
          GINT_TO_POINTER(config_assign_number(scanner,"interval")/100));
        break;
      case G_TOKEN_COLS: 
        flow_grid_set_cols(base_widget_get_child(widget),
          config_assign_number(scanner, "cols"));
        break;
      case G_TOKEN_ROWS:
        flow_grid_set_rows(base_widget_get_child(widget),
          config_assign_number(scanner, "rows"));
        break;
      case G_TOKEN_CSS:
        css_widget_apply(widget,config_assign_string(scanner,"css"));
        break;
      case G_TOKEN_ICONS:
        g_object_set_data(G_OBJECT(widget),"icons",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"icons")));
        break;
      case G_TOKEN_LABELS:
        g_object_set_data(G_OBJECT(widget),"labels",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"labels")));
        break;
      case G_TOKEN_TITLEWIDTH:
        g_object_set_data(G_OBJECT(widget),"title_width",
          GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'switcher'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);
}

void config_placer ( GScanner *scanner )
{
  gint wp_x= 10;
  gint wp_y= 10;
  gint wo_x= 0;
  gint wo_y= 0;
  gboolean pid = FALSE;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner,'{',"Missing '{' after 'placer'"))
    return;
  g_scanner_get_next_token(scanner);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token(scanner) )
    {
      case G_TOKEN_XSTEP: 
        wp_x = config_assign_number ( scanner, "xstep" );
        break;
      case G_TOKEN_YSTEP: 
        wp_y = config_assign_number ( scanner, "ystep" );
        break;
      case G_TOKEN_XORIGIN: 
        wo_x = config_assign_number ( scanner, "xorigin" );
        break;
      case G_TOKEN_YORIGIN: 
        wo_y = config_assign_number ( scanner, "yorigin" );
        break;
      case G_TOKEN_CHILDREN:
        pid = config_assign_boolean(scanner,FALSE,"children");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'placer'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);

  placer_config(wp_x,wp_y,wo_x,wo_y,pid);
}

GtkWidget *config_menu_item ( GScanner *scanner )
{
  gchar *label = NULL;
  action_t *action;
  GtkWidget *item;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'item'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&label,"missing label in 'item'",
      SEQ_REQ,',',NULL,NULL,"missing ',' in 'item'",
      SEQ_END);
  if(scanner->max_parse_errors)
  {
    g_free(label);
    return NULL;
  }

  action = config_action(scanner);

  if(!action)
  {
    g_scanner_error(scanner, "menu item: invalid action");
    g_free(label);
    return NULL;
  }

  if(g_scanner_get_next_token(scanner)!=')')
  {
    g_scanner_error(scanner,"missing ')' after 'item'");
    action_free(action,NULL);
    g_free(label);
    return NULL;
  }

  config_optional_semicolon(scanner);

  item = menu_item_new(label,action);
  g_free(label);
  return item;
}

void config_menu ( GScanner *scanner, GtkWidget *parent )
{
  gchar *name = NULL;
  GtkWidget *menu, *item;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'menu'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&name,"missing menu name",
      SEQ_REQ,')',NULL,NULL,"missing ')' afer 'menu'",
      SEQ_REQ,'{',NULL,NULL,"missing '{' afer 'menu'",
      SEQ_END);

  if(scanner->max_parse_errors)
    return g_free(name);

  menu = menu_from_name(name);
  if(!menu || parent)
    menu = gtk_menu_new();

  g_scanner_peek_next_token(scanner);
  while(scanner->next_token != G_TOKEN_EOF && scanner->next_token != '}')
  {
    item = NULL;
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_ITEM:
        item = config_menu_item(scanner);
        break;
      case G_TOKEN_SEPARATOR:
        item = gtk_separator_menu_item_new();
        config_optional_semicolon(scanner);
        break;
      case G_TOKEN_SUBMENU:
        config_menu(scanner,menu);
        break;
      default:
        g_scanner_error(scanner,
            "Unexpected token in menu. Expecting an item or a separator");
        break;
    }
    if(item)
      gtk_container_add(GTK_CONTAINER(menu),item);
    g_scanner_peek_next_token(scanner);
  }
  if(scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  if(!parent)
    menu_add(name,menu);
  else
  {
    item = gtk_menu_item_new_with_label(name);
    g_free(name);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),menu);
    gtk_container_add(GTK_CONTAINER(parent),item);
  }

  config_optional_semicolon(scanner);
}

void config_function ( GScanner *scanner )
{
  gchar *name = NULL;
  GList *actions = NULL;
  action_t *action;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'function'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&name,"missing function name",
      SEQ_REQ,')',NULL,NULL,"missing ')' afer 'function'",
      SEQ_REQ,'{',NULL,NULL,"missing '{' afer 'function'",
      SEQ_END);
  if(scanner->max_parse_errors)
    return g_free(name);

  g_scanner_peek_next_token(scanner);
  while(scanner->next_token != G_TOKEN_EOF && scanner->next_token != '}')
  {
    action = config_action(scanner);
    if(!action)
      g_scanner_error(scanner,"invalid action");
    else
      actions = g_list_append(actions, action);
  g_scanner_peek_next_token(scanner);
  }

  config_parse_sequence(scanner,
      SEQ_REQ,'}',NULL,NULL,"Expecting an action or '}'",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);

  action_function_add(name,actions);
}

void config_define ( GScanner *scanner )
{
  gchar *ident;
  gchar *value;

  if(!config_expect_token(scanner, G_TOKEN_IDENTIFIER,
        "Missing identifier after 'define'"))
    return;
  g_scanner_get_next_token(scanner);
  ident = g_strdup(scanner->value.v_identifier);  

  value = config_get_value(scanner,"define",TRUE,NULL);
  if(!value)
  {
    g_free(ident);
    return;
  }

  if(!defines)
    defines = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal,g_free,g_free);

  g_hash_table_insert(defines,ident,value);
}

void config_mappid_map ( GScanner *scanner )
{
  gchar *pattern, *appid;
  config_parse_sequence(scanner,
      SEQ_REQ,G_TOKEN_STRING,NULL,&pattern,"missing pattern in MapAppId",
      SEQ_REQ,',',NULL,NULL,"missing comma after pattern in MapAppId",
      SEQ_REQ,G_TOKEN_STRING,NULL,&appid,"missing app_id in MapAppId",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);
  if(!scanner->max_parse_errors)
    wintree_appid_map_add(pattern,appid);
  g_free(pattern);
  g_free(appid);
}

void config_trigger_action ( GScanner *scanner )
{
  gchar *trigger;
  action_t *action;

  config_parse_sequence(scanner,
      SEQ_REQ,G_TOKEN_STRING,NULL,&trigger,"missing trigger in TriggerAction",
      SEQ_REQ,',',NULL,NULL,"missing ',' in TriggerAction",
      SEQ_END);
  if(scanner->max_parse_errors)
    return g_free(trigger);

  action = config_action(scanner);
  if(!action)
    return g_free(trigger);

  action_trigger_add(action,trigger);
  config_optional_semicolon(scanner);
}

GtkWidget *config_parse_toplevel ( GScanner *scanner, gboolean toplevel )
{
  GtkWidget *w=NULL, *dest;

  while(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_SCANNER:
        config_scanner(scanner);
        break;
      case G_TOKEN_LAYOUT:
        if(!toplevel)
        {
          w = config_layout(scanner,w);
          break;
        }
        if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
        {
          g_scanner_get_next_token(scanner);
          dest = bar_grid_by_name(scanner->value.v_string);
        }
        else
          dest = bar_grid_by_name(NULL);
        config_layout(scanner,dest);
        css_widget_cascade(dest,NULL);
        break;
      case G_TOKEN_PLACER:
        config_placer(scanner);
        break;
      case G_TOKEN_SWITCHER:
        config_switcher(scanner);
        break;
      case G_TOKEN_MENU:
        config_menu(scanner,NULL);
        break;
      case G_TOKEN_DEFINE:
        config_define(scanner);
        break;
      case G_TOKEN_TRIGGERACTION:
        config_trigger_action(scanner);
        break;
      case G_TOKEN_MAPAPPID:
        config_mappid_map(scanner);
        break;
      case G_TOKEN_FUNCTION:
        config_function(scanner);
        break;
      default:
        g_scanner_error(scanner,"Unexpected toplevel token");
        break;
    }
  }
  return w;
}
GtkWidget *config_parse_data ( gchar *fname, gchar *data, gboolean toplevel )
{
  GScanner *scanner;
  GtkWidget *w;
  GtkCssProvider *css;
  gchar *tmp;

  if(!data)
    return NULL;

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

  scanner->msg_handler = config_log_error;
  scanner->max_parse_errors = FALSE;

  g_scanner_scope_add_symbol(scanner,0, "Scanner", (gpointer)G_TOKEN_SCANNER );
  g_scanner_scope_add_symbol(scanner,0, "Layout", (gpointer)G_TOKEN_LAYOUT );
  g_scanner_scope_add_symbol(scanner,0, "Placer", (gpointer)G_TOKEN_PLACER );
  g_scanner_scope_add_symbol(scanner,0, "Switcher",
      (gpointer)G_TOKEN_SWITCHER );
  g_scanner_scope_add_symbol(scanner,0, "Define", (gpointer)G_TOKEN_DEFINE );
  g_scanner_scope_add_symbol(scanner,0, "TriggerAction",
      (gpointer)G_TOKEN_TRIGGERACTION );
  g_scanner_scope_add_symbol(scanner,0, "MapAppId",
      (gpointer)G_TOKEN_MAPAPPID );
  g_scanner_scope_add_symbol(scanner,0, "End", (gpointer)G_TOKEN_END );
  g_scanner_scope_add_symbol(scanner,0, "File", (gpointer)G_TOKEN_FILE );
  g_scanner_scope_add_symbol(scanner,0, "Exec", (gpointer)G_TOKEN_EXEC );
  g_scanner_scope_add_symbol(scanner,0, "MpdClient",
      (gpointer)G_TOKEN_MPDCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "SwayClient",
      (gpointer)G_TOKEN_SWAYCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "ExecClient",
      (gpointer)G_TOKEN_EXECCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "SOcketClient",
      (gpointer)G_TOKEN_SOCKETCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "Number", (gpointer)G_TOKEN_NUMBERW );
  g_scanner_scope_add_symbol(scanner,0, "String", (gpointer)G_TOKEN_STRINGW );
  g_scanner_scope_add_symbol(scanner,0, "NoGlob", (gpointer)G_TOKEN_NOGLOB );
  g_scanner_scope_add_symbol(scanner,0, "CheckTime",
      (gpointer)G_TOKEN_CHTIME );
  g_scanner_scope_add_symbol(scanner,0, "Sum", (gpointer)G_TOKEN_SUM );
  g_scanner_scope_add_symbol(scanner,0, "Product", (gpointer)G_TOKEN_PRODUCT );
  g_scanner_scope_add_symbol(scanner,0, "Last", (gpointer)G_TOKEN_LASTW );
  g_scanner_scope_add_symbol(scanner,0, "First", (gpointer)G_TOKEN_FIRST );
  g_scanner_scope_add_symbol(scanner,0, "Grid", (gpointer)G_TOKEN_GRID );
  g_scanner_scope_add_symbol(scanner,0, "Scale", (gpointer)G_TOKEN_SCALE );
  g_scanner_scope_add_symbol(scanner,0, "Label", (gpointer)G_TOKEN_LABEL );
  g_scanner_scope_add_symbol(scanner,0, "Button", (gpointer)G_TOKEN_BUTTON );
  g_scanner_scope_add_symbol(scanner,0, "Image", (gpointer)G_TOKEN_IMAGE );
  g_scanner_scope_add_symbol(scanner,0, "Chart", (gpointer)G_TOKEN_CHART );
  g_scanner_scope_add_symbol(scanner,0, "Include", (gpointer)G_TOKEN_INCLUDE );
  g_scanner_scope_add_symbol(scanner,0, "TaskBar", (gpointer)G_TOKEN_TASKBAR );
  g_scanner_scope_add_symbol(scanner,0, "Pager", (gpointer)G_TOKEN_PAGER );
  g_scanner_scope_add_symbol(scanner,0, "Tray", (gpointer)G_TOKEN_TRAY );
  g_scanner_scope_add_symbol(scanner,0, "Style", (gpointer)G_TOKEN_STYLE );
  g_scanner_scope_add_symbol(scanner,0, "Css", (gpointer)G_TOKEN_CSS );
  g_scanner_scope_add_symbol(scanner,0, "Interval",
      (gpointer)G_TOKEN_INTERVAL );
  g_scanner_scope_add_symbol(scanner,0, "Value", (gpointer)G_TOKEN_VALUE );
  g_scanner_scope_add_symbol(scanner,0, "Pins", (gpointer)G_TOKEN_PINS );
  g_scanner_scope_add_symbol(scanner,0, "Preview", (gpointer)G_TOKEN_PREVIEW );
  g_scanner_scope_add_symbol(scanner,0, "Cols", (gpointer)G_TOKEN_COLS );
  g_scanner_scope_add_symbol(scanner,0, "Rows", (gpointer)G_TOKEN_ROWS );
  g_scanner_scope_add_symbol(scanner,0, "Action", (gpointer)G_TOKEN_ACTION );
  g_scanner_scope_add_symbol(scanner,0, "Display", (gpointer)G_TOKEN_DISPLAY );
  g_scanner_scope_add_symbol(scanner,0, "Icons", (gpointer)G_TOKEN_ICONS );
  g_scanner_scope_add_symbol(scanner,0, "Labels", (gpointer)G_TOKEN_LABELS );
  g_scanner_scope_add_symbol(scanner,0, "Loc", (gpointer)G_TOKEN_LOC );
  g_scanner_scope_add_symbol(scanner,0, "Numeric", (gpointer)G_TOKEN_NUMERIC );
  g_scanner_scope_add_symbol(scanner,0, "Filter_output", 
      (gpointer)G_TOKEN_PEROUTPUT );
  g_scanner_scope_add_symbol(scanner,0, "Title_width", 
      (gpointer)G_TOKEN_TITLEWIDTH );
  g_scanner_scope_add_symbol(scanner,0, "Tooltip", (gpointer)G_TOKEN_TOOLTIP );
  g_scanner_scope_add_symbol(scanner,0, "Trigger", (gpointer)G_TOKEN_TRIGGER );
  g_scanner_scope_add_symbol(scanner,0, "Group", (gpointer)G_TOKEN_GROUP );
  g_scanner_scope_add_symbol(scanner,0, "XStep", (gpointer)G_TOKEN_XSTEP );
  g_scanner_scope_add_symbol(scanner,0, "YStep", (gpointer)G_TOKEN_YSTEP );
  g_scanner_scope_add_symbol(scanner,0, "XOrigin", (gpointer)G_TOKEN_XORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "YOrigin", (gpointer)G_TOKEN_YORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "Children", 
      (gpointer)G_TOKEN_CHILDREN );
  g_scanner_scope_add_symbol(scanner,0, "Sort", (gpointer)G_TOKEN_SORT );
  g_scanner_scope_add_symbol(scanner,0, "True", (gpointer)G_TOKEN_TRUE );
  g_scanner_scope_add_symbol(scanner,0, "False", (gpointer)G_TOKEN_FALSE );
  g_scanner_scope_add_symbol(scanner,0, "Menu", (gpointer)G_TOKEN_MENU );
  g_scanner_scope_add_symbol(scanner,0, "MenuClear", 
      (gpointer)G_TOKEN_MENUCLEAR );
  g_scanner_scope_add_symbol(scanner,0, "PipeRead",
      (gpointer)G_TOKEN_PIPEREAD );
  g_scanner_scope_add_symbol(scanner,0, "Config", (gpointer)G_TOKEN_CONFIG );
  g_scanner_scope_add_symbol(scanner,0, "SwayCmd", (gpointer)G_TOKEN_SWAYCMD );
  g_scanner_scope_add_symbol(scanner,0, "SwayWinCmd",
      (gpointer)G_TOKEN_SWAYWIN );
  g_scanner_scope_add_symbol(scanner,0, "MpdCmd", (gpointer)G_TOKEN_MPDCMD );
  g_scanner_scope_add_symbol(scanner,0, "UserState",
      (gpointer)G_TOKEN_USERSTATE );
  g_scanner_scope_add_symbol(scanner,0, "IdleInhibit",
      (gpointer)G_TOKEN_IDLEINHIBIT );
  g_scanner_scope_add_symbol(scanner,0, "SetValue",
      (gpointer)G_TOKEN_SETVALUE );
  g_scanner_scope_add_symbol(scanner,0, "SetStyle",
      (gpointer)G_TOKEN_SETSTYLE );
  g_scanner_scope_add_symbol(scanner,0, "SetTooltip",
      (gpointer)G_TOKEN_SETTOOLTIP );
  g_scanner_scope_add_symbol(scanner,0, "Function",
      (gpointer)G_TOKEN_FUNCTION );
  g_scanner_scope_add_symbol(scanner,0, "Focus", (gpointer)G_TOKEN_FOCUS );
  g_scanner_scope_add_symbol(scanner,0, "Close", (gpointer)G_TOKEN_CLOSE );
  g_scanner_scope_add_symbol(scanner,0, "Minimize",
      (gpointer)G_TOKEN_MINIMIZE );
  g_scanner_scope_add_symbol(scanner,0, "Maximize",
      (gpointer)G_TOKEN_MAXIMIZE );
  g_scanner_scope_add_symbol(scanner,0, "UnMinimize",
      (gpointer)G_TOKEN_UNMINIMIZE );
  g_scanner_scope_add_symbol(scanner,0, "UnMaximize",
      (gpointer)G_TOKEN_UNMAXIMIZE );
  g_scanner_scope_add_symbol(scanner,0, "SetMonitor",
      (gpointer)G_TOKEN_SETMONITOR );
  g_scanner_scope_add_symbol(scanner,0, "SetLayer",
      (gpointer)G_TOKEN_SETLAYER );
  g_scanner_scope_add_symbol(scanner,0, "SetBarSize", 
      (gpointer)G_TOKEN_SETBARSIZE );
  g_scanner_scope_add_symbol(scanner,0, "SetExclusiveZone", 
      (gpointer)G_TOKEN_SETEXCLUSIVEZONE );
  g_scanner_scope_add_symbol(scanner,0, "SetBarID",
      (gpointer)G_TOKEN_SETBARID );
  g_scanner_scope_add_symbol(scanner,0, "ClientSend",
      (gpointer)G_TOKEN_CLIENTSEND );
  g_scanner_scope_add_symbol(scanner,0, "Item", (gpointer)G_TOKEN_ITEM );
  g_scanner_scope_add_symbol(scanner,0, "Separator",
      (gpointer)G_TOKEN_SEPARATOR );
  g_scanner_scope_add_symbol(scanner,0, "SubMenu", (gpointer)G_TOKEN_SUBMENU );
  g_scanner_scope_add_symbol(scanner,0, "Minimized",
      (gpointer)G_TOKEN_MINIMIZED );
  g_scanner_scope_add_symbol(scanner,0, "Maximized",
      (gpointer)G_TOKEN_MAXIMIZED );
  g_scanner_scope_add_symbol(scanner,0, "FullScreen",
      (gpointer)G_TOKEN_FULLSCREEN );
  g_scanner_scope_add_symbol(scanner,0, "Focused", (gpointer)G_TOKEN_FOCUSED );
  g_scanner_scope_add_symbol(scanner,0, "RegEx", (gpointer)G_TOKEN_REGEX );
  g_scanner_scope_add_symbol(scanner,0, "Json", (gpointer)G_TOKEN_JSON );
  g_scanner_scope_add_symbol(scanner,0, "Grab", (gpointer)G_TOKEN_GRAB );
  g_scanner_scope_add_symbol(scanner,0, "Title", (gpointer)G_TOKEN_TITLE );
  g_scanner_scope_add_symbol(scanner,0, "AppId", (gpointer)G_TOKEN_APPID );
  g_scanner_scope_add_symbol(scanner,0, "Seq", (gpointer)G_TOKEN_SEQ );

  tmp = strstr(data,"\n#CSS");
  if(tmp)
  {
    *tmp=0;
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,tmp+5,strlen(tmp+5),NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
  }

  scanner->input_name = fname;
  g_scanner_input_text( scanner, data, -1 );

  w = config_parse_toplevel ( scanner, toplevel );
  g_free(scanner->config->cset_identifier_first);
  g_free(scanner->config->cset_identifier_nth);
  g_scanner_destroy(scanner);

  return w;
}

void config_string ( gchar *string )
{
  gchar *conf;

  if(!string)
    return;

  conf = g_strdup(string);
  config_parse_data("config string",conf,TRUE);
  g_free(conf);
}

void config_pipe_read ( gchar *command )
{
  FILE *fp;
  gchar *conf;
  GIOChannel *chan;

  if(!command)
    return;

  fp = popen(command, "r");
  if(!fp)
    return;

  chan = g_io_channel_unix_new( fileno(fp) );
  if(chan)
  {
    if(g_io_channel_read_to_end( chan , &conf,NULL,NULL)==G_IO_STATUS_NORMAL)
      config_parse_data(command,conf,TRUE);
    g_free(conf);
    g_io_channel_unref(chan);
  }

  pclose(fp);
}

GtkWidget *config_parse ( gchar *file, gboolean toplevel )
{
  gchar *fname, *dir, *base ,*cssfile, *csspath, *tmp;
  gchar *conf=NULL;
  gsize size;
  GtkWidget *w=NULL;

  fname = get_xdg_config_file(file,NULL);
  g_debug("include: %s -> %s",file,fname);

  if(fname)
    if(!g_file_get_contents(fname,&conf,&size,NULL))
      conf=NULL;

  if(!conf)
  {
    g_error("Error: can't read config file %s\n",file);
    exit(1);
  }

  w = config_parse_data (fname, conf, toplevel);

  g_free(conf);

  dir = g_path_get_dirname(fname);
  base = g_path_get_basename(fname);
  
  tmp = strrchr(base,'.');
  if(tmp)
    *tmp = '\0';
  cssfile = g_strconcat(base,".css",NULL);
  csspath = g_build_filename(dir,cssfile,NULL);

  css_file_load (csspath);

  g_free(csspath);
  g_free(cssfile);
  g_free(base);
  g_free(dir);
  g_free(fname);

  return w;
}
