/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include "../src/module.h"
#include "../src/basewidget.h"
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

gboolean invalid;

typedef struct _pulse_info {
  guint32 idx;
  gchar *name;
  gboolean mute;
  pa_cvolume cvol;
  gdouble vol;
  gchar *icon, *form, *port, *monitor, *description;
  pa_channel_map cmap;
} pulse_info;

typedef struct _pulse_channel {
  gint chan_num;
  gchar *channel;
  gchar *device;
} pulse_channel;

pa_mainloop_api *papi;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;

static GList *sink_list, *source_list;
static pa_context *pctx;
static gchar *sink_name, *source_name;
static gboolean fixed_sink, fixed_source;

static void* pulse_channel_dup_dummy ( void *d )
{
  return d;
}

static void pulse_channel_free ( pulse_channel *channel )
{
  g_free(channel->channel);
  g_free(channel->device);
  g_free(channel);
}

static gboolean pulse_channel_compare ( pulse_channel *c1, pulse_channel *c2 )
{
  return (!g_strcmp0(c1->channel, c2->channel) &&
      !g_strcmp0(c1->device, c2->device));
}

static void *pulse_channel_get_str ( pulse_channel *channel, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "channel"))
    return g_strdup(channel->channel);
  if(!g_ascii_strcasecmp(prop, "device"))
    return g_strdup(channel->device);
  if(!g_ascii_strcasecmp(prop, "ChannelNumber"))
    return g_strdup_printf("%d", channel->chan_num);
  return NULL;
}

static module_queue_t channel_q = {
  .free = (void (*)(void *))pulse_channel_free,
  .duplicate = (void *(*)(void *))pulse_channel_dup_dummy,
  .get_str = (void *(*)(void *, gchar *))pulse_channel_get_str,
  .compare = (gboolean (*)(const void *, const void *))pulse_channel_compare,
};

static void *pulse_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "RemovedDevice"))
    return g_strdup(name);
  return NULL;
}

static module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))g_strdup,
  .get_str = (void *(*)(void *, gchar *))pulse_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static pulse_info *pulse_info_from_name ( GList **l, const gchar *name,
    gboolean new )
{
  GList *iter;
  pulse_info *info;

  if(name)
    for(iter=*l; iter; iter=g_list_next(iter))
      if(!g_strcmp0(((pulse_info *)iter->data)->name, name))
        return iter->data;

  if(new)
  {
    info = g_malloc0(sizeof(pulse_info));
    *l = g_list_prepend(*l, info);
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

static void pulse_remove_device ( GList **list, guint32 idx )
{
  GList *iter;
  pulse_info *info;

  for(iter=*list; iter; iter=g_list_next(iter))
    if(((pulse_info *)iter->data)->idx == idx)
      break;
  if(!iter)
    return;

  info = iter->data;
  *list = g_list_delete_link(*list, iter);

  if(info->name)
    module_queue_append(&remove_q, info->name);
  g_free(info->icon);
  g_free(info->form);
  g_free(info->port);
  g_free(info->monitor);
  g_free(info->description);
  g_free(info);
}

static void pulse_sink_cb ( pa_context *ctx, const pa_sink_info *pinfo,
    gint eol, void *d )
{
  pulse_info *info;
  pulse_channel *channel;
  gint i;

  if(!pinfo)
    return;
  if(!pulse_info_from_name(&sink_list, pinfo->name, FALSE))
  {
    for(i=0;i<pinfo->channel_map.channels;i++)
    {
      channel = g_malloc0(sizeof(pulse_channel));
      channel->chan_num = i;
      channel->channel = g_strdup(
            pa_channel_position_to_string(pinfo->channel_map.map[i]));
      channel->device = g_strdup(pinfo->name);
      module_queue_append(&channel_q, channel);
    }
  }
  info = pulse_info_from_name(&sink_list, pinfo->name, TRUE);

  g_free(info->name);
  info->name = g_strdup(pinfo->name);
  g_free(info->icon);
  info->icon = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_ICON_NAME));
  g_free(info->form);
  info->form = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_FORM_FACTOR));
  g_free(info->port);
  info->port = g_strdup(pinfo->active_port?pinfo->active_port->name:NULL);
  g_free(info->monitor);
  info->monitor = g_strdup(pinfo->monitor_source_name);
  g_free(info->description);
  info->description = g_strdup(pinfo->description);
  info->idx = pinfo->index;
  info->cvol = pinfo->volume;
  info->vol = 100.0 * pa_cvolume_avg(&info->cvol)/PA_VOLUME_NORM;
  info->mute = pinfo->mute;
  info->cmap = pinfo->channel_map;
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("pulse"));
}

static void pulse_source_cb ( pa_context *ctx, const pa_source_info *pinfo,
    gint eol, void *d )
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
  g_free(info->port);
  info->port = g_strdup(pinfo->active_port?pinfo->active_port->name:"Unknown");
  g_free(info->monitor);
  info->monitor = g_strdup(pinfo->monitor_of_sink_name);
  g_free(info->description);
  info->description = g_strdup(pinfo->description);
  info->idx = pinfo->index;
  info->cvol = pinfo->volume;
  info->vol = 100.0 * pa_cvolume_avg(&info->cvol)/PA_VOLUME_NORM;
  info->mute = pinfo->mute;
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("pulse"));
}

