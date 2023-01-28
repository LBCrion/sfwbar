/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include "../src/module.h"
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

ModuleApi *sfwbar_module_api;
gboolean invalid;

typedef struct _pulse_info {
  guint32 idx;
  gchar *name;
  gboolean mute;
  pa_cvolume cvol;
  gdouble vol;
  gchar *icon, *form;
} pulse_info;

GList *sink_list, *source_list;

pa_mainloop_api *papi;
pa_context *pctx;
gchar *sink_name, *source_name;
gboolean fixed_sink, fixed_source;

static pulse_info *pulse_info_from_name ( GList **l, const gchar *name,
    gboolean new )
{
  GList *iter;
  pulse_info *info;

  if(name)
    for(iter=*l;iter;iter=g_list_next(iter))
      if(!g_strcmp0(((pulse_info *)iter->data)->name,name))
        return iter->data;

  if(new)
  {
    info = g_malloc0(sizeof(pulse_info));
    *l = g_list_prepend(*l,info);
    return info;
  }
  else
    return NULL;
}

static void pulse_set_sink ( const gchar *sink, gboolean fixed )
{
  if(!fixed && fixed_sink)
    return;
  fixed_sink = fixed;
  g_free(sink_name);
  sink_name = g_strdup(sink);
}

static void pulse_set_source ( const gchar *source, gboolean fixed )
{
  if(!fixed && fixed_source)
    return;
  fixed_source = fixed;
  g_free(source_name);
  source_name = g_strdup(source);
}

static void pulse_sink_cb ( pa_context *ctx, const pa_sink_info *pinfo,
    gint eol, void * )
{
  pulse_info *info;

  if(!pinfo)
    return;
  info = pulse_info_from_name(&sink_list,pinfo->name,TRUE);

  g_free(info->name);
  info->name = g_strdup(pinfo->name);
  g_free(info->icon);
  info->icon = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_ICON_NAME));
  g_free(info->form);
  info->form = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_FORM_FACTOR));
  info->idx = pinfo->index;
  info->cvol = pinfo->volume;
  info->vol = 100.0 * pa_cvolume_avg(&info->cvol)/PA_VOLUME_NORM;
  info->mute = pinfo->mute;
  MODULE_TRIGGER_EMIT("pulse");
}

static void pulse_source_cb ( pa_context *ctx, const pa_source_info *pinfo,
    gint eol, void * )
{
  pulse_info *info;

  if(!pinfo)
    return;
  info = pulse_info_from_name(&source_list,pinfo->name,TRUE);

  g_free(info->name);
  info->name = g_strdup(pinfo->name);
  g_free(info->icon);
  info->icon = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_ICON_NAME));
  g_free(info->form);
  info->form = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_FORM_FACTOR));
  info->idx = pinfo->index;
  info->cvol = pinfo->volume;
  info->vol = 100.0 * pa_cvolume_avg(&info->cvol)/PA_VOLUME_NORM;
  info->mute = pinfo->mute;
  MODULE_TRIGGER_EMIT("pulse");
}

static void pulse_server_cb ( pa_context *ctx, const pa_server_info *info,
    void *d )
{
  pulse_set_sink(info->default_sink_name,FALSE);
  pulse_set_source(info->default_source_name,FALSE);
  pa_context_get_sink_info_list(ctx,pulse_sink_cb,NULL);
  pa_context_get_source_info_list(ctx,pulse_source_cb,NULL);
}

static void pulse_subscribe_cb ( pa_context *ctx,
    pa_subscription_event_type_t type, guint idx, void *d )
{
  if(!(type & PA_SUBSCRIPTION_EVENT_CHANGE))
    return;

  switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
  {
    case PA_SUBSCRIPTION_EVENT_SERVER:
      pa_context_get_server_info(ctx,pulse_server_cb,NULL);
      break;
    case PA_SUBSCRIPTION_EVENT_SINK:
      pa_context_get_sink_info_by_index(ctx,idx,pulse_sink_cb,NULL);
      break;
    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
      pa_context_get_source_info_by_index(ctx,idx,pulse_source_cb,NULL);
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
      break;
  }
}

