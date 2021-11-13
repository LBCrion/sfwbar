#include "sfwbar.h"
#include "config.h"
#include <fcntl.h>
#include <gtk-layer-shell.h>
#include <sys/stat.h>

extern gchar *confname;


void config_boolean_setbit ( GScanner *scanner, gint *dest, gint mask,  gchar *expr,
    gboolean *err)
{
  *err = parser_expect_symbol ( scanner, '=', expr);
  if(*err)
    return;
  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_TRUE:
      *dest |= mask;
      break;
    case G_TOKEN_FALSE:
      *dest &= ~mask;
      break;
    default:
      g_scanner_error(scanner, "Unexpected token, expecting a boolean in %s",
          expr);
      *err = TRUE;
      break;
  }
}

gchar *config_assign_string ( GScanner *scanner, gchar *expr, gboolean *err )
{
  *err = parser_expect_symbol ( scanner, '=', expr);
  if(*err)
    return NULL;
  if(g_scanner_peek_next_token(scanner) != G_TOKEN_STRING)
  {
    g_scanner_error(scanner, "Unexpected token, expecting a string in %s",expr);
    *err = TRUE;
    return NULL;
  }
  g_scanner_get_next_token(scanner);
  *err = FALSE;
  return g_strdup(scanner->value.v_string);
}

gdouble config_assign_number ( GScanner *scanner, gchar *expr, gboolean *err )
{
  *err = parser_expect_symbol ( scanner, '=', expr);
  if(*err)
    return 0;
  if(g_scanner_peek_next_token(scanner) != G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner, "Unexpected token, expecting a number in %s",expr);
    *err = TRUE;
    return 0;
  }
  g_scanner_get_next_token(scanner);
  *err = FALSE;
  return scanner->value.v_float;
}

