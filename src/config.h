#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <gtk/gtk.h>
#include "action.h"

#define config_lookup_key(x,y) GPOINTER_TO_INT(config_lookup_ptr(x,y))
#define config_lookup_next_key(x,y) GPOINTER_TO_INT(config_lookup_next_ptr(x,y))

enum ConfigSequenceType {
  SEQ_OPT,
  SEQ_CON,
  SEQ_REQ,
  SEQ_END
};

extern GHashTable *config_mods, *config_events, *config_var_types;
extern GHashTable *config_act_cond, *config_toplevel_keys, *config_menu_keys;
extern GHashTable *config_scanner_keys, *config_scanner_types;
extern GHashTable *config_scanner_flags, *config_filter_keys;
extern GHashTable *config_axis_keys, *config_taskbar_types;
extern GHashTable *config_widget_keys, *config_prop_keys, *config_placer_keys;
extern GHashTable *config_flowgrid_props;

typedef gboolean (*parse_func) ( GScanner *, void * );

void config_init ( void );
gpointer config_lookup_ptr ( GScanner *scanner, GHashTable *table );
gpointer config_lookup_next_ptr ( GScanner *scanner, GHashTable *table );
gboolean config_check_and_consume ( GScanner *scanner, gint token );
gchar *config_value_string ( gchar *dest, gchar *string );
GtkWidget *config_parse ( gchar *, GtkWidget * );
void config_pipe_read ( gchar *command );
void config_string ( gchar *string );
gboolean config_expect_token ( GScanner *scan, gint token, gchar *fmt, ...);
gboolean config_is_section_end ( GScanner *scanner );
void config_optional_semicolon ( GScanner *scanner );
void config_parse_sequence ( GScanner *scanner, ... );
gboolean config_assign_boolean (GScanner *scanner, gboolean def, gchar *expr);
gchar *config_assign_string ( GScanner *scanner, gchar *expr );
gdouble config_assign_number ( GScanner *scanner, gchar *expr );
void *config_assign_tokens ( GScanner *scanner, GHashTable *keys, gchar *err );
gboolean config_action ( GScanner *scanner, action_t **action_dst );
void config_action_finish ( GScanner *scanner );
void config_define ( GScanner *scanner );
gchar *config_get_value ( GScanner *, gchar *, gboolean, gchar **);
void config_menu_items ( GScanner *scanner, GtkWidget *menu );
gboolean config_flowgrid_property ( GScanner *scanner, GtkWidget *widget );
void config_scanner ( GScanner *scanner );
void config_layout ( GScanner *, GtkWidget * );
gboolean config_widget_child ( GScanner *scanner, GtkWidget *container );
gboolean config_include ( GScanner *scanner, GtkWidget *container );
void config_switcher ( GScanner *scanner );
void config_placer ( GScanner *scanner );
void config_popup ( GScanner *scanner );
GtkWidget *config_parse_toplevel ( GScanner *scanner, GtkWidget *container );

enum {
  G_TOKEN_SCANNER = G_TOKEN_LAST + 50,
  G_TOKEN_LAYOUT,
  G_TOKEN_POPUP,
  G_TOKEN_PLACER,
  G_TOKEN_SWITCHER,
  G_TOKEN_DEFINE,
  G_TOKEN_TRIGGERACTION,
  G_TOKEN_MAPAPPID,
  G_TOKEN_FILTERAPPID,
  G_TOKEN_FILTERTITLE,
  G_TOKEN_MODULE,
  G_TOKEN_THEME,
  G_TOKEN_ICON_THEME,
  G_TOKEN_DISOWNMINIMIZED,
  G_TOKEN_END,
  G_TOKEN_FILE,
  G_TOKEN_EXEC,
  G_TOKEN_MPDCLIENT,
  G_TOKEN_SWAYCLIENT,
  G_TOKEN_EXECCLIENT,
  G_TOKEN_SOCKETCLIENT,
  G_TOKEN_NUMBERW,
  G_TOKEN_STRINGW,
  G_TOKEN_NOGLOB,
  G_TOKEN_CHTIME,
  G_TOKEN_GRID,
  G_TOKEN_SCALE,
  G_TOKEN_LABEL,
  G_TOKEN_BUTTON,
  G_TOKEN_IMAGE,
  G_TOKEN_CHART,
  G_TOKEN_INCLUDE,
  G_TOKEN_TASKBAR,
  G_TOKEN_PAGER,
  G_TOKEN_TRAY,
  G_TOKEN_STYLE,
  G_TOKEN_CSS,
  G_TOKEN_INTERVAL,
  G_TOKEN_VALUE,
  G_TOKEN_PINS,
  G_TOKEN_PREVIEW,
  G_TOKEN_COLS,
  G_TOKEN_ROWS,
  G_TOKEN_ACTION,
  G_TOKEN_DISPLAY,
  G_TOKEN_ICONS,
  G_TOKEN_LABELS,
  G_TOKEN_LOC,
  G_TOKEN_NUMERIC,
  G_TOKEN_PEROUTPUT,
  G_TOKEN_TITLEWIDTH,
  G_TOKEN_TOOLTIP,
  G_TOKEN_TRIGGER,
  G_TOKEN_GROUP,
  G_TOKEN_XSTEP,
  G_TOKEN_YSTEP,
  G_TOKEN_XORIGIN,
  G_TOKEN_YORIGIN,
  G_TOKEN_CHILDREN,
  G_TOKEN_SORT,
  G_TOKEN_FILTER,
  G_TOKEN_PRIMARY,
  G_TOKEN_TRUE,
  G_TOKEN_FALSE,
  G_TOKEN_MENU,
  G_TOKEN_AUTOCLOSE,
  G_TOKEN_MENUCLEAR,
  G_TOKEN_FUNCTION,
  G_TOKEN_CLIENTSEND,
  G_TOKEN_ITEM,
  G_TOKEN_SEPARATOR,
  G_TOKEN_SUBMENU,
  G_TOKEN_REGEX,
  G_TOKEN_JSON,
  G_TOKEN_SET,
  G_TOKEN_GRAB,
  G_TOKEN_WORKSPACE,
  G_TOKEN_OUTPUT,
  G_TOKEN_FLOATING,
  G_TOKEN_LOCAL,
};

#endif
