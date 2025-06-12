/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include "module.h"
#include "trigger.h"
#include "util/string.h"
#include "vm/vm.h"

static gboolean pulse_connect_try ( void *data );
gboolean invalid;

typedef struct _pulse_info {
  guint32 idx, client;
  gchar *name;
  gboolean mute;
  pa_cvolume cvol;
  gchar *icon, *form, *port, *monitor, *description;
  pa_channel_map cmap;
} pulse_info;

typedef struct _pulse_interface {
  const gchar *prefix;
  gchar *default_name;
  gchar *default_device;
  gchar *default_control;
  gboolean fixed;
  GList *list;
  pa_operation *(*set_volume)(pa_context *, uint32_t, const pa_cvolume *,
      pa_context_success_cb_t, void *);
  pa_operation *(*set_mute)(pa_context *, uint32_t, int,
      pa_context_success_cb_t, void *);
  pa_operation *(*set_default)(pa_context *, const char *name,
      pa_context_success_cb_t, void *);

} pulse_interface_t;

pa_mainloop_api *papi;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
extern ModuleInterfaceV1 sfwbar_interface;

static pa_context *pctx;
static pulse_interface_t pulse_interfaces[];

static pulse_info *pulse_info_from_idx ( pulse_interface_t *iface, guint32 idx,
    gboolean new )
{
  GList *iter;
  pulse_info *info;

  for(iter=iface->list; iter; iter=g_list_next(iter))
    if(((pulse_info *)iter->data)->idx == idx)
      return iter->data;

  if(new)
  {
    info = g_malloc0(sizeof(pulse_info));
    iface->list = g_list_prepend(iface->list, info);
    return info;
  }
  else
    return NULL;
}

static pulse_interface_t pulse_interfaces[] = {
  {
    .prefix = "sink",
    .default_name = "default",
    .list = NULL,
    .set_volume = pa_context_set_sink_volume_by_index,
    .set_mute = pa_context_set_sink_mute_by_index,
    .set_default = pa_context_set_default_sink,
  },
  {
    .prefix = "source",
    .default_name = "default",
    .list = NULL,
    .set_volume = pa_context_set_source_volume_by_index,
    .set_mute = pa_context_set_source_mute_by_index,
    .set_default = pa_context_set_default_source,
  },
  {
    .prefix = "client",
    .default_name = "default",
    .list = NULL,
    .set_volume = pa_context_set_sink_input_volume,
    .set_mute = pa_context_set_sink_input_mute,
  }
};

pulse_interface_t *pulse_interface_get ( gchar *name, gchar **ptr )
{
  gint i;

  for(i=0; i<3; i++)
    if(g_str_has_prefix(name, pulse_interfaces[i].prefix))
    {
      if(ptr)
        *ptr = name + strlen(pulse_interfaces[i].prefix) + 1;
      return &pulse_interfaces[i];
    }

  return NULL;
}

static pulse_info *pulse_addr_parse ( const gchar *addr,
    pulse_interface_t *iface, gint *cidx )
{
  pa_channel_position_t cpos;
  pulse_info *info;
  GList *iter;
  gchar *device, *channel;
  gint i;
  gchar *ptr;

  if(cidx)
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

  if(device && g_str_has_prefix(device, "@pulse-"))
  {
    if( device && (ptr = strrchr(device, '-')) )
      info = pulse_info_from_idx(iface, atoi(ptr+1), FALSE);
    else
      info = NULL;
  }
  else
  {
    for(iter=iface->list; iter; iter=g_list_next(iter))
      if(!g_strcmp0(((pulse_info *)iter->data)->name,
            device?device:iface->default_control))
        break;
    info = iter?iter->data:NULL;
  }

  if(cidx && info && channel)
  {
    cpos = pa_channel_position_from_string(channel+1);
    for(i=0; i<info->cmap.channels; i++)
      if(info->cmap.map[i] == cpos)
        *cidx = i+1;
  }
  g_free(device);

  return info;
}

static void pulse_set_default_control ( pulse_interface_t *iface, const gchar *name,
    gboolean fixed )
{
  pulse_info *info;

  if(!iface || !name || (!fixed && iface->fixed))
    return;

  while(*name == ' ')
    name++;

  if(g_str_has_prefix(name, "@pulse"))
  {
    if( (info=pulse_addr_parse(name, iface, NULL)) )
      name = info->name;
  }

  iface->fixed = fixed;
  g_free(iface->default_control);
  iface->default_control = g_strdup(name);
  trigger_emit("volume");
}

