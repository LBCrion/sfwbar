#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <gio/gio.h>
#include "scanner.h"

typedef struct _Client Client;
struct _Client {
  ScanFile *file;
  GSocketConnection *scon;
  GSocketConnectable *addr;
  GSocketClient *sclient;
  GIOChannel *in,*out;
  void *data;
  gchar *trigger;
  gboolean (*connect) ( Client * );
  GIOStatus (*respond) ( Client * );
  GIOStatus (*consume) ( Client *, gsize *size );
};

ScanFile *client_exec ( gchar  *fname, gchar *trigger );
ScanFile *client_socket ( gchar *fname, gchar *trigger );
ScanFile *client_mpd ( gchar *fname );
void client_attach ( Client *client );
gboolean client_socket_connect ( Client *client );
void client_send ( gchar *addr, gchar *command );
void client_mpd_command ( gchar *command );

#endif
