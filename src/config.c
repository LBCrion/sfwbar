/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include "sfwbar.h"
#include "config.h"
#include <fcntl.h>
#include <sys/stat.h>

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
    dest  = va_arg(args, void * );
    err = va_arg(args, char * );
    if(g_scanner_peek_next_token(scanner) == type ||
        ( scanner->next_token == G_TOKEN_FLOAT && type == G_TOKEN_INT) )
    {
      g_scanner_get_next_token(scanner);
      matched = TRUE;
      if(dest)
        switch(type)
        {
          case G_TOKEN_STRING:
            *((gchar **)dest) = g_strdup(scanner->value.v_string);
            break;
          case G_TOKEN_FLOAT:
            *((gdouble *)dest) = scanner->value.v_float;
            break;
          case G_TOKEN_INT:
            *((gint *)dest) = (gint)scanner->value.v_float;
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

void config_scanner_var ( GScanner *scanner, struct scan_file *file )
{
  struct scan_var *var;
  gchar *vname, *pattern = NULL;
  guchar type;
  gint flag = SV_REPLACE;

  scanner->max_parse_errors = FALSE;
  g_scanner_get_next_token(scanner);
  vname = g_strdup(scanner->value.v_identifier);

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <parser>",vname))
  {
    g_free(vname);
    return;
  }
  g_scanner_get_next_token(scanner);

  g_scanner_get_next_token(scanner);
  if(((gint)scanner->token < G_TOKEN_REGEX)||
      ((gint)scanner->token > G_TOKEN_GRAB))
  {
    g_scanner_error(scanner,"Missing <parser> in %s = <parser>",vname);
    g_free(vname);
    return;
  }

  type = scanner->token - G_TOKEN_REGEX;
  if(!config_expect_token(scanner, '(', "Missing '(' in parser"))
  {
    g_free(vname);
    return;
  }
  g_scanner_get_next_token(scanner);

  if(type != VP_GRAB)
  {
    if(g_scanner_get_next_token(scanner)!=G_TOKEN_STRING)
    {
      g_scanner_error(scanner,
            "Missing <string> parameter in parser");
      g_free(vname);
      return;
    }
    else
      pattern = g_strdup(scanner->value.v_string);
  }

  if((g_scanner_peek_next_token(scanner)==',')||(type == VP_GRAB))
  {
    if(type != VP_GRAB)
      g_scanner_get_next_token(scanner);
    if(((gint)g_scanner_peek_next_token(scanner)>=G_TOKEN_SUM)&&
        ((gint)g_scanner_peek_next_token(scanner)<=G_TOKEN_FIRST))
    {
      g_scanner_get_next_token(scanner);
      flag = scanner->token - G_TOKEN_SUM + 1;
    }
    else
      if(type != VP_GRAB)
      {
        g_scanner_get_next_token(scanner);
        g_scanner_error(scanner,"Missing <aggregator> in parser");
      }
  }

  if(!config_expect_token(scanner,')', "Missing ')' in parser"))
  {
    g_free(vname);
    return;
  }
  g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);

  var = g_malloc0(sizeof(struct scan_var));

  switch(type)
  {
    case VP_JSON:
      var->json = pattern;
      break;
    case VP_REGEX:
      var->regex = g_regex_new(pattern,0,0,NULL);
      g_free(pattern);
      break;
    default:
      g_free(pattern);
      break;
  }

  var->file = file;
  var->type = type;
  var->multi = flag;

  file->vars = g_list_append(file->vars,var);
  scanner_var_attach(vname,var);
}

struct scan_file *config_scanner_source ( GScanner *scanner, gint source )
{
  gchar *fname = NULL;
  gchar *trigger = NULL;
  gint flags = 0;
  struct scan_file *file;
  GList *find;
  static GList *file_list = NULL;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '(', "Missing '(' after <source>"))
    return NULL;
  g_scanner_get_next_token(scanner);

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing <string> in source(<string>)"))
    return NULL;
  g_scanner_get_next_token(scanner);

  fname = g_strdup(scanner->value.v_string);

  if(source == SO_FILE)
    while((gint)g_scanner_peek_next_token(scanner)==',')
    {
      g_scanner_get_next_token(scanner);
      switch((gint)g_scanner_get_next_token(scanner))
      {
        case G_TOKEN_CHTIME:
          flags |= VF_CHTIME;
          break;
        case G_TOKEN_NOGLOB:
          flags |= VF_NOGLOB;
          break;
        default:
          g_scanner_error(scanner,"Invalid <file_flag> in %s",fname);
          break;
      }
    } 

  if(source == SO_CLIENT)
    if((gint)g_scanner_peek_next_token(scanner)==',')
    {
      g_scanner_get_next_token(scanner);
      if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
        g_scanner_error(scanner,"Invalid trigger in client declaration");
      else
      {
        g_scanner_get_next_token(scanner);
        trigger = g_strdup(scanner->value.v_string);
      }
    }

  if(config_expect_token(scanner,')',"Missing ')' in source"))
    g_scanner_get_next_token(scanner);

  if(config_expect_token(scanner,'{',"Missing '{' after <source>"))
    g_scanner_get_next_token(scanner);

  if(!fname)
    return NULL;

  if(scanner->max_parse_errors)
  {
    g_free(fname);
    return NULL;
  }

  if(source == SO_CLIENT)
    find = NULL;
  else
    for(find=file_list;find;find=g_list_next(find))
      if(!g_strcmp0(fname,((struct scan_file *)(find->data))->fname))
        break;

  if(find!=NULL)
    file = find->data;
  else
    file = g_malloc(sizeof(struct scan_file));
  file->fname = fname;
  file->trigger = trigger;
  file->source = source;
  file->mtime = 0;
  file->flags = flags;
  file->vars = NULL;
  if( !strchr(fname,'*') && !strchr(fname,'?') )
    file->flags |= VF_NOGLOB;
  file_list = g_list_append(file_list,file);
  if(file->trigger)
    scanner_file_attach(trigger,file);

  while(((gint)g_scanner_peek_next_token(scanner)!='}')&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch((gint)g_scanner_peek_next_token(scanner))
    {
      case G_TOKEN_IDENTIFIER:
        config_scanner_var(scanner, file);
        break;
      default:
        g_scanner_get_next_token(scanner);
        g_scanner_error(scanner, "Expecting a variable declaration or End");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  return file;
}

void config_scanner ( GScanner *scanner )
{
  struct scan_file *file;
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
        config_scanner_source(scanner,SO_FILE);
        break;
      case G_TOKEN_EXEC:
        config_scanner_source(scanner,SO_EXEC);
        break;
      case G_TOKEN_MPDCLIENT:
        file = config_scanner_source(scanner,SO_CLIENT);
        mpd_ipc_init(file);
        break;
      case G_TOKEN_SWAYCLIENT:
        file = config_scanner_source(scanner,SO_CLIENT);
        sway_ipc_client_init(file);
        break;
      case G_TOKEN_EXECCLIENT:
        file = config_scanner_source(scanner,SO_CLIENT);
        client_exec(file);
        break;
      case G_TOKEN_SOCKETCLIENT:
        file = config_scanner_source(scanner,SO_CLIENT);
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
      SEQ_REQ,'(',NULL,"missing '(' afer loc",
      SEQ_REQ,G_TOKEN_INT,&rect.x,"missing x value in loc",
      SEQ_REQ,',',NULL,"missing comma afer x value in loc",
      SEQ_REQ,G_TOKEN_INT,&rect.y,"missing y value in loc",
      SEQ_OPT,',',NULL,NULL,
      SEQ_CON,G_TOKEN_INT,&rect.w,"missing w value in loc",
      SEQ_OPT,',',NULL,NULL,
      SEQ_CON,G_TOKEN_INT,&rect.h,"missing h value in loc",
      SEQ_REQ,')',NULL,"missing ')' in loc statement",
      SEQ_OPT,';',NULL,NULL,
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

gchar *config_get_value ( GScanner *scanner, gchar *prop, gboolean assign )
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

void config_get_pins ( GScanner *scanner, struct layout_widget *lw )
{
  scanner->max_parse_errors = FALSE;

  if(lw->wtype != G_TOKEN_PAGER)
  {
    g_scanner_error(scanner,"this widget has no property 'pins'");
    return;
  }
  if(!config_expect_token(scanner, '=',"expecting pins = string [,string]"))
    return;
  do
  {
    g_scanner_get_next_token(scanner);
    if(!config_expect_token(scanner, G_TOKEN_STRING,
          "expecting a string in pins = string [,string]"))
      break;
    g_scanner_get_next_token(scanner);
    pager_add_pin(g_strdup(scanner->value.v_string));
  } while ( g_scanner_peek_next_token(scanner)==',');
  config_optional_semicolon(scanner);
}

void config_widget_cols ( GScanner *scanner, struct layout_widget *lw )
{
  scanner->max_parse_errors = FALSE;

  if( (lw->wtype != G_TOKEN_TASKBAR) &&
      (lw->wtype != G_TOKEN_PAGER) &&
      (lw->wtype != G_TOKEN_TRAY) )
  {
    g_scanner_error(scanner,"this widget has no property 'cols'");
    return;
  }
  flow_grid_set_cols(lw->widget, config_assign_number(scanner, "cols"));
}

void config_widget_rows ( GScanner *scanner, struct layout_widget *lw )
{
  scanner->max_parse_errors = FALSE;

  if( (lw->wtype != G_TOKEN_TASKBAR) &&
      (lw->wtype != G_TOKEN_PAGER) &&
      (lw->wtype != G_TOKEN_TRAY) )
  {
    g_scanner_error(scanner,"this widget has no property 'rows'");
    return;
  }
  flow_grid_set_rows(lw->widget, config_assign_number(scanner, "rows"));
}

void config_action_conditions ( GScanner *scanner, guchar *cond,
    guchar *ncond )
{
  guchar *ptr;

  if(g_scanner_peek_next_token(scanner) == '[')
  {
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
}

struct layout_action *config_action ( GScanner *scanner )
{
  struct layout_action *action;

  action = g_malloc0(sizeof(struct layout_action));
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
          SEQ_REQ,G_TOKEN_STRING,&action->addr,"Missing argument in action",
          SEQ_OPT,',',NULL,NULL,
          SEQ_CON,G_TOKEN_STRING,&action->command,"Missing argument after ','",
          SEQ_END);
      if(!action->command)
      {
        action->command = action->addr;
        action->addr = NULL;
      }
      break;
    case G_TOKEN_CLIENTSEND:
      config_parse_sequence(scanner,
          SEQ_REQ,G_TOKEN_STRING,&action->addr,"Missing address in action",
          SEQ_OPT,',',NULL,NULL,
          SEQ_CON,G_TOKEN_STRING,&action->command,"Missing command in action",
          SEQ_END);
      break;
    case G_TOKEN_SETVALUE:
      action->command = config_get_value(scanner,"action value",FALSE);
      break;
    case G_TOKEN_SETSTYLE:
      action->command = config_get_value(scanner,"action style",FALSE);
      break;
    case G_TOKEN_SETTOOLTIP:
      action->command = config_get_value(scanner,"action tooltip",FALSE);
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

void config_widget_action ( GScanner *scanner, struct layout_widget *lw )
{
  gint button;

  if(g_scanner_peek_next_token(scanner)=='[')
  {
    config_parse_sequence(scanner,
        SEQ_REQ,'[',NULL,NULL,
        SEQ_REQ,G_TOKEN_INT,&button,"expecting a number in action[<number>]",
        SEQ_REQ,']',NULL,"expecting a ']' in action[<number>]",
        SEQ_END);
    if(scanner->max_parse_errors)
      return;
  }
  else
    button = 1;

  if( button<0 || button >=MAX_BUTTON )
    return g_scanner_error(scanner,"invalid action index %d",button);
  if(g_scanner_get_next_token(scanner) != '=')
    return g_scanner_error(scanner,"expecting a '=' after 'action'");

  action_free(lw->actions[button],NULL);
  lw->actions[button] = config_action(scanner);
  if(!lw->actions[button])
    return g_scanner_error(scanner,"invalid action");

  config_optional_semicolon(scanner);
}

gboolean config_widget_props ( GScanner *scanner, struct layout_widget *lw )
{
  scanner->max_parse_errors = FALSE;

  if( g_scanner_peek_next_token( scanner ) != '{')
    return FALSE;
  else
    g_scanner_get_next_token(scanner);

  g_scanner_peek_next_token( scanner );
  while (!( (gint)scanner->next_token >= G_TOKEN_GRID &&
      (gint)scanner->next_token <= G_TOKEN_TRAY &&
      lw->wtype == G_TOKEN_GRID )&&
      (gint)scanner->next_token != '}' &&
      (gint)scanner->next_token != G_TOKEN_EOF )
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_STYLE:
        lw->style = config_get_value(scanner,"style",TRUE);
        break;
      case G_TOKEN_CSS:
        lw->css = config_assign_string(scanner,"css");
        break;
      case G_TOKEN_INTERVAL:
        if(GTK_IS_GRID(lw->widget))
          g_scanner_error(scanner,"this widget has no property 'interval'");
        else
        {
          if(!lw->interval)
            g_scanner_error(scanner,"this widget already has a trigger");
          else
            lw->interval = 1000*config_assign_number(scanner, "interval");
        }
        break;
      case G_TOKEN_TRIGGER:
        lw->interval = 0;
        lw->trigger = config_assign_string(scanner, "trigger");
        break;
      case G_TOKEN_VALUE:
        if(GTK_IS_GRID(lw->widget))
          g_scanner_error(scanner,"this widget has no property 'value'");
        else
          lw->value = config_get_value(scanner,"value",TRUE);
        break;
      case G_TOKEN_TOOLTIP:
        if(GTK_IS_GRID(lw->widget))
          g_scanner_error(scanner,"this widget has no property 'tooltip'");
        else
          lw->tooltip = config_get_value(scanner,"tooltip",TRUE);
        break;
      case G_TOKEN_PINS:
        config_get_pins( scanner, lw );
        break;
      case G_TOKEN_PREVIEW:
        if(lw->wtype != G_TOKEN_PAGER)
        {
          g_scanner_error(scanner,"this widget has no property 'preview'");
          break;
        }
        pager_set_preview(config_assign_boolean(scanner,FALSE,"preview"));
        break;
      case G_TOKEN_NUMERIC:
        if(lw->wtype != G_TOKEN_PAGER)
        {
          g_scanner_error(scanner,"this widget has no property 'numeric'");
          break;
        }
        pager_set_numeric(config_assign_boolean(scanner,TRUE,"numeric"));
        break;
      case G_TOKEN_PEROUTPUT:
        if(lw->wtype == G_TOKEN_TASKBAR)
          g_object_set_data(G_OBJECT(lw->widget),"filter_output",
            GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"filter_output")));
        else
          g_scanner_error(scanner,
              "this widget has no property 'filter_output'");
        break;
      case G_TOKEN_TITLEWIDTH:
        if(lw->wtype == G_TOKEN_TASKBAR)
          g_object_set_data(G_OBJECT(lw->widget),"title_width",
              GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
        else
          g_scanner_error(scanner,
              "this widget has no property 'title_width'");
        break;
      case G_TOKEN_COLS:
        config_widget_cols(scanner, lw);
        break;
      case G_TOKEN_ROWS:
        config_widget_rows(scanner, lw);
        break;
      case G_TOKEN_ACTION:
        config_widget_action(scanner, lw);
        break;
      case G_TOKEN_ICONS:
        g_object_set_data(G_OBJECT(lw->widget),"icons",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"icons")));
        break;
      case G_TOKEN_LABELS:
        g_object_set_data(G_OBJECT(lw->widget),"labels",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"labels")));
        break;
      case G_TOKEN_LOC:
        lw->rect = config_get_loc(scanner);
        break;
      default:
        g_scanner_error(scanner, "Unexpected token in widget definition");
    }
    g_scanner_peek_next_token( scanner );
  }
  if((gint)g_scanner_peek_next_token(scanner) == '}' &&
      lw->wtype != G_TOKEN_GRID )
    g_scanner_get_next_token(scanner);

  return TRUE;
}

