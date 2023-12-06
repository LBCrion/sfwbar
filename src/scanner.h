#ifndef __SCANNER_H__
#define __SCANNER_H__

#include <json.h>
#include "expr.h"

enum {
  SO_FILE = 0,
  SO_EXEC = 1,
  SO_CLIENT = 2
};

enum {
  VF_CHTIME = 1,
  VF_NOGLOB = 2
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
  ExprCache *expr;
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
  gboolean inuse;
  ScanFile *file;
} ScanVar;

void scanner_invalidate ( void );
void scanner_var_reset ( ScanVar *var, gpointer dummy );
void scanner_update_json ( struct json_object *, ScanFile * );
GIOStatus scanner_file_update ( GIOChannel *, ScanFile *, gsize * );
int scanner_glob_file ( ScanFile * );
void *scanner_get_value ( gchar *ident, gboolean update, ExprCache *expr );
void scanner_var_new ( gchar *, ScanFile *, gchar *, guint, gint );
gchar *scanner_parse_identifier ( gchar *id, gchar **fname );
ScanFile *scanner_file_get ( gchar *trigger );
ScanFile *scanner_file_new ( gint , gchar *, gchar *, gint );
gboolean scanner_is_variable ( gchar *identifier );
void scanner_file_attach ( const gchar *trigger, ScanFile *file );

#endif
