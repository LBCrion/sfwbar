#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include <gtk/gtk.h>
#include "ext-background-effect-v1.h"

typedef enum {
  BACKGROUND_EFFECT_NONE = 1,
  BACKGROUND_EFFECT_BLUR = 2
} background_effect_t;

extern GType background_effect_enum;

void background_effect_init ( void );
void background_effect_set ( GtkWidget *widget, background_effect_t type );
background_effect_t background_effect_get ( GtkWidget *widget );

#endif
