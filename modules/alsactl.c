/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <alsa/asoundlib.h>
#include "module.h"
#include "trigger.h"
#include "util/string.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
extern ModuleInterfaceV1 sfwbar_interface;

typedef struct _alsa_source {
  GSource src;
  gchar *name, *desc;
  snd_mixer_t *mixer;
  struct pollfd *pfds;
  int pfdcount;
} alsa_source_t;

typedef struct _mixer_api {
  gchar *prefix;
  gchar *default_name;
  int (*has_volume)( snd_mixer_elem_t *);
  int (*has_channel)( snd_mixer_elem_t *, snd_mixer_selem_channel_id_t );
  int (*get_range) ( snd_mixer_elem_t *, long *, long *);
  int (*get_channel)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *);
  int (*set_channel)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long );
  int (*has_switch)( snd_mixer_elem_t *);
  int (*get_switch)( snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, int *);
  int (*set_switch) ( snd_mixer_elem_t *, int );
} mixer_api_t;

typedef struct _alsa_channel {
  gchar *card, *element;
  snd_mixer_selem_channel_id_t channel;
  gchar *name;
  mixer_api_t *api;
} alsa_channel_t;

static alsa_source_t *main_src;
GHashTable *alsa_sources;

static mixer_api_t playback_api = {
  .prefix = "sink",
  .has_volume = snd_mixer_selem_has_playback_volume,
  .get_range = snd_mixer_selem_get_playback_volume_range,
  .has_channel = snd_mixer_selem_has_playback_channel,
  .get_channel = snd_mixer_selem_get_playback_volume,
  .set_channel = snd_mixer_selem_set_playback_volume,
  .has_switch = snd_mixer_selem_has_playback_switch,
  .get_switch = snd_mixer_selem_get_playback_switch,
  .set_switch = snd_mixer_selem_set_playback_switch_all
};

static mixer_api_t capture_api = {
  .prefix = "source",
  .has_volume = snd_mixer_selem_has_capture_volume,
  .has_channel = snd_mixer_selem_has_capture_channel,
  .get_range = snd_mixer_selem_get_capture_volume_range,
  .get_channel = snd_mixer_selem_get_capture_volume,
  .set_channel = snd_mixer_selem_set_capture_volume,
  .has_switch = snd_mixer_selem_has_capture_switch,
  .get_switch = snd_mixer_selem_get_capture_switch,
  .set_switch = snd_mixer_selem_set_capture_switch_all
};

static void alsa_channel_free ( alsa_channel_t *channel )
{
  g_free(channel->card);
  g_free(channel->element);
  g_free(channel->name);
  g_free(channel);
}

static gboolean alsa_channel_comp ( alsa_channel_t *c1, alsa_channel_t *c2 )
{
  return (c1->channel==c2->channel && c1->api==c2->api &&
    !g_strcmp0(c1->card, c2->card) && !g_strcmp0(c1->element, c2->element));
}

static void *alsa_channel_get_str ( alsa_channel_t *channel, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "interface"))
    return g_strdup(channel->api->prefix);
  if(!g_ascii_strcasecmp(prop, "name"))
    return g_strconcat(channel->element, " ", channel->name, NULL);
  if(!g_ascii_strcasecmp(prop, "id"))
    return g_strconcat(channel->element, ":", channel->name, NULL);
  if(!g_ascii_strcasecmp(prop, "device"))
    return g_strdup(channel->card);
  if(!g_ascii_strcasecmp(prop, "index"))
    return g_strdup_printf("%d", channel->channel);
  return NULL;
}

static module_queue_t channel_q = {
  .free = (void (*)(void *))alsa_channel_free,
  .duplicate = (void *(*)(void *))ptr_pass,
  .get_str = (void *(*)(void *, gchar *))alsa_channel_get_str,
  .compare = (gboolean (*)(const void *, const void *))alsa_channel_comp,
};

static void *alsa_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "removed-id"))
    return g_strdup(name);
  return NULL;
}

static module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))g_strdup,
  .get_str = (void *(*)(void *, gchar *))alsa_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static snd_mixer_elem_t *alsa_element_get ( snd_mixer_t *mixer, gchar *name )
{
  snd_mixer_selem_id_t *sid;

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, (name && *name)?name:"Master");
  return snd_mixer_find_selem(mixer, sid);
}

