#ifndef __SCANNER_H__
#define __SCANNER_H__

#include <json.h>
#include "vm/expr.h"
#include "vm/vm.h"

enum {
  VF_CHTIME = 1,
  VF_NOGLOB = 2
};

enum {
  VT_SUM = 1,
  VT_PROD,
  VT_LAST,
  VT_FIRST
};

enum {
  SCANNER_TYPE_STR = 1,
  SCANNER_TYPE_VAL,
  SCANNER_TYPE_PVAL,
  SCANNER_TYPE_COUNT,
  SCANNER_TYPE_TIME,
  SCANNER_TYPE_AGE
};

typedef struct _source source_t;
typedef struct _scan_var scan_var_t;
typedef struct _src_handler src_handler_t;

struct _source {
  gboolean invalid;
  gboolean (*update)(source_t *src);
  GRecMutex mutex;
  GList *vars;
  GList *handlers;
  gchar *fname;
  gpointer data;
};

typedef struct _file_source {
  gint flags;
  time_t mtime;
} file_source_t;

#define FILE_SOURCE(x) ((file_source_t *)(x->data))

typedef struct _expr_source {
  expr_cache_t *expr;
  gboolean updating;
} expr_source_t;

#define EXPR_SOURCE(x) ((expr_source_t *)(x->data))

struct _scan_var {
  GQuark id;
  gboolean vstate;
  gchar * (*parse)(scan_var_t *, GString *);
  void *definition;
  gchar *str;
  double val;
  double pval;
  gint64 time;
  gint64 ptime;
  gint count;
  gint multi;
  guint type;
  gboolean invalid;
  source_t *src;
};

#define SCAN_VAR(x) ((scan_var_t *)(x))

struct _src_handler {
  void (*init)(source_t *, src_handler_t *);
  void (*handle)(source_t *, src_handler_t *, GString *);
  void (*finish)(source_t *, src_handler_t *);
  gpointer data;
};

#define SRC_HANDLER(x) ((src_handler_t *)(x))

void scanner_invalidate ( void );
void scanner_var_invalidate ( GQuark key, scan_var_t *var, void *data );
void scanner_var_reset ( scan_var_t *var, gpointer dummy );
void scanner_update_json ( struct json_object *, source_t * );
GIOStatus scanner_source_update ( GIOChannel *, source_t *, gsize * );
value_t scanner_get_value ( GQuark id, gchar ftype, gboolean update,
    gboolean *vstate );
scan_var_t *scanner_var_new_calc ( gchar *name, source_t *src, gpointer code,
   vm_store_t *store );
scan_var_t *scanner_var_new_regex ( gchar *name, source_t *src, void *regex );
scan_var_t *scanner_var_new_json ( gchar *name, source_t *src, void *path );
scan_var_t *scanner_var_new_grab ( gchar *name, source_t *src, void *dummy );
GQuark scanner_parse_identifier ( const gchar *id, guint8 *dtype );
source_t *scanner_source_new ( gchar *fname );
source_t *scanner_file_new ( gchar *fname, gint flags );
source_t *scanner_exec_new ( gchar *fname );
gboolean scanner_is_variable ( gchar *identifier );

#endif
