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
  gchar *trigger;
  gint flags;
  guchar source;
  time_t mtime;
  GList *vars;
  GSocketConnection *scon;
  GIOChannel *out;
} ScanFile;

typedef struct scan_var {
  ExprCache *expr;
  void *definition;
  gchar *str;
  guint vcount;
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
void scanner_file_update ( GIOChannel *, ScanFile * );
int scanner_glob_file ( ScanFile * );
char *scanner_get_string ( gchar *, gboolean, guint * );
double scanner_get_numeric ( gchar *, gboolean, guint * );
void scanner_var_new ( gchar *, ScanFile *, gchar *, guint, gint );
ScanFile *scanner_file_get ( gchar *trigger );
ScanFile *scanner_file_new ( gint , gchar *, gchar *, gint );
gboolean scanner_is_variable ( gchar *identifier );

#endif
