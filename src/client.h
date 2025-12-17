#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <gio/gio.h>
#include "scanner.h"

typedef struct _Client Client;
struct _Client {
  source_t *src;
  GSocketConnection *scon;
  GSocketConnectable *addr;
  GSocketClient *sclient;
  GIOChannel *in,*out;
  void *data;
  const gchar *trigger;
  gboolean (*connect) ( Client * );
  GIOStatus (*respond) ( Client * );
  GIOStatus (*consume) ( Client *, gsize *size );
};

source_t *client_exec ( gchar  *fname, gchar *trigger );
source_t *client_socket ( gchar *fname, gchar *trigger );
source_t *client_mpd ( gchar *fname );
void client_attach ( Client *client );
gboolean client_socket_connect ( Client *client );
GIOStatus client_source_update ( Client *client, gsize *size );
void client_send ( gchar *addr, gchar *command );

#endif
