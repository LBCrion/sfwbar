#include <glib.h>
#include "../src/module.h"
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

ModuleApi *sfwbar_module_api;
gboolean invalid;
pa_mainloop_api *papi;
pa_context *pctx;
gchar *sink_name, *source_name;
gboolean fixed_sink, fixed_source;
gdouble sink_vol, source_vol;
pa_cvolume sink_cvol, source_cvol;
gboolean sink_mute, source_mute;

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

static void pulse_sink_cb ( pa_context *ctx, const pa_sink_info *info,
    gint eol, void * )
{
  if(!info)
    return;
  if(g_strcmp0(info->name, sink_name))
    return;

  sink_cvol = info->volume;
  sink_vol = 100.0 * pa_cvolume_avg(&sink_cvol)/PA_VOLUME_NORM;
  sink_mute = info->mute;
  MODULE_TRIGGER_EMIT("pulse");
}

static void pulse_source_cb ( pa_context *ctx, const pa_source_info *info,
    gint eol, void * )
{
  if(!info)
    return;
  if(g_strcmp0(info->name, source_name))
    return;

  source_cvol = info->volume;
  source_vol = 100.0 * pa_cvolume_avg(&source_cvol)/PA_VOLUME_NORM;
  source_mute = info->mute;
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
  gchar *result;

  if(!params || !params[0])
    return g_strdup("");

  if(!g_ascii_strcasecmp(params[0],"sink-volume"))
    return g_strdup_printf("%f",sink_vol);
  else if(!g_ascii_strcasecmp(params[0],"source-volume"))
    return g_strdup_printf("%f",source_vol);
  else if(!g_ascii_strcasecmp(params[0],"sink-mute"))
    return g_strdup_printf("%d",sink_mute);
  else if(!g_ascii_strcasecmp(params[0],"source-mute"))
    return g_strdup_printf("%d",source_mute);

  result = g_strdup("invalid query");

  return result;
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

static void pulse_action ( gchar *cmd, gchar *dummy, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!g_ascii_strncasecmp(cmd,"sink-volume",11))
    pa_context_set_sink_volume_by_name(pctx,sink_name,
        pulse_adjust_volume(&sink_cvol,g_ascii_strtod(cmd+11,NULL)),NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"source-volume",13))
    pa_context_set_source_volume_by_name(pctx,source_name,
        pulse_adjust_volume(&source_cvol,g_ascii_strtod(cmd+13,NULL)),
        NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"sink-mute",9))
    pa_context_set_sink_mute_by_name(pctx,sink_name,
        pulse_mute_parse(cmd+9,sink_mute),NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd,"source-mute",11))
    pa_context_set_sink_mute_by_name(pctx,source_name,
        pulse_mute_parse(cmd+11,source_mute),NULL,NULL);
}

gint64 sfwbar_module_signature = 0x73f4d956a1;

ModuleExpressionHandler handler1 = {
  .numeric = FALSE,
  .name = "Pulse",
  .parameters = "S",
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

ModuleActionHandler *sfwbar_action_handlers[] = {
  &act_handler1,
  NULL
};
