#ifndef __TOPLEVEL_H__
#define __TOPLEVEL_H__

GtkWidget *toplevel_new ( gchar *name );
void toplevel_hide ( gchar *name );
void toplevel_show ( gchar *name );
void toplevel_toggle ( gchar *name );

#endif
