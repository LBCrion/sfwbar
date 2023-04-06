#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <gio/gio.h>
#include "scanner.h"

typedef struct _Client Client;
struct _Client {
  ScanFile *file;
  GSocketConnection *scon;
  GIOChannel *in,*out,*err;
  void (*connect) ( Client * );
};

void client_exec ( ScanFile *file );
void client_socket ( ScanFile *file );

#endif
