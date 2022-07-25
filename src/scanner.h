#ifndef __SCANNER_H__
#define __SCANNER_H__

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
  GRegex *regex;
  gchar *json;
  gchar *str;
  double val;
  double pval;
  gint64 time;
  gint64 ptime;
  gint count;
  gint multi;
  guint type;
  guchar status;
  ScanFile *file;
} ScanVar;

void scanner_expire ( void );
int scanner_reset_vars ( GList * );
void scanner_update_json ( struct json_object *, ScanFile * );
int scanner_update_file ( GIOChannel *, ScanFile * );
int scanner_glob_file ( ScanFile * );
char *scanner_get_string ( gchar *, gboolean );
double scanner_get_numeric ( gchar *, gboolean );
void scanner_var_attach ( gchar *, ScanFile *, gchar *, guint, gint );
ScanFile *scanner_file_get ( gchar *trigger );
ScanFile *scanner_file_new ( gint , gchar *, gchar *, gint );

#endif