static void pulse_set_default_device ( pulse_interface_t *iface,
    const gchar *name )
{
  g_free(iface->default_device);
  iface->default_device = g_strdup(name);

  pulse_set_default_control (iface, name, FALSE);
}

static void pulse_set_default ( pulse_interface_t *iface, const gchar *name )
{
  pulse_info *info;

  if(!iface || !name)
    return;

  while(*name == ' ')
    name++;

  if(g_str_has_prefix(name, "@pulse"))
  {
    if( (info=pulse_addr_parse(name, iface, NULL)) )
      name = info->name;
  }

  if(iface->set_default)
    iface->set_default(pctx, name, NULL, NULL);
}

static void pulse_remove_device ( pulse_interface_t *iface, guint32 idx )
{
  GList *iter;
  pulse_info *info;

  for(iter=iface->list; iter; iter=g_list_next(iter))
    if(((pulse_info *)iter->data)->idx == idx)
      break;

  if(iter)
  {
    info = iter->data;
    iface->list = g_list_delete_link(iface->list, iter);

    if(info->name)
      trigger_emit_with_string("volume-conf-removed", "device_id",
          g_strdup_printf("@pulse-%s-%d", iface->prefix, idx));
    g_free(info->name);
    g_free(info->icon);
    g_free(info->form);
    g_free(info->port);
    g_free(info->monitor);
    g_free(info->description);
    g_free(info);
  }
}

static void pulse_operation ( pa_operation *o, gchar *cmd )
{
  if(o)
    pa_operation_unref(o);
  else
    g_message("%s() failed: %s", cmd, pa_strerror(pa_context_errno(pctx)));
}

static void pulse_device_advertise ( gint iface_idx,
    const pa_channel_map *cmap, gint idx )
{
  vm_store_t *store;
  gint i;

  if(iface_idx<0 || iface_idx>2)
    return;

  for(i=0; i<cmap->channels; i++)
  {
    store = vm_store_new(NULL, TRUE);
    vm_store_insert_full(store, "interface", value_new_string(g_strdup(
            pulse_interfaces[iface_idx].prefix)));
    vm_store_insert_full(store, "device_id", value_new_string(
          g_strdup_printf("@pulse-%s-%d",
            pulse_interfaces[iface_idx].prefix, idx)));
    vm_store_insert_full(store, "channel_id", value_new_string(
          g_strdup(pa_channel_position_to_string(cmap->map[i]))));
    vm_store_insert_full(store, "channel_index",
        value_new_string(g_strdup_printf("%d", i)));
    trigger_emit_with_data("volume-conf", store);
    vm_store_free(store);
  }
}

static void pulse_sink_cb ( pa_context *ctx, const pa_sink_info *pinfo,
    gint eol, void *d )
{
  pulse_info *info;
  gboolean new;

  if(!pinfo)
    return;
  new = !pulse_info_from_idx(&pulse_interfaces[0], pinfo->index, FALSE);

  info = pulse_info_from_idx(&pulse_interfaces[0], pinfo->index, TRUE);

  g_free(info->name);
  info->name = g_strdup(pinfo->name);
  g_free(info->icon);
  info->icon = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_ICON_NAME));
  g_free(info->form);
  info->form = g_strdup(pa_proplist_gets(pinfo->proplist,
        PA_PROP_DEVICE_FORM_FACTOR));
  g_free(info->port);
  info->port = g_strdup(pinfo->active_port? pinfo->active_port->name : NULL);
  g_free(info->monitor);
  info->monitor = g_strdup(pinfo->monitor_source_name);
  g_free(info->description);
  info->description = g_strdup(pinfo->description);
  info->idx = pinfo->index;
  info->cvol = pinfo->volume;
  info->mute = pinfo->mute;
  info->cmap = pinfo->channel_map;

  if(new)
    pulse_device_advertise(0, &pinfo->channel_map, pinfo->index);

  trigger_emit("volume");
}