static snd_mixer_selem_channel_id_t alsa_channel_parse ( gchar *name )
{
  snd_mixer_selem_channel_id_t i;

  if(!g_ascii_strcasecmp(name, "mono"))
    return SND_MIXER_SCHN_MONO;

  for(i=0;i<=SND_MIXER_SCHN_LAST;i++)
    if(!g_ascii_strcasecmp(name, snd_mixer_selem_channel_name(i)))
      return i;

  return -1;
}

static gchar *alsa_device_get ( gchar *addr, gchar **remnant )
{
  gchar *ptr;

  ptr = strchr(addr + (g_str_has_prefix(addr, "hw:")? 3: 0), ':');
  if(remnant)
    *remnant = ptr;
  if(ptr)
    return g_strndup(addr, ptr-addr);
  else
    return g_strdup(addr);
}

static gboolean alsa_addr_parse ( mixer_api_t *api, gchar *address,
    alsa_source_t **src, snd_mixer_elem_t **elem,
    snd_mixer_selem_channel_id_t *channel )
{
  gchar *ptr, *device, *chan_ptr, *elem_str;

  if(!address || !*address)
    address = api->default_name?api->default_name:"default";

  device = alsa_device_get(address, &ptr);

  *src = g_hash_table_lookup(alsa_sources, device);
  g_free(device);

  if(!*src)
    return FALSE;

  if(!ptr)
  {
    *elem = alsa_element_get((*src)->mixer, NULL);
    *channel = -1;
    return TRUE;
  }

  ptr++;
  if( (chan_ptr = strchr(ptr, ':')) )
  {
    elem_str = g_strndup(ptr, chan_ptr-ptr);
    *elem = alsa_element_get((*src)->mixer, elem_str);
    g_free(elem_str);
    *channel = alsa_channel_parse(chan_ptr+1);
  }
  else
  {
    *elem = alsa_element_get((*src)->mixer, ptr);
    *channel = -1;
  }

  return TRUE;
}

static void alsa_iface_advertise ( mixer_api_t *api, alsa_source_t *src )
{
  snd_mixer_elem_t *elem;
  alsa_channel_t *channel;
  gint i, cnum=0;

  for(elem=snd_mixer_first_elem(src->mixer); elem;
      elem=snd_mixer_elem_next(elem))
  {
    if(api->has_volume(elem) && !snd_mixer_selem_has_common_volume(elem))
      for(i=0;i<=SND_MIXER_SCHN_LAST;i++)
        if(api->has_channel(elem, i))
        {
          channel = g_malloc0(sizeof(alsa_channel_t));
          channel->api = api;
          channel->channel = cnum++;
          channel->card = g_strdup(src->name);
          channel->element = g_strdup(snd_mixer_selem_get_name(elem));
          channel->name = g_strdup( snd_mixer_selem_is_playback_mono(elem)?
              "Mono": snd_mixer_selem_channel_name(i));
          module_queue_append(&channel_q, channel);
        }
  }
}

gboolean alsa_source_prepare(GSource *source, gint *timeout)
{
  *timeout = -1;
  return FALSE;
}

gboolean alsa_source_check( GSource *gsrc )
{
  alsa_source_t *src = (alsa_source_t *)gsrc;
  gushort revents;

  if(!src->pfds)
    return FALSE;
  snd_mixer_poll_descriptors_revents(src->mixer, src->pfds, src->pfdcount,
      &revents);
  return !!(revents & POLLIN);
}

gboolean alsa_source_dispatch( GSource *gsrc, GSourceFunc cb, gpointer data)
{
  alsa_source_t *src = (alsa_source_t *)gsrc;

  snd_mixer_handle_events(src->mixer);
  trigger_emit("volume");

  return TRUE;
}

static void alsa_source_finalize ( GSource *gsrc )
{
  alsa_source_t *src = (alsa_source_t *)gsrc;

  if(src->name)
    module_queue_append(&remove_q, src->name);

  g_clear_pointer(&src->mixer, snd_mixer_close);
  g_clear_pointer(&src->pfds, g_free);
  g_clear_pointer(&src->name, g_free);
  g_clear_pointer(&src->desc, g_free);
}

