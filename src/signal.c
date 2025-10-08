/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include <glib.h>
#include "trigger.h"

static gint signal_counter[255];
static gint signal_flag;

static void signal_handler ( gint sig )
{
  if(sig<SIGRTMIN || sig>SIGRTMAX)
    return;

  g_atomic_int_inc(&signal_counter[sig-SIGRTMIN]);
  g_atomic_int_set(&signal_flag, 1);
}

static gboolean signal_source_prepare( GSource *source, gint *timeout)
{
  *timeout = -1;
  return signal_flag;
}

static gboolean signal_source_check( GSource *source )
{
  return signal_flag;
}

static gboolean signal_source_dispatch( GSource *source, GSourceFunc cb,
  gpointer data)
{
  gchar *trigger;
  gint sig;

  g_atomic_int_set(&signal_flag, 0);
  for(sig=SIGRTMIN; sig<SIGRTMAX; sig++)
    while(signal_counter[sig-SIGRTMIN])
    {
      g_atomic_int_dec_and_test(&signal_counter[sig-SIGRTMIN]);
      trigger = g_strdup_printf("sigrtmin+%d", sig-SIGRTMIN);
      trigger_emit(trigger);
      g_free(trigger);
    }
  return TRUE;
}

static void signal_source_finalize ( GSource *source )
{
}

static GSourceFuncs signal_source_funcs = {
  signal_source_prepare,
  signal_source_check,
  signal_source_dispatch,
  signal_source_finalize
};

void signal_subscribe ( void )
{
  GSource *source;
  struct sigaction act;
  gint sig;

  act.sa_handler = signal_handler;
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  for(sig=SIGRTMIN; sig<=SIGRTMAX; sig++)
    sigaction(sig, &act, NULL);

  source = g_source_new(&signal_source_funcs, sizeof(GSource));
  g_source_attach(source, NULL);
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
}
