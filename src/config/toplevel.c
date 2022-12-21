
#include "../config.h"
#include "../menu.h"
#include "../sfwbar.h"
#include "../module.h"

static GHashTable *defines;

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
      case G_TOKEN_USERSTATE2:
        *ptr |= WS_USERSTATE2;
        break;
      case G_TOKEN_CHILDREN:
        *ptr |= WS_CHILDREN;
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
    case G_TOKEN_IDENTIFIER:
      action->ident = g_strdup(scanner->value.v_identifier);
    case G_TOKEN_EXEC:
    case G_TOKEN_MENU:
    case G_TOKEN_MENUCLEAR:
    case G_TOKEN_PIPEREAD:
    case G_TOKEN_SWAYCMD:
    case G_TOKEN_SWAYWIN:
    case G_TOKEN_MPDCMD:
    case G_TOKEN_IDLEINHIBIT:
    case G_TOKEN_USERSTATE:
    case G_TOKEN_USERSTATE2:
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

void config_menu_items ( GScanner *scanner, GtkWidget *menu )
{
  GtkWidget *item, *submenu;
  gchar *itemname, *subname;
  gboolean items;

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
        itemname = NULL;
        subname = NULL;
        items = FALSE;
        config_parse_sequence(scanner,
            SEQ_REQ,'(',NULL,NULL,"missing '(' after 'submenu'",
            SEQ_REQ,G_TOKEN_STRING,NULL,&itemname,"missing submenu title",
            SEQ_OPT,',',NULL,NULL,NULL,
            SEQ_CON,G_TOKEN_STRING,NULL,&subname,"missing submenu name",
            SEQ_REQ,')',NULL,NULL,"missing ')' afer 'submenu'",
            SEQ_OPT,'{',NULL,&items,"missing '{' afer 'submenu'",
            SEQ_END);
        if(!scanner->max_parse_errors && itemname)
        {
          item = menu_item_new(itemname,NULL);
          if(subname)
            submenu = menu_new(subname);
          else
            submenu = gtk_menu_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);
          if(items)
            config_menu_items(scanner,submenu);
        }
        g_free(itemname);
        g_free(subname);
        break;
      default:
        g_scanner_error(scanner,
            "Unexpected token in menu. Expecting a menu item");
        break;
    }
    if(item)
      gtk_container_add(GTK_CONTAINER(menu),item);
    g_scanner_peek_next_token(scanner);
  }
  if(scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

void config_menu ( GScanner *scanner )
{
  gchar *name = NULL;
  GtkWidget *menu;
  gboolean items;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'menu'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&name,"missing menu name",
      SEQ_REQ,')',NULL,NULL,"missing ')' afer 'menu'",
      SEQ_REQ,'{',NULL,&items,"missing '{' afer 'menu'",
      SEQ_END);
  if(!scanner->max_parse_errors && name)
  {
    menu = menu_new(name);
    if(items)
      config_menu_items(scanner, menu);
  }
  g_free(name);
  config_optional_semicolon(scanner);
}

void config_menu_clear ( GScanner *scanner )
{
  gchar *name = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'menu'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&name,"missing menu name",
      SEQ_REQ,')',NULL,NULL,"missing ')' afer 'menu'",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);
  if(!scanner->max_parse_errors && name)
    menu_remove(name);
  g_free(name);
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
      SEQ_OPT,'{',NULL,NULL,"missing '{' afer 'function'",
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

void config_module ( GScanner *scanner )
{
  gchar *name = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"missing '(' after 'module'",
      SEQ_REQ,G_TOKEN_STRING,NULL,&name,"missing module name",
      SEQ_REQ,')',NULL,NULL,"missing ')' afer 'module'",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);
  if(scanner->max_parse_errors || !name)
    return;

  module_load ( name );
  g_free(name);

}

GtkWidget *config_parse_toplevel ( GScanner *scanner, gboolean toplevel )
{
  GtkWidget *w=NULL;

  while(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_SCANNER:
        config_scanner(scanner);
        break;
      case G_TOKEN_LAYOUT:
        config_layout(scanner,&w,toplevel);
        break;
      case G_TOKEN_PLACER:
        config_placer(scanner);
        break;
      case G_TOKEN_SWITCHER:
        config_switcher(scanner);
        break;
      case G_TOKEN_MENU:
        config_menu(scanner);
        break;
      case G_TOKEN_MENUCLEAR:
        config_menu_clear(scanner);
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
      case G_TOKEN_MODULE:
        config_module(scanner);
        break;
      default:
        g_scanner_error(scanner,"Unexpected toplevel token");
        break;
    }
  }
  return w;
}
