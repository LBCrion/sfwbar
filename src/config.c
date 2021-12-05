#include "sfwbar.h"
#include "config.h"
#include <fcntl.h>
#include <gtk-layer-shell.h>
#include <sys/stat.h>

extern gchar *confname;

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

void config_boolean_setbit ( GScanner *scanner, gint *dest, gint mask,
    gchar *expr )
{
  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '=')
    return g_scanner_error(scanner, "Missing '=' in %s = <boolean>",expr);
  g_scanner_get_next_token(scanner);

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_TRUE:
      *dest |= mask;
      break;
    case G_TOKEN_FALSE:
      *dest &= ~mask;
      break;
    default:
      g_scanner_error(scanner, "Missing <boolean> in %s = <boolean>",
          expr);
      break;
  }

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);
}

gchar *config_assign_string ( GScanner *scanner, gchar *expr )
{
  gchar *result;
  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '=')
  {
    g_scanner_error(scanner, "Missing '=' in %s = <string>",expr);
    return NULL;
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) != G_TOKEN_STRING)
  {
    g_scanner_error(scanner, "Missing <string> in %s = <string>",expr);
    return NULL;
  }
  g_scanner_get_next_token(scanner);

  result = g_strdup(scanner->value.v_string);

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

  return result;
}

gdouble config_assign_number ( GScanner *scanner, gchar *expr )
{
  gdouble result;

  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '=')
  {
    g_scanner_error(scanner, "Missing '=' in %s = <number>",expr);
    return 0;
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) != G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner, "Missing <number> in %s = <number>",expr);
    return 0;
  }
  g_scanner_get_next_token(scanner);
  result = scanner->value.v_float;

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

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

  if(g_scanner_peek_next_token(scanner) != '=')
  {
    g_scanner_error(scanner, "Missing '=' in %s = <parser>",vname);
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
  if(g_scanner_peek_next_token(scanner) != '(')
  {
    g_scanner_error(scanner, "Missing '(' in parser");
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

  if(g_scanner_peek_next_token(scanner) != ')')
  {
    g_scanner_error(scanner, "Missing ')' in parser");
    g_free(vname);
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

  var = g_malloc(sizeof(struct scan_var));
  var->json = NULL;
  var->regex = NULL;

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

  var->name = vname;
  var->file = file;
  var->type = type;
  var->multi = flag;
  var->val = 0;
  var->pval = 0;
  var->time = 0;
  var->ptime = 0;
  var->status = 0;
  var->str = NULL;

  file->vars = g_list_append(file->vars,var);
  context->scan_list = g_list_append(context->scan_list,var);
}

void config_scanner_source ( GScanner *scanner, gint source )
{
  gchar *fname=NULL;
  gint flags=0;
  struct scan_file *file;
  GList *find;

  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '(')
    return g_scanner_error(scanner, "Missing '(' after <source>");
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
    return g_scanner_error(scanner,"Missing <string> in source(<string>)");

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

  if(g_scanner_peek_next_token(scanner)!=')')
    g_scanner_error(scanner,"Missing ')' in source");
  else
    g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner)!='{')
    g_scanner_error(scanner,"Missing '{' after <source>");
  else
    g_scanner_get_next_token(scanner);

  if(scanner->max_parse_errors)
    return;

  if(fname==NULL)
    return;

  for(find=context->file_list;find;find=g_list_next(find))
    if(!g_strcmp0(fname,((struct scan_file *)(find->data))->fname))
      break;

  if(find!=NULL)
    file = find->data;
  else
    file = g_malloc(sizeof(struct scan_file));
  file->fname = fname;
  file->source = source;
  file->mtime = 0;
  file->flags = flags;
  file->vars = NULL;
  if( !strchr(fname,'*') && !strchr(fname,'?') )
    file->flags |= VF_NOGLOB;
  context->file_list = g_list_append(context->file_list,file);

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
}