static void pulse_client_cb ( pa_context *ctx, const pa_client_info *cinfo,
    int eol, void *data )
{
  pulse_info *info;
  gboolean change = FALSE;
  GList *iter;

  if(!cinfo)
    return;

  for(iter=pulse_interfaces[2].list; iter; iter=g_list_next(iter))
  {
    info = iter->data;
    if(info->client==cinfo->index && g_strcmp0(info->description, cinfo->name))
    {
      g_free(info->description);
      info->description = g_strdup(cinfo->name);
      change = TRUE;
    }
  }

  if(change)
    trigger_emit("volume");
}

static void pulse_sink_input_cb ( pa_context *ctx,
    const pa_sink_input_info *pinfo, gint eol, void *d )
{
  pulse_info *info;
  gboolean new;

  if(!pinfo)
    return;

  new = !pulse_info_from_idx(&pulse_interfaces[2], pinfo->index, FALSE);
  info = pulse_info_from_idx(&pulse_interfaces[2], pinfo->index, TRUE);
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
  info->mute = pinfo->mute;
  info->cmap = pinfo->channel_map;
  info->client = pinfo->client;
  trigger_emit("volume");

  if(new)
    pulse_device_advertise(2, &pinfo->channel_map, pinfo->index);

  pulse_operation(pa_context_get_client_info(ctx, pinfo->client,
        pulse_client_cb, NULL), "pa_context_get_client_info");
}

static void pulse_source_cb ( pa_context *ctx, const pa_source_info *pinfo,
    gint eol, void *d )
{
  pulse_info *info;

  if(!pinfo)
    return;
  info = pulse_info_from_idx(&pulse_interfaces[1], pinfo->index, TRUE);

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
  info->mute = pinfo->mute;
  trigger_emit("volume");
}

static void pulse_server_cb ( pa_context *ctx, const pa_server_info *info,
    void *d )
{
  pulse_set_default_device(&pulse_interfaces[0], info->default_sink_name);
  pulse_set_default_device(&pulse_interfaces[1], info->default_source_name);
  pulse_operation(pa_context_get_sink_info_list(ctx, pulse_sink_cb, NULL),
      "pa_context_get_sink_info_list");
  pulse_operation(pa_context_get_source_info_list(ctx, pulse_source_cb, NULL),
      "pa_context_get_source_info_list");
  pulse_operation(pa_context_get_sink_input_info_list(ctx, pulse_sink_input_cb,
        NULL), "pa_context_get_sink_input_info_list");
}

