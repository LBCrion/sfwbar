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

void exec_cmd ( const gchar *cmd )
{
  gint argc;
  gchar **argv;

  if(exec)
    exec(cmd);
  else if(g_shell_parse_argv(cmd, &argc, &argv, NULL))
  {
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH |
         G_SPAWN_STDOUT_TO_DEV_NULL |
         G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
    g_strfreev(argv);
  }
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

  if( !(exec = g_app_info_get_executable((GAppInfo *)info)) )
    return;

  if(g_desktop_app_info_get_boolean(info, "Terminal"))
    exec_cmd_in_term(exec);
  else
    exec_cmd(exec);
}