struct layout_widget *config_include ( GScanner *scanner )
{
  struct layout_widget *lw;
  gchar *fname = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,"Missing '(' after include",
      SEQ_REQ,G_TOKEN_STRING,&fname,"Missing filename in include",
      SEQ_REQ,')',NULL,"Missing ')',after include",
      SEQ_OPT,';',NULL,NULL,
      SEQ_END);

  if(!scanner->max_parse_errors) 
  {
    lw = config_parse(fname,FALSE);
    lw->wtype = G_TOKEN_INCLUDE;
  }
  else
    lw = NULL;

  g_free(fname);

  return lw;
}

void config_widgets ( GScanner *scanner, GtkWidget *parent )
{
  GtkWidget *sibling=NULL;
  struct layout_widget *lw;
  gboolean extra;

  while ( (gint)g_scanner_peek_next_token ( scanner ) != '}' &&
      (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF )
  {
    lw = layout_widget_new();
    lw->wtype = g_scanner_get_next_token(scanner);
    switch ( lw->wtype )
    {
      case G_TOKEN_GRID:
        scanner->max_parse_errors=FALSE;
        lw->widget = gtk_grid_new();
        break;
      case G_TOKEN_LABEL:
        scanner->max_parse_errors=FALSE;
        lw->widget = gtk_label_new("");
        gtk_label_set_ellipsize(GTK_LABEL(lw->widget),PANGO_ELLIPSIZE_END);
        break;
      case G_TOKEN_IMAGE:
        scanner->max_parse_errors=FALSE;
        lw->widget = scale_image_new();
        break;
      case G_TOKEN_BUTTON:
        scanner->max_parse_errors=FALSE;
        lw->widget = gtk_button_new();
        break;
      case G_TOKEN_SCALE:
        scanner->max_parse_errors=FALSE;
        lw->widget = gtk_progress_bar_new();
        break;
      case G_TOKEN_INCLUDE:
        layout_widget_free(lw);
        lw = config_include( scanner );
        break;
      case G_TOKEN_TASKBAR:
        scanner->max_parse_errors=FALSE;
        lw->widget = flow_grid_new(TRUE);
        break;
      case G_TOKEN_PAGER:
        scanner->max_parse_errors=FALSE;
        lw->widget = flow_grid_new(TRUE);
        pager_set_numeric(TRUE);
        break;
      case G_TOKEN_TRAY:
        scanner->max_parse_errors=FALSE;
        lw->widget = flow_grid_new(TRUE);
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'layout'");
        layout_widget_free(lw);
        continue;
    }
    if(!lw)
      continue;
    if(scanner->max_parse_errors || !lw->widget)
    {
      layout_widget_free(lw);
      continue;
    }
    extra = config_widget_props( scanner, lw);
    sibling = layout_widget_config ( lw, parent, sibling );

    if(lw->wtype == G_TOKEN_GRID && extra)
      config_widgets(scanner,lw->widget);

    layout_widget_attach(lw);
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

struct layout_widget *config_layout ( GScanner *scanner,
    struct layout_widget *lw )
{
  gboolean extra;

  scanner->max_parse_errors=FALSE;
  
  if(!lw)
  {
    lw = layout_widget_new();
    lw->wtype = G_TOKEN_GRID;
    lw->widget = gtk_grid_new();
    gtk_widget_set_name(lw->widget,"layout");
  }

  extra = config_widget_props(scanner, lw);
  layout_widget_config(lw,NULL,NULL);
  if( lw->widget && extra)
    config_widgets(scanner, lw->widget);

  return lw;
}

void config_switcher ( GScanner *scanner )
{
  gchar *css=NULL;
  gint interval = 1, cols = 1, twidth = -1;
  gboolean icons = FALSE, labels = FALSE;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{',"Missing '{' after 'switcher'"))
    return;
  g_scanner_get_next_token(scanner);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_INTERVAL: 
        interval = config_assign_number(scanner,"interval")/100;
        break;
      case G_TOKEN_COLS: 
        cols = config_assign_number(scanner,"cols");
        break;
      case G_TOKEN_CSS:
        g_free(css);
        css = config_assign_string(scanner,"css");
        break;
      case G_TOKEN_ICONS:
        icons = config_assign_boolean(scanner,FALSE,"icons");
        break;
      case G_TOKEN_LABELS:
        labels = config_assign_boolean(scanner,FALSE,"labels");
        break;
      case G_TOKEN_TITLEWIDTH:
        twidth = config_assign_number(scanner,"title_width");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'switcher'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);

  switcher_config(cols,css,interval,icons,labels,twidth);
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

  if(wp_x<1)
    wp_x=1;
  if(wp_y<1)
    wp_y=1;
  placer_config(wp_x,wp_y,wo_x,wo_y,pid);
}

GtkWidget *config_menu_item ( GScanner *scanner )
{
  gchar *label = NULL;
  struct layout_action *action;
  GtkWidget *item;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,"missing '(' after 'item'",
      SEQ_REQ,G_TOKEN_STRING,&label,"missing label in 'item'",
      SEQ_REQ,',',NULL,"missing ',' in 'item'",
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

  item = gtk_menu_item_new_with_label(label);
  g_free(label);
  g_signal_connect(G_OBJECT(item),"activate",
      G_CALLBACK(widget_menu_action),action);
  g_object_weak_ref(G_OBJECT(item),(GWeakNotify)action_free,action);
  return item;
}

void config_menu ( GScanner *scanner, GtkWidget *parent )
{
  gchar *name = NULL;
  GtkWidget *menu, *item;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,"missing '(' after 'menu'",
      SEQ_REQ,G_TOKEN_STRING,&name,"missing menu name",
      SEQ_REQ,')',NULL,"missing ')' afer 'menu'",
      SEQ_REQ,'{',NULL,"missing '{' afer 'menu'",
      SEQ_END);

  if(scanner->max_parse_errors)
    return g_free(name);

  menu = layout_menu_get(name);
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
    layout_menu_add(name,menu);
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
  GList *actions;
  struct layout_action *action;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,"missing '(' after 'function'",
      SEQ_REQ,G_TOKEN_STRING,&name,"missing function name",
      SEQ_REQ,')',NULL,"missing ')' afer 'function'",
      SEQ_REQ,'{',NULL,"missing '{' afer 'function'",
      SEQ_END);
  if(scanner->max_parse_errors)
    return g_free(name);

  actions = NULL;

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

  if(scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);

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

  value = config_get_value(scanner,"define",TRUE);
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

void config_trigger_action ( GScanner *scanner )
{
  gchar *trigger;
  struct layout_action *action;

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "missing trigger in TriggerAction"))
    return;
  g_scanner_get_next_token(scanner);
  trigger = g_strdup(scanner->value.v_string);
  if(!config_expect_token(scanner, ',',
        "missing ',' in TriggerAction"))
  {
    g_free(trigger);
    return;
  }

  g_scanner_get_next_token(scanner);
  action = config_action(scanner);
  if(!action)
  {
    g_free(trigger);
    g_scanner_error(scanner, "TriggerAction: invalid action");
    return;
  }

  action_trigger_add(action,trigger);
  config_optional_semicolon(scanner);
}

struct layout_widget *config_parse_toplevel ( GScanner *scanner,
    gboolean layout, gboolean toplevel )
{
  struct layout_widget *w=NULL, *dest;

  while(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_SCANNER:
        config_scanner(scanner);
        break;
      case G_TOKEN_LAYOUT:
        if(layout)
        {
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
            dest = bar_grid_by_name("sfwbar");
          config_layout(scanner,dest);
        }
        else
          g_scanner_error(scanner,"layout not supported in dynamic config");
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
struct layout_widget *config_parse_data ( gchar *fname, gchar *data,
    gboolean layout, gboolean toplevel )
{
  GScanner *scanner;
  struct layout_widget *w;
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
  g_scanner_scope_add_symbol(scanner,0, "XStep", (gpointer)G_TOKEN_XSTEP );
  g_scanner_scope_add_symbol(scanner,0, "YStep", (gpointer)G_TOKEN_YSTEP );
  g_scanner_scope_add_symbol(scanner,0, "XOrigin", (gpointer)G_TOKEN_XORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "YOrigin", (gpointer)G_TOKEN_YORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "Children", 
      (gpointer)G_TOKEN_CHILDREN );
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

  w = config_parse_toplevel ( scanner, layout, toplevel );
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
  config_parse_data("config string",conf,FALSE,FALSE);
  g_free(conf);
}

void config_pipe_read ( gchar *command )
{
  FILE *fp;
  gchar *conf;
  GIOChannel *chan;

  fp = popen(command, "r");
  if(!fp)
    return;

  chan = g_io_channel_unix_new( fileno(fp) );
  if(chan)
  {
    if(g_io_channel_read_to_end( chan , &conf,NULL,NULL)==G_IO_STATUS_NORMAL)
      config_parse_data(command,conf,FALSE,FALSE);
    g_free(conf);
    g_io_channel_unref(chan);
  }

  pclose(fp);
}

struct layout_widget *config_parse ( gchar *file, gboolean toplevel )
{
  gchar *fname;
  gchar *conf=NULL;
  gsize size;
  struct layout_widget *w=NULL;

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

  w = config_parse_data (fname, conf,TRUE,toplevel);

  g_free(conf);
  g_free(fname);
  return w;
}
