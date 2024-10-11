/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include <gtk/gtk.h>

GdkMonitor *widget_get_monitor ( GtkWidget *self )
{
  GtkWidget *parent, *w;
  GdkWindow *win;
  GdkDisplay *disp;

  g_return_val_if_fail(GTK_IS_WIDGET(self),NULL);

  if(gtk_widget_get_mapped(self))
    win = gtk_widget_get_window(self);
  else
  {
    for(w=self; w; w=gtk_widget_get_parent(w))
      if( (parent=g_object_get_data(G_OBJECT(w), "parent_window")) )
        break;
    if(!w)
      return NULL;
    win = gtk_widget_get_window(parent);
  }

  if(!win || !(disp = gdk_window_get_display(win)) )
      return NULL;
  return gdk_display_get_monitor_at_window(disp, win);
}
