#ifndef __VALUE_H__
#define __VALUE_H__

#include <glib.h>

enum expr_type_t {
  EXPR_TYPE_NUMERIC,
  EXPR_TYPE_STRING,
  EXPR_TYPE_ARRAY,
  EXPR_TYPE_BOOLEAN, // Not in use
  EXPR_TYPE_NA
};

typedef struct {
  guint8 type;
  union {
    gboolean boolean;
    gdouble numeric;
    gchar *string;
    GArray *array;
  } value;
} value_t;

extern const value_t value_na;

#define value_new_string(v) \
  ((value_t){.type=EXPR_TYPE_STRING, .value.string=(v)})
#define value_new_numeric(v) \
  ((value_t){.type=EXPR_TYPE_NUMERIC, .value.numeric=(v)})
#define value_new_array(v) \
  ((value_t){.type=EXPR_TYPE_ARRAY, .value.array=(v)})
#define value_new_na() (value_na)

#define value_is_string(v) ((v).type == EXPR_TYPE_STRING)
#define value_is_numeric(v) ((v).type == EXPR_TYPE_NUMERIC)
#define value_is_na(v) ((v).type == EXPR_TYPE_NA)
#define value_is_array(v) ((v).type == EXPR_TYPE_ARRAY)

#define value_like_string(v) (value_is_string(v) || value_is_na(v))
#define value_like_numeric(v) (value_is_numeric(v) || value_is_na(v))
#define value_like_array(v) (value_is_array(v) || value_is_na(v))

#define value_get_string(v) \
  ((value_is_string(v)&&v.value.string)?v.value.string:"")
#define value_get_numeric(v) (value_is_numeric(v)?v.value.numeric:0)
#define value_get_array(v) (value_is_array(v)?v.value.array:NULL)

#define value_as_numeric(v) (value_like_numeric(v)? value_get_numeric(v) : \
    (value_is_string(v)? g_ascii_strtod(v.value.string, NULL) : 0))

void value_free ( value_t );
void value_free_ptr ( value_t *v1 );
value_t value_dup ( value_t );
value_t value_array_concat ( value_t v1, value_t v2 );
gchar *value_array_to_string ( value_t value );

#endif