void config_scanner ( GScanner *scanner )
{
  scanner->max_parse_errors = FALSE;

  if(g_scanner_peek_next_token(scanner) != '{')
    return g_scanner_error(scanner, "Missing '{' after 'scanner'");
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

  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '(')
  {
    g_scanner_error(scanner, "Missing '(' after loc");
    return rect;
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"Expecting x to be a <number> in loc(x,y[,w,h])");
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.x = scanner->value.v_float;

  if(g_scanner_peek_next_token(scanner) != ',')
  {
    g_scanner_error(scanner, "Missing ',' in loc");
    return rect;
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"Expecting y to be a <number> in loc(x,y[,w,h])");
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.y = scanner->value.v_float;

  if(g_scanner_peek_next_token(scanner)!=')')
  {
    if(g_scanner_peek_next_token(scanner) != ',')
    {
      g_scanner_error(scanner, "Missing ',' in loc");
      return rect;
    }
    g_scanner_get_next_token(scanner);

    if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
    {
      g_scanner_error(scanner,"Expecting w to be a <number> in loc(x,y[,w,h])");
      return rect;
    }
    g_scanner_get_next_token(scanner);
    rect.w = MAX(1,scanner->value.v_float);

    if(g_scanner_peek_next_token(scanner) != ',')
    {
      g_scanner_error(scanner, "Missing ',' in loc");
      return rect;
    }
    g_scanner_get_next_token(scanner);

    if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
    {
      g_scanner_error(scanner,"Expecting h to be a <number> in loc(x,y[,w,h])");
      return rect;
    }
    g_scanner_get_next_token(scanner);
    rect.h = MAX(1,scanner->value.v_float);
  }
  if(g_scanner_peek_next_token(scanner) != ')')
  {
    g_scanner_error(scanner, "Missing ')' after loc");
    return rect;
  }
  g_scanner_get_next_token(scanner);


  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

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

gchar *config_get_value ( GScanner *scanner )
{
  gchar *value, *temp;

  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner)!='=')
  {
    g_scanner_error(scanner,"expecting value = expression");
    return NULL;
  }
  g_scanner_get_next_token(scanner);
  value = g_strdup("");;
  while(((gint)g_scanner_peek_next_token(scanner)<=G_TOKEN_SCANNER)&&
      (g_scanner_peek_next_token(scanner)!='}')&&
      (g_scanner_peek_next_token(scanner)!=';')&&
      (g_scanner_peek_next_token(scanner)!=G_TOKEN_EOF))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_STRING:
        value = config_value_string(value, scanner->value.v_string);
        break;
      case G_TOKEN_IDENTIFIER:
        temp = value;
        value = g_strconcat(value, scanner->value.v_identifier, NULL);
        g_free(temp);
        break;
      case G_TOKEN_TIME:
      case G_TOKEN_MIDW:
      case G_TOKEN_EXTRACT:
      case G_TOKEN_DF:
      case G_TOKEN_VAL:
      case G_TOKEN_STRW:
        temp = value;
        value = g_strconcat(value,expr_token[scanner->token], NULL);
        g_free(temp);
        break;
      case G_TOKEN_FLOAT:
        temp = value;
        value = g_strdup_printf("%s%f",value,scanner->value.v_float);
        g_free(temp);
        break;
      default:
        temp = value;
        value = g_strdup_printf("%s%c",value,scanner->token);
        g_free(temp);
        break;
    }
  }
  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);
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
  if(g_scanner_peek_next_token(scanner)!='=')
  {
    g_scanner_error(scanner,"expecting pins = string [,string]");
    return;
  }
  do
  {
    g_scanner_get_next_token(scanner);
    if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
    {
      g_scanner_error(scanner,"expecting a string in pins = string [,string]");
      break;
    }
    g_scanner_get_next_token(scanner);
    context->pager_pins = g_list_append(context->pager_pins,
        g_strdup(scanner->value.v_string));
  } while ( g_scanner_peek_next_token(scanner)==',');
  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);
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