static void pulse_state_cb ( pa_context *ctx, gpointer data )
{
  pa_context_state_t state;
  state = pa_context_get_state(ctx);

  if(state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
    papi->quit(papi,0);
  else if(state == PA_CONTEXT_READY)
  {
    pa_context_get_server_info(ctx,pulse_server_cb,NULL);
    pa_context_set_subscribe_callback(ctx,pulse_subscribe_cb,NULL);
    pa_context_subscribe(ctx,PA_SUBSCRIPTION_MASK_SERVER |
        PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT |
        PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
        NULL, NULL);
  }
}

void sfwbar_module_init ( ModuleApi *api )
{
  pa_glib_mainloop *ploop;

  sfwbar_module_api = api;
  ploop = pa_glib_mainloop_new(g_main_context_get_thread_default());
  papi = pa_glib_mainloop_get_api(ploop);
  pctx = pa_context_new(papi,"sfwbar");
  pa_context_connect(pctx, NULL, PA_CONTEXT_NOFAIL, NULL);
  pa_context_set_state_callback(pctx,pulse_state_cb,NULL);
}

void sfwbar_module_invalidate ( void )
{
  invalid = TRUE;
}

void *pulse_expr_func ( void **params )
{
  gchar *cmd;
  pulse_info *info;

  if(!params || !params[0])
    return g_strdup("");

  if(!g_ascii_strncasecmp(params[0],"sink-",5))
  {
    info = pulse_info_from_name(&sink_list,params[1]?params[1]:sink_name,
        FALSE);
    cmd = params[0]+5;
  }
  else if(!g_ascii_strncasecmp(params[0],"source-",7))
  {
    info = pulse_info_from_name(&source_list,params[1]?params[1]:source_name,
        FALSE);
    cmd = params[0]+7;
  }
  else
    info = NULL;

  if(!info || !cmd || !*cmd)
    return g_strdup("");

  if(!g_ascii_strcasecmp(cmd,"volume"))
    return g_strdup_printf("%f",info->vol);
  else if(!g_ascii_strcasecmp(cmd,"mute"))
    return g_strdup_printf("%d",info->mute);
  else if(!g_ascii_strcasecmp(cmd,"icon"))
    return g_strdup(info->icon?info->icon:"");
  else if(!g_ascii_strcasecmp(cmd,"form"))
    return g_strdup(info->icon?info->form:"");

  return g_strdup_printf("invalid query: %s",cmd);
}

static pa_cvolume *pulse_adjust_volume ( pa_cvolume *vol, gint vold )
{
  if(vold>0)
    pa_cvolume_inc_clamp(vol,vold*PA_VOLUME_NORM/100,PA_VOLUME_UI_MAX);
  else
    pa_cvolume_dec(vol,(-vold*PA_VOLUME_NORM/100));

  return vol;
}

static gboolean pulse_mute_parse ( gchar *cmd, gboolean mute )
{
  while(*cmd==' ')
    cmd++;

  if(!g_ascii_strcasecmp(cmd,"toggle"))
    return !mute;
  else if(!g_ascii_strcasecmp(cmd,"true"))
    return TRUE;
  else if(!g_ascii_strcasecmp(cmd,"false"))
    return FALSE;

  return mute;
}

static void pulse_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  pulse_info *info;

  if(!g_ascii_strncasecmp(cmd,"sink-",5))
    info = pulse_info_from_name(&sink_list,name?name:sink_name,FALSE);
  else if(!g_ascii_strncasecmp(cmd,"source-",7))
    info = pulse_info_from_name(&source_list,name?name:source_name,FALSE);
  else
    info = NULL;

  if(!info)
    return;

  if(!g_ascii_strncasecmp(cmd,"sink-volume",11))
    pa_context_set_sink_volume_by_index(pctx,info->idx,
        pulse_adjust_volume(&info->cvol,g_ascii_strtod(cmd+11,NULL)),NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"source-volume",13))
    pa_context_set_source_volume_by_index(pctx,info->idx,
        pulse_adjust_volume(&info->cvol,g_ascii_strtod(cmd+13,NULL)),
        NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"sink-mute",9))
    pa_context_set_sink_mute_by_index(pctx,info->idx,
        pulse_mute_parse(cmd+9,info->mute),NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"source-mute",11))
    pa_context_set_sink_mute_by_index(pctx,info->idx,
        pulse_mute_parse(cmd+11,info->mute),NULL,NULL);
}

static void pulse_set_sink_action ( gchar *sink, gchar *dummy, void *d1,
    void *d2, void *d3, void *d4 )
{
  pulse_set_sink(sink,TRUE);
}

static void pulse_set_source_action ( gchar *source, gchar *dummy, void *d1,
    void *d2, void *d3, void *d4 )
{
  pulse_set_source(source,TRUE);
}

gint64 sfwbar_module_signature = 0x73f4d956a1;

ModuleExpressionHandler handler1 = {
  .numeric = FALSE,
  .name = "Pulse",
  .parameters = "Ss",
  .function = pulse_expr_func
};

ModuleExpressionHandler *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};

ModuleActionHandler act_handler1 = {
  .name = "PulseCmd",
  .function = pulse_action
};

ModuleActionHandler act_handler2 = {
  .name = "PulseSetDefaultSink",
  .function = pulse_set_sink_action
};

ModuleActionHandler act_handler3 = {
  .name = "PulseSetDefaultSource",
  .function = pulse_set_source_action
};

ModuleActionHandler *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  &act_handler3,
  NULL
};