static void pulse_server_cb ( pa_context *ctx, const pa_server_info *info,
    void *d )
{
  pulse_set_sink(info->default_sink_name,FALSE);
  pulse_set_source(info->default_source_name,FALSE);
  pa_operation_unref(
      pa_context_get_sink_info_list(ctx,pulse_sink_cb,NULL));
  pa_operation_unref(
      pa_context_get_source_info_list(ctx,pulse_source_cb,NULL));
}

static void pulse_subscribe_cb ( pa_context *ctx,
    pa_subscription_event_type_t type, guint idx, void *d )
{
  if((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
  {
    switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
    {
      case PA_SUBSCRIPTION_EVENT_SINK:
        pulse_remove_device(&sink_list, idx);
        break;
      case PA_SUBSCRIPTION_EVENT_SOURCE:
        pulse_remove_device(&source_list, idx);
        break;
    }
  }
  if(!(type & PA_SUBSCRIPTION_EVENT_CHANGE))
    return;

  switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
  {
    case PA_SUBSCRIPTION_EVENT_SERVER:
      pa_operation_unref(
          pa_context_get_server_info(ctx,pulse_server_cb,NULL));
      break;
    case PA_SUBSCRIPTION_EVENT_SINK:
      pa_operation_unref(
        pa_context_get_sink_info_by_index(ctx,idx,pulse_sink_cb,NULL));
      break;
    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
      pa_operation_unref(
        pa_context_get_source_info_by_index(ctx,idx,pulse_source_cb,NULL));
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
    pa_operation_unref(
        pa_context_get_server_info(ctx,pulse_server_cb,NULL));
    pa_context_set_subscribe_callback(ctx,pulse_subscribe_cb,NULL);
    pa_operation_unref(
      pa_context_subscribe(ctx,PA_SUBSCRIPTION_MASK_SERVER |
          PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT |
          PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
          NULL, NULL));
  }
}

gboolean sfwbar_module_init ( void )
{
  pa_glib_mainloop *ploop;

  channel_q.trigger = g_intern_static_string("pulse_channel");
  remove_q.trigger = g_intern_static_string("pulse_removed");
  ploop = pa_glib_mainloop_new(g_main_context_get_thread_default());
  papi = pa_glib_mainloop_get_api(ploop);
  pctx = pa_context_new(papi, "sfwbar");
  pa_context_connect(pctx, NULL, PA_CONTEXT_NOFAIL, NULL);
  pa_context_set_state_callback(pctx, pulse_state_cb, NULL);
  return TRUE;
}

void sfwbar_module_invalidate ( void )
{
  invalid = TRUE;
}

static pulse_info *pulse_addr_parse ( gchar *addr, GList **list, gchar *def,
    gint *cidx )
{
  pa_channel_position_t cpos;
  pulse_info *info;
  gchar *device, *channel;
  gint i;

  *cidx = 0;

  if(addr)
  {
    device = g_strdup(addr);
    if( (channel = strchr(device, ':')) )
      *channel = '\0';
  }
  else
  {
    device = NULL;
    channel = NULL;
  }

  info = pulse_info_from_name(list, device?device:def, FALSE);

  if(info && channel)
  {
    cpos = pa_channel_position_from_string(channel+1);
    for(i=0; i<info->cmap.channels; i++)
      if(info->cmap.map[i] == cpos)
        *cidx = i+1;
  }
  g_free(device);

  return info;
}

static void *pulse_expr_func ( void **params, void *widget, void *event )
{
  pulse_info *info;
  gchar *cmd;
  gint cidx;

  if(!params || !params[0])
    return g_strdup("");

  if(!g_ascii_strncasecmp(params[0],"sink-",5))
  {
    info = pulse_addr_parse(params[1], &sink_list, sink_name, &cidx);
    cmd = params[0]+5;
  }
  else if(!g_ascii_strncasecmp(params[0],"source-",7))
  {
    info = pulse_addr_parse(params[1], &source_list, source_name, &cidx);
    cmd = params[0]+7;
  }
  else
    return g_strdup("");

  if(!info)
    return g_strdup("");

  if(!g_ascii_strcasecmp(cmd, "volume"))
    return g_strdup_printf("%f",cidx?info->cvol.values[cidx-1]*100.0/PA_VOLUME_NORM:info->vol);
  if(!g_ascii_strcasecmp(cmd, "mute"))
    return g_strdup_printf("%d",info->mute);
  if(!g_ascii_strcasecmp(cmd, "icon"))
    return g_strdup(info->icon?info->icon:"");
  if(!g_ascii_strcasecmp(cmd, "form"))
    return g_strdup(info->form?info->form:"");
  if(!g_ascii_strcasecmp(cmd, "port"))
    return g_strdup(info->port?info->port:"");
  if(!g_ascii_strcasecmp(cmd, "monitor"))
    return g_strdup(info->monitor?info->monitor:"");
  if(!g_ascii_strcasecmp(cmd, "description"))
    return g_strdup(info->description?info->description:"");

  return g_strdup_printf("invalid query: %s", cmd);
}

static void *pulse_channel_func ( void **params, void *widget, void *event )
{
  gchar *result;

  if(!params || !params[0])
    return g_strdup("");

  if( (result = module_queue_get_string(&channel_q, params[0])) )
    return result;
  if( (result = module_queue_get_string(&remove_q, params[0])) )
    return result;

  return g_strdup("");
}

static pa_cvolume *pulse_adjust_volume ( pulse_info *info, gint cidx,
    gchar *vstr )
{
  gint vdelta;

  if(!vstr)
    return &(info->cvol);

  while(*vstr==' ')
    vstr++;

  vdelta = g_ascii_strtod(vstr, NULL)*PA_VOLUME_NORM/100;
  if(*vstr!='+' && *vstr!='-')
    vdelta -= cidx?info->cvol.values[cidx-1]:pa_cvolume_avg(&info->cvol);

  if(!cidx)
  {
    if(vdelta > 0)
      pa_cvolume_inc_clamp(&info->cvol, vdelta, PA_VOLUME_UI_MAX);
    else
      pa_cvolume_dec(&info->cvol, -vdelta);
  }
  else
    info->cvol.values[cidx-1] = CLAMP((gint)(info->cvol.values[cidx-1]) +
        vdelta, 0, PA_VOLUME_UI_MAX);

  return &info->cvol;
}

static gboolean pulse_mute_parse ( gchar *cmd, gboolean mute )
{
  while(*cmd==' ')
    cmd++;

  if(!g_ascii_strcasecmp(cmd, "toggle"))
    return !mute;
  else if(!g_ascii_strcasecmp(cmd, "true"))
    return TRUE;
  else if(!g_ascii_strcasecmp(cmd, "false"))
    return FALSE;

  return mute;
}

static void pulse_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  pulse_info *info;
  pa_operation *op;
  gint cidx;

  if(!g_ascii_strncasecmp(cmd, "sink-", 5))
    info = pulse_addr_parse(name, &sink_list, sink_name, &cidx);
  else if(!g_ascii_strncasecmp(cmd, "source-", 7))
    info = pulse_addr_parse(name, &source_list, source_name, &cidx);
  else
    info = NULL;

  if(!info)
    return;

  if(!g_ascii_strncasecmp(cmd, "sink-volume", 11))
    op = pa_context_set_sink_volume_by_index(pctx, info->idx,
        pulse_adjust_volume(info, cidx, cmd+11), NULL, NULL);
  else if(!g_ascii_strncasecmp(cmd, "source-volume", 13))
    op = pa_context_set_source_volume_by_index(pctx, info->idx,
        pulse_adjust_volume(info, cidx, cmd+13), NULL,NULL);
  else if(!g_ascii_strncasecmp(cmd, "sink-mute", 9))
    op = pa_context_set_sink_mute_by_index(pctx, info->idx,
        pulse_mute_parse(cmd+9,info->mute), NULL, NULL);
  else if(!g_ascii_strncasecmp(cmd, "source-mute", 11))
    op = pa_context_set_sink_mute_by_index(pctx, info->idx,
        pulse_mute_parse(cmd+11,info->mute), NULL, NULL);
  else
    op = NULL;

  if(op)
    pa_operation_unref(op);
}

static void pulse_channel_ack_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&channel_q);
}