gboolean config_scanner_var ( GScanner *scanner, struct scan_file *file )
{
  gboolean err;
  struct scan_var *var;
  gchar *vname, *pattern;
  guchar type;
  gint flag = SV_REPLACE;

  g_scanner_get_next_token(scanner);
  vname = g_strdup(scanner->value.v_identifier);
  err = parser_expect_symbol(scanner,'=',"Identifier = Source(..)");
  g_scanner_get_next_token(scanner);

  if(((gint)scanner->token < G_TOKEN_REGEX)||
      ((gint)scanner->token > G_TOKEN_GRAB))
  {
    if(!err)
      g_scanner_error(scanner,"Expecting a Parser");
    return TRUE;
  }
  else
  {
    type = scanner->token - G_TOKEN_REGEX;
    err = parser_expect_symbol(scanner,'(',"Parser(String[,Flag])");

    if(type != VP_GRAB)
    {
      if(g_scanner_get_next_token(scanner)!=G_TOKEN_STRING)
      {
        if(!err)
          g_scanner_error(scanner,"Expecting a String in Parser(string,[Flag])");
        g_free(vname);
        return TRUE;
      }
      else
      {
        pattern = g_strdup(scanner->value.v_string);
      }
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
      {
        g_scanner_get_next_token(scanner);
        if(!err)
          g_scanner_error(scanner,"Expecting a variable flag after a comma");
        err = TRUE;
      }
    }
    err = parser_expect_symbol(scanner,')',"Source(String,Flag)");
  }

  var = g_malloc(sizeof(struct scan_var));

  switch(type)
  {
    case VP_JSON:
      var->json = pattern;
      file->flags |= VF_FINAL;
      break;
    case VP_REGEX:
      var->regex = g_regex_new(pattern,0,0,NULL);
      g_free(pattern);
      file->flags |= VF_CONCUR;
      break;
    case VP_GRAB:
      file->flags |= VF_CONCUR;
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
  return err;
}

gboolean config_scanner_source ( GScanner *scanner, gint source )
{
  gboolean err;
  gchar *fname=NULL;
  gint flags=0;
  struct scan_file *file;
  GList *find;

  err = parser_expect_symbol(scanner,'(',"File(string)");
  if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
  {
    g_scanner_get_next_token(scanner);
    fname = g_strdup(scanner->value.v_string);
  }
  else
  {
    g_scanner_get_next_token(scanner);
    if(!err)
      g_error("Expecting a string in File(string)");
    err = TRUE;
  }
  if(source == SO_FILE)
    while((gint)g_scanner_peek_next_token(scanner)==',')
    {
      g_scanner_get_next_token(scanner);
      if(((gint)g_scanner_peek_next_token(scanner)>=G_TOKEN_NOGLOB)&&
          ((gint)g_scanner_peek_next_token(scanner)<=G_TOKEN_CHTIME))
      {
        g_scanner_get_next_token(scanner);
        flags |= (1>>(scanner->token - G_TOKEN_NOGLOB));
      }
      else
      {
        g_scanner_get_next_token(scanner);
        if(!err)
          g_scanner_error(scanner, "Expecting a file flag");
        err = TRUE;
      }
    } 

  err = parser_expect_symbol(scanner,')',"Missing ')' in File(string[,flag])");

  err = parser_expect_symbol(scanner,'{',"file(string) { ... }");
  if(err)
    return err;

  if(fname==NULL)
    return err;

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
  context->file_list = g_list_append(context->file_list,file);

  while(((gint)g_scanner_peek_next_token(scanner)!='}')&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch((gint)g_scanner_peek_next_token(scanner))
    {
      case G_TOKEN_IDENTIFIER:
        err = config_scanner_var(scanner, file);
        break;
      default:
        g_scanner_get_next_token(scanner);
        if(!err)
          g_scanner_error(scanner, "Expecting a variable declaration or End");
        err = TRUE;
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
  return err;
}

void config_scanner ( GScanner *scanner )
{
  gboolean err = FALSE;

  err = parser_expect_symbol(scanner,'{',"scanner { ... }");
  if(err)
    return;

  while(((gint)g_scanner_peek_next_token(scanner) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_FILE:
        err = config_scanner_source(scanner,SO_FILE);
        break;
      case G_TOKEN_EXEC:
        err = config_scanner_source(scanner,SO_EXEC);
        break;
      default:
        if(!err)
          g_scanner_error(scanner, "Unexpected declaration in scanner");
        err = TRUE;
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

struct rect config_get_loc ( GScanner *scanner, gboolean *err )
{
  struct rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.w = 0;
  rect.h = 0;
  *err = parser_expect_symbol(scanner, '(', ".loc(x,y[,w,h])");
  if(*err)
    return rect;
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"expecting a number in .loc");
    *err=TRUE;
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.x = scanner->value.v_float;
  *err = parser_expect_symbol(scanner, ',', ".loc(x,y[,w,h])");
  if(*err)
    return rect;
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"expecting a number in .loc");
    *err=TRUE;
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.y = scanner->value.v_float;
  if(g_scanner_peek_next_token(scanner)==')')
  {
    g_scanner_get_next_token(scanner);
    return rect;
  }
  *err = parser_expect_symbol(scanner, ',', ".loc(x,y[,w,h])");
  if(*err)
    return rect;
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"expecting a number in .loc");
    *err=TRUE;
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.w = scanner->value.v_float;
  *err = parser_expect_symbol(scanner, ',', ".loc(x,y[,w,h])");
  if(*err)
    return rect;
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_FLOAT)
  {
    g_scanner_error(scanner,"expecting a number in .loc");
    *err=TRUE;
    return rect;
  }
  g_scanner_get_next_token(scanner);
  rect.h = scanner->value.v_float;
  *err = parser_expect_symbol(scanner, ')', ".loc(x,y[,w,h])");

  return rect;
}

gchar *config_get_value ( GScanner *scanner, gboolean *err )
{
  gchar *value, *temp;
  if(g_scanner_peek_next_token(scanner)!='=')
  {
    g_scanner_error(scanner,"expecting value = expression");
    *err = TRUE;
    return NULL;
  }
  g_scanner_get_next_token(scanner);
  value = g_strdup("");;
  while(((gint)g_scanner_peek_next_token(scanner)<=G_TOKEN_SCANNER)&&
      (g_scanner_peek_next_token(scanner)!='}')&&
      (g_scanner_peek_next_token(scanner)!=G_TOKEN_EOF))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_STRING:
        temp = value;
        value = g_strconcat(value,"\"", scanner->value.v_string, "\"", NULL);
        g_free(temp);
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
  return value;
}

gboolean config_widget_props ( GScanner *scanner, struct layout_widget *lw)
{
  gboolean err;

  err = FALSE;

  if( g_scanner_peek_next_token( scanner ) != '{')
    return FALSE;
  else
    g_scanner_get_next_token(scanner);

  while (!(( (gint)g_scanner_peek_next_token ( scanner ) >= G_TOKEN_GRID )&&
    ((gint)g_scanner_peek_next_token ( scanner ) <= G_TOKEN_TRAY ))&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    err = FALSE;
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_STYLE:
        lw->style = config_assign_string(scanner,"style = String",&err);
        break;
      case G_TOKEN_CSS:
        lw->css = config_assign_string(scanner,"css = String",&err);
        break;
      case G_TOKEN_INTERVAL:
        lw->interval = config_assign_number(scanner, "interval = Number",&err);
        break;
      case G_TOKEN_VALUE:
        lw->value = config_get_value(scanner,&err);
        break;
      case G_TOKEN_PINS:
        if(lw->wtype != G_TOKEN_PAGER)
        {
          g_scanner_error(scanner,"this widget has no property 'pins'");
          err = TRUE;
          break;
        }
        if(g_scanner_peek_next_token(scanner)!='=')
        {
          g_scanner_error(scanner,"expecting pins = string [,string]");
          err = TRUE;
          break;
        }
        do {
          g_scanner_get_next_token(scanner);
          if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
          {
            g_scanner_error(scanner,"expecting a string in pins = string [,string]");
            err = TRUE;
            break;
          }
          g_scanner_get_next_token(scanner);
          context->pager_pins = g_list_append(context->pager_pins,
              g_strdup(scanner->value.v_string));
        } while ( g_scanner_peek_next_token(scanner)==',');
        break;
      case G_TOKEN_PREVIEW:
        if(lw->wtype != G_TOKEN_PAGER)
        {
          g_scanner_error(scanner,"this widget has no property 'preview'");
          err = TRUE;
          break;
        }
        config_boolean_setbit(scanner,&context->features,F_PA_RENDER,"preview = true|false",&err);
        break;
      case G_TOKEN_COLS:
        switch(lw->wtype)
        {
          case G_TOKEN_TASKBAR:
            context->tb_cols =
              config_assign_number(scanner, "cols = Number",&err);
            break;
          case G_TOKEN_PAGER:
            context->pager_cols =
              config_assign_number(scanner, "cols = Number",&err);
            break;
          default:
            g_scanner_error(scanner,"this widget has no property 'cols'");
            err = TRUE;
            break;
        }
        break;
      case G_TOKEN_ROWS:
        switch(lw->wtype)
        {
          case G_TOKEN_TASKBAR:
            context->tb_rows =
              config_assign_number(scanner, "rows = Number",&err);
            break;
          case G_TOKEN_PAGER:
            context->pager_rows =
              config_assign_number(scanner, "rows = Number",&err);
            break;
          default:
            g_scanner_error(scanner,"this widget has no property 'rows'");
            err = TRUE;
            break;
        }
        break;
      case G_TOKEN_ACTION:
        lw->action = config_assign_string(scanner,"action = String",&err);
        break;
      case G_TOKEN_ICONS:
        config_boolean_setbit(scanner,&context->features,F_TB_ICON,"icons = true|false",&err);
        break;
      case G_TOKEN_LABELS:
        config_boolean_setbit(scanner,&context->features,F_TB_LABEL,"labels = true|false",&err);
        break;
      case G_TOKEN_ICON:
        lw->icon = config_assign_string(scanner,"action = String",&err);
        break;
      case G_TOKEN_LOC:
        lw->rect = config_get_loc(scanner,&err);
        break;
      default:
        if(!err)
          g_scanner_error(scanner, "Widget: unexpected token");
    }
  }
  if((gint)g_scanner_peek_next_token(scanner) == '}')
    g_scanner_get_next_token(scanner);

  layout_widget_config(lw);

  return err;
}

GtkWidget *config_include ( GScanner *scanner, gboolean *err )
{
  GtkWidget *widget;
  *err = parser_expect_symbol(scanner, '(', "Include(String)");
  if(*err)
      return NULL;
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
  {
    g_scanner_error(scanner, "Expecting a String in Include(String)");
    return NULL;
  }
  g_scanner_get_next_token(scanner);
  widget = config_parse(scanner->value.v_string);
  *err = parser_expect_symbol(scanner, ')', "Include(String)");
  return widget;
}

void config_widgets ( GScanner *scanner, GtkWidget *parent );

void config_widgets ( GScanner *scanner, GtkWidget *parent )
{
  gboolean err;
  GtkWidget *sibling=NULL;
  gint dir = GTK_POS_RIGHT;
  struct layout_widget *lw;

  gtk_widget_style_get(parent,"direction",&dir,NULL);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    lw = layout_widget_new();
    lw->wtype = g_scanner_get_next_token(scanner);
    err = FALSE;
    switch ( lw->wtype )
    {
      case G_TOKEN_GRID:
        lw->widget = gtk_grid_new();
        break;
      case G_TOKEN_LABEL:
        lw->widget = gtk_label_new("");
        break;
      case G_TOKEN_IMAGE:
        lw->widget = scale_image_new();
        break;
      case G_TOKEN_BUTTON:
        lw->widget = gtk_button_new();
        break;
      case G_TOKEN_SCALE:
        lw->widget = gtk_progress_bar_new();
        break;
      case G_TOKEN_INCLUDE:
        lw->widget = config_include( scanner, &err );
        break;
      case G_TOKEN_TASKBAR:
        lw->widget = clamp_grid_new();
        break;
      case G_TOKEN_PAGER:
        lw->widget = gtk_grid_new();
        break;
      case G_TOKEN_TRAY:
        lw->widget = gtk_grid_new();
        break;
      default:
        if(!err)
          g_scanner_error(scanner,"Layout: unexpected token");
        err = TRUE;
        continue;
    }
    if(!err)
    {
      err = config_widget_props( scanner, lw);
      if(!err)
      {
        if(lw->rect.w<1)
          lw->rect.w=1;
        if(lw->rect.h<1)
          lw->rect.h=1;
        if((lw->rect.x<1)||(lw->rect.y<1))
          gtk_grid_attach_next_to(GTK_GRID(parent),lw->widget,sibling,dir,1,1);
        else
          gtk_grid_attach(GTK_GRID(parent),lw->widget,
              lw->rect.x,lw->rect.y,lw->rect.w,lw->rect.h);
        if(lw->value)
          context->widgets = g_list_append(context->widgets,lw);
        sibling = lw->widget;
      }
      if(lw->wtype == G_TOKEN_GRID)
        config_widgets(scanner,lw->widget);
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);
}

GtkWidget *config_layout ( GScanner *scanner )
{
  struct layout_widget *lw;

  lw = layout_widget_new();

  lw->widget = gtk_grid_new();
  gtk_widget_set_name(lw->widget,"layout");
  config_widget_props(scanner, lw);

  config_widgets(scanner, lw->widget);

  return lw->widget;
}

void config_switcher ( GScanner *scanner )
{
  gchar *css=NULL;
  gboolean err = FALSE;

  err = parser_expect_symbol(scanner,'{',"switcher { ... }");
  if(err)
    return;

  context->features |= F_SWITCHER;
  context->sw_max = 1;
  context->sw_cols = 1;

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_INTERVAL: 
        context->sw_max = 1000*
          config_assign_number(scanner,"interval = number",&err);
        break;
      case G_TOKEN_COLS: 
        context->sw_cols = config_assign_number(scanner,"cols = number",&err);
        break;
      case G_TOKEN_CSS:
        g_free(css);
        css = config_assign_string(scanner,"css = string",&err);
        break;
      case G_TOKEN_ICONS:
        config_boolean_setbit(scanner,&context->features,F_SW_ICON,"icons = true|false",&err);
        break;
      case G_TOKEN_LABELS:
        config_boolean_setbit(scanner,&context->features,F_SW_LABEL,"labels = true|false",&err);
        break;
      default:
        if(!err)
          g_scanner_error(scanner,"Switcher: unexpected token");
        err = TRUE;
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  if(!(context->features & F_SW_ICON))
    context->features |= F_SW_LABEL;
  context->sw_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW(context->sw_win));
  gtk_layer_set_layer(GTK_WINDOW(context->sw_win),GTK_LAYER_SHELL_LAYER_OVERLAY);
  context->sw_box = gtk_grid_new();
  gtk_widget_set_name(context->sw_box, "switcher");
  gtk_widget_set_name(context->sw_win, "switcher");
  gtk_container_add(GTK_CONTAINER(context->sw_win),context->sw_box);
  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (context->sw_box);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }
}

