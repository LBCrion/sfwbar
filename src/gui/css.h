#ifndef __CSS_H__
#define __CSS_H__

#include <gtk/gtk.h>

void css_init ( gchar * );
void css_file_load ( gchar * );
void css_widget_apply ( GtkWidget *widget, gchar *css );
void css_widget_cascade ( GtkWidget *widget, gpointer data );
void css_add_class ( GtkWidget *widget, gchar *css_class );
void css_remove_class ( GtkWidget *widget, gchar *css_class );
void css_set_class ( GtkWidget *widget, gchar *css_class, gboolean state );
gchar *css_legacy_preprocess ( gchar *css_string );

void widget_set_css ( GtkWidget *, gpointer );
void widget_parse_css ( GtkWidget *widget, gchar *css );

#endif