void config_widget_props ( GScanner *scanner, struct layout_widget *lw )
{
  scanner->max_parse_errors = FALSE;

  if( g_scanner_peek_next_token( scanner ) != '{')
    return layout_widget_config(lw);
  else
    g_scanner_get_next_token(scanner);

  while (!(( (gint)g_scanner_peek_next_token ( scanner ) >= G_TOKEN_GRID )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) <= G_TOKEN_TRAY ))&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_STYLE:
        lw->style = config_assign_string(scanner,"style");
        break;
      case G_TOKEN_CSS:
        lw->css = config_assign_string(scanner,"css");
        break;
      case G_TOKEN_INTERVAL:
        if(GTK_IS_GRID(lw->widget))
        {
          g_scanner_error(scanner,"this widget has no property 'interval'");
          break;
        }
        lw->interval = 1000*config_assign_number(scanner, "interval");
        break;
      case G_TOKEN_VALUE:
        if(GTK_IS_GRID(lw->widget))
        {
          g_scanner_error(scanner,"this widget has no property 'value'");
          break;
        }
        lw->value = config_get_value(scanner);
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
        config_boolean_setbit(scanner,&context->features,F_PA_RENDER,"preview");
        break;
      case G_TOKEN_COLS:
        config_widget_cols(scanner, lw);
        break;
      case G_TOKEN_ROWS:
        config_widget_rows(scanner, lw);
        break;
      case G_TOKEN_ACTION:
        lw->action = config_assign_string(scanner,"action");
        break;
      case G_TOKEN_ICONS:
        config_boolean_setbit(scanner,&context->features,F_TB_ICON,"icons");
        break;
      case G_TOKEN_LABELS:
        config_boolean_setbit(scanner,&context->features,F_TB_LABEL,"labels");
        break;
      case G_TOKEN_ICON:
        lw->icon = config_assign_string(scanner,"action");
        break;
      case G_TOKEN_LOC:
        lw->rect = config_get_loc(scanner);
        break;
      default:
        g_scanner_error(scanner, "Unexpected token in widget definition");
    }
  }
  if((gint)g_scanner_peek_next_token(scanner) == '}')
    g_scanner_get_next_token(scanner);

  return layout_widget_config(lw);
}

struct layout_widget *config_include ( GScanner *scanner )
{
  struct layout_widget *lw;

  scanner->max_parse_errors = FALSE;
  if(g_scanner_peek_next_token(scanner) != '(')
  {
    g_scanner_error(scanner, "Missing '(' after include");
    return NULL;
  }
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
  {
    g_scanner_error(scanner, "Missing <string> in include(<string>)");
    return NULL;
  }
  g_scanner_get_next_token(scanner);
  lw = config_parse(scanner->value.v_string);
  lw->wtype = G_TOKEN_INCLUDE;

  if(g_scanner_peek_next_token(scanner) != ')')
    g_scanner_error(scanner, "Missing ')' after include");
  g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

  return lw;
}

void config_widgets ( GScanner *scanner, GtkWidget *parent )
{
  GtkWidget *sibling=NULL;
  gint dir = GTK_POS_RIGHT;
  struct layout_widget *lw;

  gtk_widget_style_get(parent,"direction",&dir,NULL);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
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
        break;
      case G_TOKEN_TRAY:
        scanner->max_parse_errors=FALSE;
        lw->widget = flow_grid_new(TRUE);
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'layout'");
        continue;
    }
    if(!lw)
      continue;
    if(scanner->max_parse_errors || !lw->widget)
    {
      layout_widget_free(lw);
      continue;
    }
    config_widget_props( scanner, lw);
    if( (lw->rect.x < 1) || (lw->rect.y < 1 ) )
      gtk_grid_attach_next_to(GTK_GRID(parent),lw->lobject,sibling,dir,1,1);
    else
      gtk_grid_attach(GTK_GRID(parent),lw->lobject,
          lw->rect.x,lw->rect.y,lw->rect.w,lw->rect.h);
    sibling = lw->lobject;

    if(lw->wtype == G_TOKEN_GRID)
      config_widgets(scanner,lw->widget);

    if(lw->value)
      context->widgets = g_list_append(context->widgets,lw);
    else
      layout_widget_free(lw);
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

struct layout_widget *config_layout ( GScanner *scanner )
{
  struct layout_widget *lw;

  scanner->max_parse_errors=FALSE;

  lw = layout_widget_new();
  lw->widget = gtk_grid_new();
  gtk_widget_set_name(lw->widget,"layout");