void config_placer ( GScanner *scanner )
{
  gboolean err = FALSE;

  err = parser_expect_symbol(scanner,'{',"placer { ... }");
  if(err)
    return;

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
        context->wp_x = config_assign_number ( scanner, "xstep = number", &err );
        break;
      case G_TOKEN_YSTEP: 
        context->wp_y = config_assign_number ( scanner, "ystep = number", &err );
        break;
      case G_TOKEN_XORIGIN: 
        context->wo_x = config_assign_number ( scanner, "xorigin = number", &err );
        break;
      case G_TOKEN_YORIGIN: 
        context->wo_y = config_assign_number ( scanner, "yorigin = number", &err );
        break;
      case G_TOKEN_CHILDREN:
        config_boolean_setbit(scanner,&context->features,F_PL_CHKPID,"children = true|false",&err);
        break;
      default:
        if(!err)
          g_scanner_error(scanner,"Placer: unexpected token");
        err = TRUE;
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  if(context->wp_x<1)
    context->wp_x=1;
  if(context->wp_y<1)
    context->wp_y=1;
}

GtkWidget *config_parse_toplevel ( GScanner *scanner )
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

GtkWidget *config_parse ( gchar *file )
{
  GScanner *scanner;
  gchar *fname;
  gchar *tmp;
  gchar *conf=NULL;
  GtkCssProvider *css;
  gsize size;
  gint dir;
  GtkWidget *w=NULL;

  fname = get_xdg_config_file(file);
  if(fname)
    g_file_get_contents(fname,&conf,&size,NULL);

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