static GSourceFuncs alsa_source_funcs = {
  .prepare = alsa_source_prepare,
  .check = alsa_source_check,
  .dispatch = alsa_source_dispatch,
  .finalize = alsa_source_finalize
};

#define alsa_source_bail(x) { g_source_destroy((GSource *)x); return NULL; }

static alsa_source_t *alsa_source_subscribe ( gchar *name )
{
  alsa_source_t *src;

  src = (alsa_source_t *)g_source_new(&alsa_source_funcs,
      sizeof(alsa_source_t));

  if(snd_mixer_open(&src->mixer, 0) < 0)
    alsa_source_bail(src);
  if(snd_mixer_attach(src->mixer, name) < 0)
    alsa_source_bail(src);
  if(snd_mixer_selem_register(src->mixer, NULL, NULL) < 0)
    alsa_source_bail(src);
  if(snd_mixer_load(src->mixer) < 0)
    alsa_source_bail(src);
  src->pfdcount = snd_mixer_poll_descriptors_count (src->mixer);
  if(src->pfdcount <= 0)
    alsa_source_bail(src);
  src->pfds = g_malloc(sizeof(struct pollfd) * src->pfdcount);
  if(snd_mixer_poll_descriptors(src->mixer, src->pfds, src->pfdcount) < 0)
    alsa_source_bail(src);

  src->name = g_strdup(name);

  snd_ctl_t *ctl;
  snd_ctl_card_info_t *info;
  snd_ctl_card_info_alloca(&info);
  if(snd_ctl_open(&ctl, name, 0) == 0)
  {
    if(snd_ctl_card_info(ctl, info)==0)
      src->desc  = g_strdup(snd_ctl_card_info_get_name(info));
    snd_ctl_close(ctl);
  }

  g_source_attach((GSource *)src, NULL);
  g_source_set_priority((GSource *)src, G_PRIORITY_DEFAULT);
  g_source_add_poll((GSource *)src, (GPollFD *)src->pfds);

  if(!playback_api.default_name)
    playback_api.default_name = g_strdup(src->name);
  if(!capture_api.default_name)
    capture_api.default_name = g_strdup(src->name);

  alsa_iface_advertise(&playback_api, src);
  alsa_iface_advertise(&capture_api, src);

  g_hash_table_insert(alsa_sources, src->name, src);

  return src;
}

void alsa_source_subscribe_all ( void )
{
  gchar *id;
  gint i;

  main_src = alsa_source_subscribe("default");
  for(i=-1; snd_card_next(&i)>=0 && i>=0;)
  {
    id = g_strdup_printf("hw:%d", i);
    alsa_source_subscribe(id);
    g_free(id);
  }
  trigger_emit("volume");
}

void alsa_source_remove ( GSource *src )
{
  g_source_unref(src);
  g_source_destroy(src);
}

static glong alsa_volume_avg_get ( snd_mixer_elem_t *element, mixer_api_t *api )
{
  glong vol, tvol = 0;
  gint i, count = 0;

  for(i=0;i<=SND_MIXER_SCHN_LAST;i++)
    if(api->has_channel(element, i))
    {
      api->get_channel(element, i, &vol);
      tvol += vol;
      count++;
    }

  return tvol/count;
}

static gdouble alsa_volume_get ( snd_mixer_elem_t *element,
    snd_mixer_selem_channel_id_t channel, mixer_api_t *api )
{
  glong min, max, vol;

  if(!api->has_volume(element))
    return 0;

  api->get_range(element, &min, &max);
  if(channel < 0)
    vol = alsa_volume_avg_get(element, api );
  else
    api->get_channel(element, channel, &vol);
  return ((gdouble)vol-min)/(max-min)*100;
}

static gdouble alsa_mute_get ( snd_mixer_elem_t *element, mixer_api_t *api )
{
  gint pb;

  if(!api->has_switch(element))
    return FALSE;

  api->get_switch(element, 0, &pb);
  return !pb;
}

mixer_api_t *alsa_api_parse ( gchar *cmd, gchar **verb )
{
  if(!g_ascii_strncasecmp(cmd, "sink-", 5))
  {
    *verb = cmd + 5;
    return &playback_api;
  }
  if(!g_ascii_strncasecmp(cmd, "source-", 7))
  {
    *verb = cmd + 7;
    return &playback_api;
  }
    return NULL;
}

