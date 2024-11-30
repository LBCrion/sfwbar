#ifndef __VALUE_H__
#define __VALUE_H__

enum expr_type_t {
  EXPR_TYPE_NUMERIC,
  EXPR_TYPE_STRING,
  EXPR_TYPE_BOOLEAN, // Not in use
  EXPR_TYPE_NA
};

typedef struct {
  guint8 type;
  union {
    gboolean boolean;
    gdouble numeric;
    gchar *string;
  } value;
} value_t;

extern const value_t value_na;

#define value_new_string(v) ((value_t){.type=EXPR_TYPE_STRING, .value.string=(v)})
#define value_new_numeric(v) ((value_t){.type=EXPR_TYPE_NUMERIC, .value.numeric=(v)})
#define value_new_na() (value_na)

#define value_is_string(v) ((v).type == EXPR_TYPE_STRING)
#define value_is_numeric(v) ((v).type == EXPR_TYPE_NUMERIC)
#define value_is_na(v) ((v).type == EXPR_TYPE_NA)

#define value_free(v) { if(value_is_string(v)) g_free(v.value.string); }

#endif