  config_widget_props(scanner, lw);
  config_widgets(scanner, lw->widget);

  return lw;
}

void config_switcher ( GScanner *scanner )
{
  gchar *css=NULL;
  GtkWidget *win, *box;
  gint interval = 1;
  scanner->max_parse_errors = FALSE;

  if(g_scanner_peek_next_token(scanner)!='{')
    return g_scanner_error(scanner,"Missing '{' after 'switcher'");
  g_scanner_get_next_token(scanner);

  context->features |= F_SWITCHER;
  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW(win));
  gtk_layer_set_layer(GTK_WINDOW(win),GTK_LAYER_SHELL_LAYER_OVERLAY);
  box = flow_grid_new(FALSE);
  gtk_widget_set_name(box, "switcher");
  gtk_widget_set_name(win, "switcher");
  gtk_container_add(GTK_CONTAINER(win),box);
  flow_grid_set_cols(box,1);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_INTERVAL: 
        interval = config_assign_number(scanner,"interval")/100;
        break;
      case G_TOKEN_COLS: 
        flow_grid_set_cols(box,
            config_assign_number(scanner,"cols"));
        break;
      case G_TOKEN_CSS:
        g_free(css);
        css = config_assign_string(scanner,"css");
        break;
      case G_TOKEN_ICONS:
        config_boolean_setbit(scanner,&context->features,F_SW_ICON,"icons");
        break;
      case G_TOKEN_LABELS:
        config_boolean_setbit(scanner,&context->features,F_SW_LABEL,"labels");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'switcher'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

  if(!(context->features & F_SW_ICON))
    context->features |= F_SW_LABEL;

  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (box);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }
  switcher_config(win,box,interval);
}

void config_placer ( GScanner *scanner )
{
  scanner->max_parse_errors = FALSE;

  if(g_scanner_peek_next_token(scanner)!='{')
    return g_scanner_error(scanner,"Missing '{' after 'placer'");
  g_scanner_get_next_token(scanner);

  context->features |= F_PLACEMENT;
  context->wp_x= 10;
  context->wp_y= 10;
  context->wo_x= 0;
  context->wo_y= 0;
  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token(scanner) )
    {
      case G_TOKEN_XSTEP: 
        context->wp_x = config_assign_number ( scanner, "xstep" );
        break;
      case G_TOKEN_YSTEP: 
        context->wp_y = config_assign_number ( scanner, "ystep" );
        break;
      case G_TOKEN_XORIGIN: 
        context->wo_x = config_assign_number ( scanner, "xorigin" );
        break;
      case G_TOKEN_YORIGIN: 
        context->wo_y = config_assign_number ( scanner, "yorigin" );
        break;
      case G_TOKEN_CHILDREN:
        config_boolean_setbit(scanner,&context->features,F_PL_CHKPID,"children");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'placer'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);

  if(context->wp_x<1)
    context->wp_x=1;
  if(context->wp_y<1)
    context->wp_y=1;
}

struct layout_widget *config_parse_toplevel ( GScanner *scanner )
{
  struct layout_widget *w=NULL;

  while(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_SCANNER:
        config_scanner(scanner);
        break;
      case G_TOKEN_LAYOUT:
        layout_widget_free(w);
        w = config_layout(scanner);
        break;
      case G_TOKEN_PLACER:
        config_placer(scanner);
        break;
      case G_TOKEN_SWITCHER:
        config_switcher(scanner);
        break;
      default:
        g_scanner_error(scanner,"Unexpected toplevel token");
        break;
    }
  }
  return w;
}

struct layout_widget *config_parse ( gchar *file )
{
  GScanner *scanner;
  gchar *fname;
  gchar *tmp;
  gchar *conf=NULL;
  GtkCssProvider *css;
  gsize size;
  gint dir;
  struct layout_widget *w=NULL;

  fname = get_xdg_config_file(file);
  if(fname)
    if(!g_file_get_contents(fname,&conf,&size,NULL))
      conf=NULL;

  if(!conf)
    {
      g_error("Error: can't read config file\n");
      exit(1);
    }