static value_t alsa_func_volume ( vm_t *vm, value_t p[], gint np )
{
  snd_mixer_elem_t *element;
  snd_mixer_selem_channel_id_t channel;
  alsa_source_t *src;
  gchar *verb;
  mixer_api_t *api;

  vm_param_check_np_range(vm, np, 1, 2, "Volume");
  vm_param_check_string(vm, p, 0, "Volume");
  if(np==2)
    vm_param_check_string(vm, p, 1, "Volume");

  if( !(api = alsa_api_parse(value_get_string(p[np-1]), &verb)) )
    return value_na;

  if(!g_ascii_strcasecmp(verb, "count"))
    return value_new_numeric( g_hash_table_size(alsa_sources));

  if(!alsa_addr_parse(api, (np==2)? value_get_string(p[0]) : NULL, &src,
        &element, &channel) || !element)
    return value_na;

  if(!g_ascii_strcasecmp(verb, "volume"))
    return value_new_numeric(alsa_volume_get(element, channel, api));
  if(!g_ascii_strcasecmp(verb, "mute"))
    return value_new_numeric(alsa_mute_get(element, api));
  if(!g_ascii_strcasecmp(verb, "is-default"))
    return value_new_numeric(!g_strcmp0(
          api->default_name? api->default_name : "default", src->name));

  return value_na;
}

static value_t alsa_func_volume_info ( vm_t *vm, value_t p[], gint np )
{
  snd_mixer_elem_t *element;
  snd_mixer_selem_channel_id_t channel;
  alsa_source_t *src;
  gchar *verb;
  mixer_api_t *api;

  vm_param_check_np_range(vm, np, 1, 2, "VolumeInfo");
  vm_param_check_string(vm, p, 0, "VolumeInfo");
  if(np==2)
    vm_param_check_string(vm, p, 1, "VolumeInfo");

  if( !(api = alsa_api_parse(value_get_string(p[np-1]), &verb)) )
    return value_na;

  if(!alsa_addr_parse(api, (np==2)? value_get_string(p[0]) : NULL, &src,
        &element, &channel) || !element)
    return value_na;

  if(!g_ascii_strcasecmp(verb, "description"))
    return value_new_string(g_strdup(src->desc));

  return value_na;
}

static value_t alsa_func_channel ( vm_t *vm, value_t p[], gint np )
{
  gchar *result;

  vm_param_check_np(vm, np, 1, "VolumeConf");
  vm_param_check_string(vm, p, 0, "VolumeConf");

  if( (result = module_queue_get_string(&channel_q, value_get_string(p[0]))) )
    return value_new_string(result);
  if( (result = module_queue_get_string(&remove_q, value_get_string(p[0]))) )
    return value_new_string(result);

  return value_na;
}

static void alsa_volume_set ( snd_mixer_elem_t *element,
    snd_mixer_selem_channel_id_t channel, gchar *vstr, mixer_api_t *api )
{
  long min, max, vol, vdelta;
  gint i;

  if(!api->has_volume(element))
    return;

  api->get_range(element, &min, &max);
  vol = alsa_volume_get(element, channel, api)*(max-min)/100 + min;

  while(*vstr==' ')
    vstr++;

  vdelta = g_ascii_strtod(vstr,NULL);
  vdelta = (vdelta*(max-min) + ((vdelta<0)?-50:50))/100;
  switch(*vstr)
  {
    case '+':
      vdelta = MAX(1,vdelta);
      break;
    case '-':
      vdelta = MIN(-1,vdelta);
      break;
    default:
      vdelta = vdelta - vol;
      break;
  }

  if(channel < 0)
  {
    for(i=0;i<=SND_MIXER_SCHN_LAST;i++)
      if(api->has_channel(element, i))
      {
        api->get_channel(element, i, &vol);
        api->set_channel(element, i, CLAMP(vol + vdelta, min, max));
      }
  }
  else
    api->set_channel(element, channel, CLAMP(vol + vdelta, min, max));
}

