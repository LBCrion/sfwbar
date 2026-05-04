#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include <gtk/gtk.h>
#include "ext-background-effect-v1.h"

typedef enum {
  BACKGROUND_EFFECT_NONE = 1,
  BACKGROUND_EFFECT_BLUR = 2
} background_effect_t;

typedef struct _background_effect_priv {
  gboolean invalid;
  background_effect_t type;
  struct ext_background_effect_surface_v1 *effect;
} background_effect_priv_t;

extern GType background_effect_enum;

void background_effect_init ( void );
void background_effect_set ( GtkWidget *widget, background_effect_t type );
background_effect_t background_effect_get ( GtkWidget *widget );

#endif
