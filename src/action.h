#ifndef __ACTION_H__
#define __ACTION_H__

typedef struct user_action {
  guchar cond;
  guchar ncond;
  guint type;
  gchar *command;
  gchar *addr;
} action_t;

void action_exec ( GtkWidget *, action_t *, GdkEvent *, window_t *, guint16 *);
void action_free ( action_t *, GObject *);
void action_function_add ( gchar *, GList *);
void action_function_exec ( gchar *, GtkWidget *, GdkEvent *, window_t *,
    guint16 *);
void action_trigger_add ( action_t *action, gchar *trigger );
action_t *action_trigger_lookup ( gchar *trigger );

#endif
