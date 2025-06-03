#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <gtk/gtk.h>
#include "vm/vm.h"

typedef struct {
  GHashTable *locals;
  vm_store_t *store;
} scanner_data_t;

#define SCANNER_DATA(x) ((scanner_data_t *)((x)->user_data))
#define SCANNER_STORE(x) (SCANNER_DATA(x)->store)

#define config_lookup_key(x,y) GPOINTER_TO_INT(config_lookup_ptr(x,y))
#define config_lookup_next_key(x,y) GPOINTER_TO_INT(config_lookup_next_ptr(x,y))
#define config_check_identifier(x,y) \
  (g_scanner_peek_next_token(x)==G_TOKEN_IDENTIFIER && \
   !g_ascii_strcasecmp(x->next_value.v_identifier,y))
#define config_add_key(table, str, key) \
  g_hash_table_insert(table, str, GINT_TO_POINTER(key))

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
extern GHashTable *config_flowgrid_props, *config_menu_item_keys;

typedef gboolean (*parse_func) ( GScanner *, void * );

void config_init ( void );
gpointer config_lookup_ptr ( GScanner *scanner, GHashTable *table );
gpointer config_lookup_next_ptr ( GScanner *scanner, GHashTable *table );
gboolean config_check_and_consume ( GScanner *scanner, gint token );
gchar *config_value_string ( gchar *dest, gchar *string );
GtkWidget *config_parse ( gchar *, GtkWidget *, vm_store_t * );
GtkWidget *config_parse_data ( gchar *, gchar *, GtkWidget *, vm_store_t *);
gboolean config_expect_token ( GScanner *scan, gint token, gchar *fmt, ...);
gboolean config_is_section_end ( GScanner *scanner );
void config_parse_sequence ( GScanner *scanner, ... );
gboolean config_assign_boolean (GScanner *scanner, gboolean def, const gchar *expr);
gchar *config_assign_enum ( GScanner *scanner, const gchar *expr );
gchar *config_assign_string ( GScanner *scanner, const gchar *expr );
gdouble config_assign_number ( GScanner *scanner, const gchar *expr );
void *config_assign_tokens ( GScanner *scanner, GHashTable *keys, gchar *err );
GPtrArray *config_assign_string_list ( GScanner *scanner );
GBytes *config_assign_action ( GScanner *scanner, gchar *err );
GBytes *config_assign_expr ( GScanner *scanner, const gchar *err );
gboolean config_action ( GScanner *scanner, GBytes **action_dst );
gboolean config_expr ( GScanner *scanner, GBytes **expr_dst );
void config_action_finish ( GScanner *scanner );
void config_define ( GScanner *scanner );
gchar *config_get_value ( GScanner *, gchar *, gboolean, gchar **);
void config_menu_items ( GScanner *scanner, GtkWidget *menu );
gboolean config_flowgrid_property ( GScanner *scanner, GtkWidget *widget );
gboolean config_widget_set_property ( GScanner *scanner, gchar *prefix,
    GtkWidget *widget );
void config_scanner ( GScanner *scanner );
gboolean config_scanner_source ( GScanner *scanner );
GtkWidget *config_layout ( GScanner *, GtkWidget * );
gboolean config_widget_child ( GScanner *scanner, GtkWidget *container );
gboolean config_include ( GScanner *scanner, GtkWidget *container );
void config_popup ( GScanner *scanner );
GtkWidget *config_parse_toplevel ( GScanner *scanner, GtkWidget *container );
void config_menu_clear ( GScanner *scanner );
void config_menu ( GScanner *scanner );
void config_skip_statement ( GScanner *scanner );

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
  G_TOKEN_TOOLTIPS,
  G_TOKEN_LOC,
  G_TOKEN_NUMERIC,
  G_TOKEN_PEROUTPUT,
  G_TOKEN_TITLEWIDTH,
  G_TOKEN_TOOLTIP,
  G_TOKEN_TRIGGER,
  G_TOKEN_DISABLE,
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
  G_TOKEN_VAR,
  G_TOKEN_PRIVATE,
  G_TOKEN_GRAB,
  G_TOKEN_WORKSPACE,
  G_TOKEN_OUTPUT,
  G_TOKEN_FLOATING,
  G_TOKEN_LOCAL,
  G_TOKEN_SIZE,
  G_TOKEN_LAYER,
  G_TOKEN_EXCLUSIVEZONE,
  G_TOKEN_SENSOR,
  G_TOKEN_SENSORDELAY,
  G_TOKEN_TRANSITION,
  G_TOKEN_BAR_ID,
  G_TOKEN_MONITOR,
  G_TOKEN_MARGIN,
  G_TOKEN_MIRROR,
  G_TOKEN_INDEX,
  G_TOKEN_DESKTOPID,
};

#endif