static void pulse_channel_ack_removed_action ( gchar *cmd, gchar *name,
    void *d1, void *d2, void *d3, void *d4 )
{
  module_queue_remove(&remove_q);
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

ModuleExpressionHandlerV1 handler1 = {
  .flags = 0,
  .name = "Pulse",
  .parameters = "Ss",
  .function = pulse_expr_func
};

ModuleExpressionHandlerV1 handler2 = {
  .flags = 0,
  .name = "PulseChannel",
  .parameters = "S",
  .function = pulse_channel_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  &handler2,
  NULL
};

ModuleActionHandlerV1 act_handler1 = {
  .name = "PulseCmd",
  .function = pulse_action
};

ModuleActionHandlerV1 act_handler2 = {
  .name = "PulseSetDefaultSink",
  .function = pulse_set_sink_action
};

ModuleActionHandlerV1 act_handler3 = {
  .name = "PulseSetDefaultSource",
  .function = pulse_set_source_action
};

ModuleActionHandlerV1 act_handler4 = {
  .name = "PulseChannelAck",
  .function = pulse_channel_ack_action
};

ModuleActionHandlerV1 act_handler5 = {
  .name = "PulseChannelAckRemoved",
  .function = pulse_channel_ack_removed_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  &act_handler3,
  &act_handler4,
  &act_handler5,
  NULL
};
