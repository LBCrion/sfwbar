#ifndef __BASE_WIDGET_H__
#define __BASE_WIDGET_H__

#include "sfwbar.h" 
#include "action.h"
#include "expr.h"

#define BASE_WIDGET_TYPE            (base_widget_get_type())
G_DECLARE_DERIVABLE_TYPE (BaseWidget, base_widget, BASE, WIDGET, GtkEventBox);
#define IS_BASE_WIDGET BASE_IS_WIDGET

typedef struct _BaseWidgetClass BaseWidgetClass;

struct _BaseWidgetClass
{
  GtkEventBoxClass parent_class;

  void (*update_value)(GtkWidget *self);
  void (*old_size_allocate)(GtkWidget *, GtkAllocation * );
  GtkWidget *(*get_child)(GtkWidget *self);
  GtkWidget *(*mirror)(GtkWidget *self);
};

typedef struct _BaseWidgetPrivate BaseWidgetPrivate;

struct _BaseWidgetPrivate
{
  gchar *id;
  GList *css;
  ExprCache *style;
  ExprCache *value;
  ExprCache *tooltip;
  gulong tooltip_h;
  GList *actions;
  gulong button_h;
  gulong buttonp_h;
  gulong click_h;
  gulong scroll_h;
  gint64 interval;
  guint maxw, maxh;
  gchar *trigger;
  gint64 next_poll;
  gint dir;
  gboolean always_update;
  guint16 user_state;
  GdkRectangle rect;
  GList *mirror_children;
  GtkWidget *mirror_parent;
  GdkModifierType saved_modifiers;
};

typedef struct _base_widget_attachment {
  action_t *action;
  gint event;
  GdkModifierType mods;
} base_widget_attachment_t;

GType base_widget_get_type ( void );

void base_widget_set_tooltip ( GtkWidget *self, gchar *tooltip );
void base_widget_set_value ( GtkWidget *self, gchar *value );
void base_widget_set_style ( GtkWidget *self, gchar *style );
void base_widget_set_trigger ( GtkWidget *self, gchar *trigger );
void base_widget_set_id ( GtkWidget *self, gchar *id );
void base_widget_set_interval ( GtkWidget *self, gint64 interval );
void base_widget_set_state ( GtkWidget *self, guint16 mask, gboolean state );
void base_widget_set_action ( GtkWidget *, gint, GdkModifierType, action_t *);
void base_widget_set_max_width ( GtkWidget *self, guint x );
void base_widget_set_max_height ( GtkWidget *self, guint x );
gboolean base_widget_update_value ( GtkWidget *self );
void base_widget_set_rect ( GtkWidget *self, GdkRectangle rect );
void base_widget_attach ( GtkWidget *, GtkWidget *, GtkWidget *);
guint16 base_widget_get_state ( GtkWidget *self );
gint64 base_widget_get_next_poll ( GtkWidget *self );
void base_widget_set_next_poll ( GtkWidget *self, gint64 ctime );
gchar *base_widget_get_id ( GtkWidget *self );
GtkWidget *base_widget_get_child ( GtkWidget *self );
GtkWidget *base_widget_from_id ( gchar *id );
gchar *base_widget_get_value ( GtkWidget *self );
action_t *base_widget_get_action ( GtkWidget *self, gint, GdkModifierType );
gpointer base_widget_scanner_thread ( GMainContext *gmc );
void base_widget_set_css ( GtkWidget *widget, gchar *css );
gboolean base_widget_emit_trigger ( gchar *trigger );
void base_widget_autoexec ( GtkWidget *self, gpointer data );
void base_widget_set_always_update ( GtkWidget *self, gboolean update );
void base_widget_copy_actions ( GtkWidget *dest, GtkWidget *src );
void base_widget_copy_properties ( GtkWidget *dest, GtkWidget *src );
GtkWidget *base_widget_mirror ( GtkWidget *src );
GdkModifierType base_widget_get_modifiers ( GtkWidget *self );

#endif