static void pulse_subscribe_cb ( pa_context *ctx,
    pa_subscription_event_type_t type, guint idx, void *d )
{
  if((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
  {
    switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
    {
      case PA_SUBSCRIPTION_EVENT_SINK:
        pulse_remove_device(&pulse_interfaces[0], idx);
        break;
      case PA_SUBSCRIPTION_EVENT_SOURCE:
        pulse_remove_device(&pulse_interfaces[1], idx);
        break;
      case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        pulse_remove_device(&pulse_interfaces[2], idx);
        break;
    }
  }
  if(!(type & PA_SUBSCRIPTION_EVENT_CHANGE))
    return;

  switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
  {
    case PA_SUBSCRIPTION_EVENT_SERVER:
      pulse_operation(pa_context_get_server_info(ctx, pulse_server_cb, NULL),
          "pa_context_get_server_info");
      break;
    case PA_SUBSCRIPTION_EVENT_SINK:
      pulse_operation(pa_context_get_sink_info_by_index(ctx, idx,
            pulse_sink_cb, NULL), "pa_context_get_sink_info_by_index");
      break;
    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
      pulse_operation(pa_context_get_sink_input_info(ctx, idx,
            pulse_sink_input_cb, NULL),"pa_context_get_sink_input_info");
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
      pulse_operation(pa_context_get_source_info_by_index(ctx, idx,
            pulse_source_cb, NULL), "pa_context_get_source_info_by_index");
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
      break;
    case PA_SUBSCRIPTION_EVENT_CLIENT:
      pulse_operation(pa_context_get_client_info(ctx, idx, pulse_client_cb,
            NULL), "pa_context_get_client_info");
      break;
  }
}

static void pulse_state_cb ( pa_context *ctx, gpointer data )
{
  pa_context_state_t state;
  state = pa_context_get_state(ctx);

  if(state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
  {
    module_interface_deactivate(&sfwbar_interface);
    g_timeout_add (1000, (GSourceFunc )pulse_connect_try, NULL);
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    module_interface_select(sfwbar_interface.interface);
    trigger_emit("volume");
  }
  else if(state == PA_CONTEXT_READY)
  {
    pulse_operation(pa_context_get_server_info(ctx, pulse_server_cb, NULL),
        "pa_context_get_server_info");
    module_interface_activate(&sfwbar_interface);
  }
}

static gboolean pulse_connect_try ( void *data )
{
  if(sfwbar_interface.active)
    return TRUE;

  pctx = pa_context_new(papi, "sfwbar");
  pa_context_set_state_callback(pctx, pulse_state_cb, NULL);
  pa_context_connect(pctx, NULL, PA_CONTEXT_NOFLAGS, NULL);

  return FALSE;
}

static gint pulse_volume_get ( pulse_info *info, gint cidx )
{
  return cidx?info->cvol.values[cidx-1]:pa_cvolume_avg(&info->cvol);
}

static void pulse_volume_adjust ( pulse_interface_t *iface, pulse_info *info,
    gint cidx, gchar *vstr )
{
  gint vdelta;

  if(!vstr)
    return;

  while(*vstr==' ')
    vstr++;

  vdelta = g_ascii_strtod(vstr, NULL)*PA_VOLUME_NORM/100;
  if(*vstr!='+' && *vstr!='-')
    vdelta -= pulse_volume_get(info, cidx);

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

  pulse_operation(iface->set_volume(pctx, info->idx, &info->cvol, NULL, NULL),
      "volume adjust");
}

static void pulse_mute_adjust ( pulse_interface_t *iface, pulse_info *info,
    gchar *cmd )
{
  gboolean new;
  while(*cmd==' ')
    cmd++;

  if(!g_ascii_strcasecmp(cmd, "toggle"))
    new = !info->mute;
  else if(!g_ascii_strcasecmp(cmd, "true"))
    new = TRUE;
  else if(!g_ascii_strcasecmp(cmd, "false"))
    new = FALSE;
  else
    new = info->mute;

  pulse_operation( iface->set_mute(pctx, info->idx, new, NULL, NULL), "mute");
}

static void pulse_client_set_sink ( pulse_info *info, gchar *dest )
{
  pulse_info *dinfo;

  if(!info || !dest || !info->client)
    return;

  while(*dest == ' ')
    dest++;

  if( !(dinfo = pulse_addr_parse(dest, &pulse_interfaces[0], NULL)) )
    return;

  pulse_operation(pa_context_move_sink_input_by_index(pctx, info->idx,
        dinfo->idx, NULL, NULL), "pa_context_move_sink_input_by_index");
}

static value_t pulse_volume_func ( vm_t *vm, value_t p[], gint np )
{
  pulse_info *info;
  pulse_interface_t *iface;
  gchar *cmd;
  gint cidx;

  vm_param_check_np_range(vm, np, 1, 2, "Volume");
  vm_param_check_string(vm, p, 0, "Volume");
  if(np==2)
    vm_param_check_string(vm, p, 1, "Volume");

  if( !(iface = pulse_interface_get(value_get_string(p[0]), &cmd)) )
    return value_na;

  info = pulse_addr_parse(np==2? value_get_string(p[1]) : NULL, iface, &cidx);

  if(info && !g_ascii_strcasecmp(cmd, "volume"))
    return value_new_numeric(100.0*pulse_volume_get(info,cidx)/PA_VOLUME_NORM);
  if(info && !g_ascii_strcasecmp(cmd, "mute"))
    return value_new_numeric(info->mute);
  if(!g_ascii_strcasecmp(cmd, "count"))
    return value_new_numeric(g_list_length(iface->list));
  if(info && !g_ascii_strcasecmp(cmd, "is-default-device"))
    return value_new_numeric(!g_strcmp0(info->name, iface->default_device));
  if(info && !g_ascii_strcasecmp(cmd, "is-default"))
    return value_new_numeric(!g_strcmp0(info->name, iface->default_control));

  return value_na;
}

static value_t pulse_volume_info_func ( vm_t *vm, value_t p[], gint np )
{
  pulse_info *info;
  pulse_interface_t *iface;
  gchar *cmd;
  gint cidx;

  vm_param_check_np_range(vm, np, 1, 2, "VolumeInfo");
  vm_param_check_string(vm, p, 0, "VolumeInfo");
  if(np==2)
    vm_param_check_string(vm, p, 1, "VolumeInfo");

  if( !(iface = pulse_interface_get(value_get_string(p[0]), &cmd)) )
    return value_na;

  if( !(info = pulse_addr_parse(np==2? value_get_string(p[1]) : NULL, iface,
          &cidx)) )
    return value_na;

  if(!g_ascii_strcasecmp(cmd, "icon"))
    return value_new_string(g_strdup(info->icon? info->icon : ""));
  if(!g_ascii_strcasecmp(cmd, "form"))
    return value_new_string(g_strdup(info->form? info->form : ""));
  if(!g_ascii_strcasecmp(cmd, "port"))
    return value_new_string(g_strdup(info->port? info->port : ""));
  if(!g_ascii_strcasecmp(cmd, "monitor"))
    return value_new_string(g_strdup(info->monitor? info->monitor : ""));
  if(!g_ascii_strcasecmp(cmd, "description"))
    return value_new_string(g_strdup(info->description? info->description:""));

  return value_new_string(g_strdup_printf("invalid query: %s", cmd));
}

static value_t pulse_volume_ctl_action ( vm_t *vm, value_t p[], gint np )
{
  pulse_info *info;
  pulse_interface_t *iface;
  gint cidx;
  gchar *command;

  vm_param_check_np_range(vm, np, 1, 2, "VolumeCtl");
  vm_param_check_string(vm, p, 0, "VolumeCtl");
  if(np==2)
    vm_param_check_string(vm, p, 1, "VolumeCtl");

  if( !(iface = pulse_interface_get(value_get_string(p[np-1]), &command)) )
    return value_na;

  if( !(info = pulse_addr_parse(np==2? value_get_string(p[0]) : NULL, iface,
          &cidx)) )
    return value_na;

  if(!g_ascii_strncasecmp(command, "volume", 6))
    pulse_volume_adjust(iface, info, cidx, command+6);
  else if(!g_ascii_strncasecmp(command, "mute", 4))
    pulse_mute_adjust(iface, info, command+4);
  else if(!g_ascii_strncasecmp(command, "set-sink", 8))
    pulse_client_set_sink(info, command+8);
  else if(!g_ascii_strncasecmp(command, "set-default-device", 18))
    pulse_set_default(iface, command+18);
  else if(!g_ascii_strncasecmp(command, "set-default", 11))
    pulse_set_default_control(iface, command+11, TRUE);

  return value_na;
}

static void pulse_activate ( void )
{
  vm_func_add("volume", pulse_volume_func, FALSE);
  vm_func_add("volumeinfo", pulse_volume_info_func, FALSE);
  vm_func_add("volumectl", pulse_volume_ctl_action, TRUE);
  pa_context_set_subscribe_callback(pctx, pulse_subscribe_cb, NULL);
  pulse_operation(pa_context_subscribe(pctx, PA_SUBSCRIPTION_MASK_SERVER |
        PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT |
        PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
        NULL, NULL), "pa_context_subscribe");
  trigger_emit("volume");
}

static void pulse_deactivate ( void )
{
  gint i;

  g_debug("pulse: deactivating");
  pa_context_subscribe(pctx,PA_SUBSCRIPTION_MASK_NULL, NULL, NULL);
  pa_context_set_subscribe_callback(pctx, NULL, NULL);

  for(i=0; i<3; i++)
      while(pulse_interfaces[i].list)
        pulse_remove_device(&pulse_interfaces[i],
            ((pulse_info *)pulse_interfaces[i].list->data)->idx);

  sfwbar_interface.active = FALSE;
}

static void pulse_finalize ( void )
{
  vm_func_remove("volume");
  vm_func_remove("volumeinfo");
  vm_func_remove("volumeconf");
  vm_func_remove("volumectl");
  vm_func_remove("volumeack");
}

gboolean sfwbar_module_init ( void )
{
  pa_glib_mainloop *ploop;

  ploop = pa_glib_mainloop_new(g_main_context_get_thread_default());
  papi = pa_glib_mainloop_get_api(ploop);
  g_timeout_add (1000,(GSourceFunc )pulse_connect_try, NULL);

  return TRUE;
}

void sfwbar_module_invalidate ( void )
{
  invalid = TRUE;
}

ModuleInterfaceV1 sfwbar_interface = {
  .interface = "volume-control",
  .provider = "pulse",
  .activate = pulse_activate,
  .deactivate = pulse_deactivate,
  .finalize = pulse_finalize
};
