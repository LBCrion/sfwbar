#ifndef __BAR_H__
#define __BAR_H__

#include <gtk/gtk.h>

#define BAR_TYPE            (bar_get_type())
#define BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BAR_TYPE, Bar))
#define BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), BAR_TYPE, BarClass))
#define IS_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BAR_TYPE))
#define IS_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BAR_TYPE))
#define BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), BAR_TYPE, BarClass))

GType bar_get_type ( void );

typedef struct _Bar Bar;
typedef struct _BarClass BarClass;

struct _Bar
{
  GtkWindow parent_class;
};

struct _BarClass
{
  GtkWindowClass parent_class;
  void (*old_hide)( GtkWidget * );
};

typedef struct _BarPrivate BarPrivate;

struct _BarPrivate {
  gchar *name;
  gchar *size;
  gint64 margin;
  gchar *ezone;
  gchar *layer;
  gchar *bar_id;
  gint dir;
  GtkAlign halign, valign;
  GtkWidget *start, *center, *end;
  GtkWidget *box, *sensor;
  gint64 sensor_timeout;
  gboolean sensor_state, sensor_block;
  guint sensor_handle;
  GList *sensor_refs;
  gboolean hidden;
  gboolean jump;
  gboolean visible;
  gboolean full_size;
  gchar *output;
  GdkMonitor *current_monitor;
  GList *mirror_targets;
  GList *mirror_children;
  GtkWidget *mirror_parent;
};

GtkWidget *bar_new ( gchar * );
gboolean bar_address_all ( GtkWidget *self, gchar *value,
    void (*bar_func)( GtkWidget *, gchar * ) );
GHashTable *bar_get_list ( void );
void bar_set_monitor ( GtkWidget *, gchar * );
void bar_set_layer ( GtkWidget *, gchar * );
void bar_set_size ( GtkWidget *, gchar * );
void bar_set_margin ( GtkWidget *, gint64 );
void bar_set_exclusive_zone ( GtkWidget *, gchar * );
gchar *bar_get_output ( GtkWidget * );
gint bar_get_toplevel_dir ( GtkWidget * );
void bar_set_id ( GtkWidget *, gchar * );
void bar_set_sensor ( GtkWidget *self, gint64 timeout );
void bar_set_mirrors ( GtkWidget *self, GList *mirrors );
void bar_set_visibility ( GtkWidget *, const gchar *, gchar );
void bar_set_mirrors_old ( GtkWidget *self, gchar *mirror );
gboolean bar_visibility_toggle_all ( gpointer d );
gboolean bar_update_monitor ( GtkWidget * );
void bar_save_monitor ( GtkWidget * );
GtkWidget *bar_from_name ( gchar *name );
GtkWidget *bar_grid_from_name ( gchar *addr );
void bar_set_theme ( gchar *new_theme );
void bar_set_icon_theme ( gchar *new_theme );
GtkWidget *bar_mirror ( GtkWidget *, GdkMonitor * );
void bar_handle_direction ( GtkWidget *self );
void bar_sensor_cancel_hide ( GtkWidget *self );
void bar_set_interactivity ( GtkWidget *self, gboolean interactivity );

#endif
