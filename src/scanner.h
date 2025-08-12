#ifndef __SCANNER_H__
#define __SCANNER_H__

#include <json.h>
#include "vm/expr.h"
#include "vm/vm.h"

enum {
  SO_FILE = 0,
  SO_EXEC = 1,
  SO_CLIENT = 2
};

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
  SCANNER_TYPE_NONE = 0,
  SCANNER_TYPE_STR,
  SCANNER_TYPE_VAL,
  SCANNER_TYPE_PVAL,
  SCANNER_TYPE_COUNT,
  SCANNER_TYPE_TIME,
  SCANNER_TYPE_AGE
};

typedef struct scan_file {
  gchar *fname;
  const gchar *trigger;
  gint flags;
  guchar source;
  time_t mtime;
  GList *vars;
  void *client;
} ScanFile;

typedef struct scan_var {
  GMutex mutex;
  expr_cache_t *expr;
  vm_store_t *store;
  void *definition;
  gchar *str;
  guint vstate;
  double val;
  double pval;
  gint64 time;
  gint64 ptime;
  gint count;
  gint multi;
  guint type;
  gboolean invalid;
  ScanFile *file;
} ScanVar;

void scanner_invalidate ( void );
void scanner_var_reset ( ScanVar *var, gpointer dummy );
void scanner_update_json ( struct json_object *, ScanFile * );
GIOStatus scanner_file_update ( GIOChannel *, ScanFile *, gsize * );
int scanner_glob_file ( ScanFile * );
value_t scanner_get_value ( GQuark id, gchar ftype, gboolean update,
    expr_cache_t *expr );
void scanner_var_new ( gchar *name, ScanFile *file, gchar *pattern,
    guint type, gint flag, vm_store_t *store );
void scanner_file_merge ( ScanFile *keep, ScanFile *temp );
GQuark scanner_parse_identifier ( const gchar *id, guint8 *dtype );
ScanFile *scanner_file_get ( gchar *trigger );
ScanFile *scanner_file_new ( gint , gchar *, gchar *, gint );
gboolean scanner_is_variable ( gchar *identifier );
void scanner_file_attach ( const gchar *trigger, ScanFile *file );

#endif
