#ifndef __BASE_WIDGET_H__
#define __BASE_WIDGET_H__

#include "vm/expr.h"
#include "vm/vm.h"

#define BASE_WIDGET_MAX_ACTION  8

#define BASE_WIDGET_TYPE            (base_widget_get_type())
#define BASE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), BASE_WIDGET_TYPE, BaseWidgetClass))
#define BASE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BASE_WIDGET_TYPE, BaseWidget))
#define IS_BASE_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BASE_WIDGET_TYPE))
#define IS_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BASE_WIDGET_TYPE))
#define BASE_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), BASE_WIDGET_TYPE, BaseWidgetClass))

typedef struct _BaseWidgetClass BaseWidgetClass;
typedef struct _BaseWidget BaseWidget;

struct _BaseWidget
{
  GtkEventBox widget;
};

struct _BaseWidgetClass
{
  GtkEventBoxClass parent_class;

  void (*update_value)(GtkWidget *self);
  void (*mirror)(GtkWidget *self, GtkWidget *src);
  gboolean (*action_exec)( GtkWidget *self, gint slot, GdkEvent *ev );
  void (*action_configure)( GtkWidget *self, gint slot );

  gboolean always_update;
  gboolean custom_tooltip;
};

typedef struct _BaseWidgetPrivate BaseWidgetPrivate;

struct _BaseWidgetPrivate
{
  gchar *id;
  gchar *css;
  expr_cache_t *style;
  expr_cache_t *value;
  expr_cache_t *tooltip;
  gint64 interval;
  gint64 next_poll;
  guint maxw, maxh;
  const gchar *trigger;
  gint dir;
  gboolean local_state;
  gboolean is_drag_dest;
  gboolean disabled;
  guint user_state;
  GdkRectangle rect;
  GtkCssProvider *provider;
  GPtrArray *actions;
  GList *mirror_children;
  GtkWidget *mirror_parent;
  GMutex mutex;
  vm_store_t *store;
};

typedef struct _base_widget_attachment {
  GBytes *action;
  gint event;
  GdkModifierType mods;
} base_widget_attachment_t;

GType base_widget_get_type ( void );

void base_widget_set_state ( GtkWidget *self, guint16 mask, gboolean state );
void base_widget_set_action ( GtkWidget *, gint, GdkModifierType, GBytes *);
void base_widget_set_max_width ( GtkWidget *self, guint x );
void base_widget_set_max_height ( GtkWidget *self, guint x );
vm_store_t * base_widget_get_store ( GtkWidget *self );
void base_widget_attach ( GtkWidget *, GtkWidget *, GtkWidget *);
GList *base_widget_get_mirror_children ( GtkWidget *self );
GtkWidget *base_widget_get_mirror_parent ( GtkWidget *self );
gchar *base_widget_get_id ( GtkWidget *self );
GtkWidget *base_widget_get_child ( GtkWidget *self );
GtkWidget *base_widget_from_id ( vm_store_t *store, gchar *id );
gchar *base_widget_get_value ( GtkWidget *self );
GBytes *base_widget_get_action ( GtkWidget *self, gint, GdkModifierType );
gpointer base_widget_scanner_thread ( GMainContext *gmc );
guint16 base_widget_state_build ( GtkWidget *self, window_t *win );
void base_widget_autoexec ( GtkWidget *self, gpointer data );
GtkWidget *base_widget_mirror ( GtkWidget *src );
GdkModifierType base_widget_get_modifiers ( GtkWidget *self );
gboolean base_widget_action_exec ( GtkWidget *, gint, GdkEvent *);
gboolean base_widget_check_action_slot ( GtkWidget *self, gint slot );
void base_widget_action_configure ( GtkWidget *self, gint slot );
gboolean base_widget_update_expressions ( GtkWidget *self );
GPtrArray *base_widget_attachment_new_array ( GBytes *code, gint ev,
    GdkModifierType mods );

#endif
