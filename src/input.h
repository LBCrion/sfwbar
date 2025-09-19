#ifndef __INPUT_H__
#define __INPUT_H__

#include <glib.h>

struct input_api {
  void (*layout_next) ( void );
  void (*layout_prev) ( void );
  void (*layout_set) ( gchar * );
};

void input_api_register ( struct input_api *new );
gboolean input_api_check ( void );
void input_layout_set ( const gchar *new_layout );
const gchar *input_layout_get ( void );
void input_layout_next ( void );
void input_layout_prev ( void );

#endif