static void alsa_mute_set ( snd_mixer_elem_t *element, gchar *vstr,
   mixer_api_t *api )
{
  gint pb;

  if(!api->has_switch(element))
    return;

  while(*vstr==' ')
    vstr++;

  if(!g_ascii_strcasecmp(vstr,"on"))
    api->set_switch(element,0);
  else if(!g_ascii_strcasecmp(vstr,"off"))
    api->set_switch(element,1);
  else if(!g_ascii_strcasecmp(vstr,"toggle"))
  {
    api->get_switch(element, 0, &pb);
    api->set_switch(element, !pb);
  }
}

static void alsa_default_set ( mixer_api_t *api, gchar *name )
{
  gchar *new;

  if(!api || !name)
    return;

  while(*name==' ')
    name++;

  if( (new = alsa_device_get(name, NULL)) )
  {
    g_free(api->default_name);
    api->default_name = new;
    trigger_emit("volume");
  }
}

static value_t alsa_action_volumectl ( vm_t *vm, value_t p[], gint np )
{
  alsa_source_t *src;
  snd_mixer_elem_t *element;
  snd_mixer_selem_channel_id_t channel;
  gchar *verb;
  mixer_api_t *api;

  vm_param_check_np_range(vm, np, 1, 2, "VolumeCtl");
  vm_param_check_string(vm, p, 0, "VolumeCtl");
  if(np==2)
    vm_param_check_string(vm, p, 1, "VolumeCtl");

  if( !(api = alsa_api_parse(value_get_string(p[np-1]), &verb)) )
    return value_na;

  if(!g_ascii_strncasecmp(verb, "set-default", 11))
  {
    alsa_default_set(api, verb+11);
    return value_na;
  }

  if(!alsa_addr_parse(api, (np==2)? value_get_string(p[0]) : NULL, &src,
        &element, &channel) || !element)
    return value_na;

  if(!g_ascii_strncasecmp(verb, "volume", 6))
    alsa_volume_set(element, channel, verb+6, api);
  else if(!g_ascii_strncasecmp(verb, "mute", 4))
    alsa_mute_set(element, verb+4, api);
  else
    return value_na;

  trigger_emit("volume");
  return value_na;
}

static value_t alsa_action_volumeack ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "VolumeAck");
  vm_param_check_string(vm, p, 0, "VolumeAck");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "volume-conf"))
    module_queue_remove(&channel_q);
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "volume-conf-removed"))
    module_queue_remove(&remove_q);
  if(!sfwbar_interface.ready)
  {
    sfwbar_interface.active = !!channel_q.list | !!remove_q.list;
    module_interface_select(sfwbar_interface.interface);
    if(!sfwbar_interface.active)
      sfwbar_interface.ready = TRUE;
  }

  return value_na;
}

void alsa_activate ( void )
{
  vm_func_add("volume", alsa_func_volume, FALSE);
  vm_func_add("volumeinfo", alsa_func_volume_info, FALSE);
  vm_func_add("volumeconf", alsa_func_channel, FALSE);
  vm_func_add("volumectl", alsa_action_volumectl, TRUE);
  vm_func_add("volumeack", alsa_action_volumeack, TRUE);
  g_idle_add((GSourceFunc)alsa_source_subscribe_all, NULL);
}

void alsa_deactivate ( void )
{
  g_hash_table_remove_all(alsa_sources);
  g_clear_pointer(&playback_api.default_name, g_free);
  g_clear_pointer(&capture_api.default_name, g_free);
  sfwbar_interface.active = !!channel_q.list | !!remove_q.list;
}

void alsa_finalize ( void )
{
  vm_func_remove("volume");
  vm_func_remove("volumeinfo");
  vm_func_remove("volumeconf");
  vm_func_remove("volumectl");
  vm_func_remove("volumeack");
}

gboolean sfwbar_module_init ( void )
{
  gint card = -1;

  channel_q.trigger = g_intern_static_string("volume-conf");
  remove_q.trigger = g_intern_static_string("volume-conf-removed");
  alsa_sources = g_hash_table_new_full( g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)alsa_source_remove );

  if(snd_card_next(&card) >=0 && card >= 0)
    module_interface_activate(&sfwbar_interface);
  return TRUE;
}

ModuleInterfaceV1 sfwbar_interface = {
  .interface = "volume-control",
  .provider = "ALSA",
  .activate = alsa_activate,
  .deactivate = alsa_deactivate,
  .finalize = alsa_finalize
};
