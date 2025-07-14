#ifndef __POPUP_H__
#define __POPUP_H__

G_BEGIN_DECLS

#define POPUP_TYPE_WINDOW popup_window_get_type ()
G_DECLARE_FINAL_TYPE(PopupWindow, popup_window, POPUP, WINDOW, GtkWindow)

typedef struct _PopupWindow {
  GtkWindow parent;
} PopupWindow;

G_END_DECLS

GtkWidget *popup_new ( gchar *name );
GtkWidget *popup_from_name ( gchar *name );
void popup_trigger ( GtkWidget *parent, gchar *name, GdkEvent *ev );
void popup_show ( GtkWidget *parent, GtkWidget *popup, GdkSeat *seat );
void popup_get_gravity ( GtkWidget *widget, GdkGravity *, GdkGravity * );
void popup_popdown_autoclose ( void );

#endif