  tmp = g_strstr_len(conf,size,"\n#CSS");
  if(tmp)
  {
    *tmp=0;
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,tmp+5,strlen(tmp+5),NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
  }

  gtk_widget_style_get(GTK_WIDGET(context->window),"direction",&dir,NULL);
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_LEFT,!(dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_RIGHT,!(dir==GTK_POS_LEFT));
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_BOTTOM,!(dir==GTK_POS_TOP));
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_TOP,!(dir==GTK_POS_BOTTOM));


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
  g_scanner_scope_add_symbol(scanner,0, "Switcher", (gpointer)G_TOKEN_SWITCHER );
  g_scanner_scope_add_symbol(scanner,0, "End", (gpointer)G_TOKEN_END );
  g_scanner_scope_add_symbol(scanner,0, "File", (gpointer)G_TOKEN_FILE );
  g_scanner_scope_add_symbol(scanner,0, "Exec", (gpointer)G_TOKEN_EXEC );
  g_scanner_scope_add_symbol(scanner,0, "Number", (gpointer)G_TOKEN_NUMBERW );
  g_scanner_scope_add_symbol(scanner,0, "String", (gpointer)G_TOKEN_STRINGW );
  g_scanner_scope_add_symbol(scanner,0, "NoGlob", (gpointer)G_TOKEN_NOGLOB );
  g_scanner_scope_add_symbol(scanner,0, "CheckTime", (gpointer)G_TOKEN_CHTIME );
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
  g_scanner_scope_add_symbol(scanner,0, "Interval", (gpointer)G_TOKEN_INTERVAL );
  g_scanner_scope_add_symbol(scanner,0, "Value", (gpointer)G_TOKEN_VALUE );
  g_scanner_scope_add_symbol(scanner,0, "Pins", (gpointer)G_TOKEN_PINS );
  g_scanner_scope_add_symbol(scanner,0, "Preview", (gpointer)G_TOKEN_PREVIEW );
  g_scanner_scope_add_symbol(scanner,0, "Cols", (gpointer)G_TOKEN_COLS );
  g_scanner_scope_add_symbol(scanner,0, "Rows", (gpointer)G_TOKEN_ROWS );
  g_scanner_scope_add_symbol(scanner,0, "Action", (gpointer)G_TOKEN_ACTION );
  g_scanner_scope_add_symbol(scanner,0, "Display", (gpointer)G_TOKEN_DISPLAY );
  g_scanner_scope_add_symbol(scanner,0, "Icons", (gpointer)G_TOKEN_ICONS );
  g_scanner_scope_add_symbol(scanner,0, "Labels", (gpointer)G_TOKEN_LABELS );
  g_scanner_scope_add_symbol(scanner,0, "Icon", (gpointer)G_TOKEN_ICON );
  g_scanner_scope_add_symbol(scanner,0, "Loc", (gpointer)G_TOKEN_LOC );
  g_scanner_scope_add_symbol(scanner,0, "XStep", (gpointer)G_TOKEN_XSTEP );
  g_scanner_scope_add_symbol(scanner,0, "YStep", (gpointer)G_TOKEN_YSTEP );
  g_scanner_scope_add_symbol(scanner,0, "XOrigin", (gpointer)G_TOKEN_XORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "YOrigin", (gpointer)G_TOKEN_YORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "Children", (gpointer)G_TOKEN_CHILDREN );
  g_scanner_scope_add_symbol(scanner,0, "True", (gpointer)G_TOKEN_TRUE );
  g_scanner_scope_add_symbol(scanner,0, "False", (gpointer)G_TOKEN_FALSE );
  g_scanner_scope_add_symbol(scanner,0, "RegEx", (gpointer)G_TOKEN_REGEX );
  g_scanner_scope_add_symbol(scanner,0, "Json", (gpointer)G_TOKEN_JSON );
  g_scanner_scope_add_symbol(scanner,0, "Grab", (gpointer)G_TOKEN_GRAB );

  scanner->input_name = fname;
  g_scanner_input_text( scanner, conf, -1 );

  w = config_parse_toplevel ( scanner );
  g_free(conf);
  g_free(fname);
  return w;
}
