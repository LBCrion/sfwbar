/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "sfwbar.h"

static enum ipc_type ipc;


void ipc_set ( enum ipc_type new )
{
  ipc = new;
}

enum ipc_type ipc_get ( void )
{
  return ipc;
}

