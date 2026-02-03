/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "exec.h"

static void (*exec)(const gchar *);
static const struct {
  const gchar *exec;
  const gchar *arg;
} terms[] = {
  { "xdg-terminal-exec", " " },
  { "foot", " " },
  { "alacritty", " -e " },
  { "kitty", " " },
  { "xfce4-terminal", " -x " },
  { "gnome-terminal", " -- " },
  { "mate-terminal", " -x " },
  { "tilix", " -e " },
  { "konsole", " -e " },
  { "nxterm", " -e " },
  { "urxvt", " -e " },
  { "rxvt", " -e " },
  { NULL, NULL }
};


void exec_api_set ( void (*new)( const gchar *) )
{
  exec = new;
}

static gchar * exec_clean_macros ( const gchar *cmd )
{
  GString *res;
  const gchar *p;

  res = g_string_sized_new(strlen(cmd));
  for(p=cmd; *p; p++)
  {
    if(*p!='%')
      g_string_append_c(res, *p);
    else
    {
      p++;
      if(!*p || *p=='%')
        g_string_append_c(res, '%');
      if(!*p)
        break;
    }
  }

  return g_string_free(res, FALSE);
}

void exec_cmd ( const gchar *cmd )
{
  gint argc;
  gchar **argv, *clean_cmd;

  if(!cmd)
    return;
  clean_cmd = exec_clean_macros(cmd); 

  if(exec)
    exec(clean_cmd);
  else if(g_shell_parse_argv(clean_cmd, &argc, &argv, NULL))
  {
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH |
         G_SPAWN_STDOUT_TO_DEV_NULL |
         G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
    g_strfreev(argv);
  }
  g_free(clean_cmd);
}

void exec_cmd_in_term ( const gchar *cmd )
{
  gchar *term, *exec;
  gint i;

  for(i=0; terms[i].exec; i++)
    if( (term = g_find_program_in_path(terms[i].exec)) )
      break;

  if(!terms[i].exec)
    return;

  exec = g_strconcat(term, terms[i].arg, cmd, NULL);
  g_free(term);
  exec_cmd(exec);
  g_free(exec);
}

void exec_launch ( GDesktopAppInfo *info )
{
  const gchar *exec;

  g_return_if_fail(info);

  if( !(exec = g_app_info_get_commandline((GAppInfo *)info)) )
    return;

  if(g_desktop_app_info_get_boolean(info, "Terminal"))
    exec_cmd_in_term(exec);
  else
    exec_cmd(exec);
}
