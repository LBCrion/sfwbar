/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <alsa/asoundlib.h>
#include "../src/module.h"
#include "../src/basewidget.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;

static GSource *main_src;
static snd_mixer_t *mixer;
static  struct pollfd *pfds;
static  int pfdcount;

typedef struct _mixer_api {
  int (*has_volume)( snd_mixer_elem_t *);
  int (*has_channel)( snd_mixer_elem_t *, snd_mixer_selem_channel_id_t );
  int (*get_range) ( snd_mixer_elem_t *, long *, long *);
  int (*get_channel)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *);
  int (*set_channel)(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long );
  int (*has_switch)( snd_mixer_elem_t *);
  int (*get_switch)( snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, int *);
  int (*set_switch) ( snd_mixer_elem_t *, int );
} mixer_api_t;

static mixer_api_t playback_api = {
  .has_volume = snd_mixer_selem_has_playback_volume,
  .has_channel = snd_mixer_selem_has_playback_channel,
  .get_range = snd_mixer_selem_get_playback_volume_range,
  .get_channel = snd_mixer_selem_get_playback_volume,
  .set_channel = snd_mixer_selem_set_playback_volume,
  .has_switch = snd_mixer_selem_has_playback_switch,
  .get_switch = snd_mixer_selem_get_playback_switch,
  .set_switch = snd_mixer_selem_set_playback_switch_all
};

static mixer_api_t capture_api = {
  .has_volume = snd_mixer_selem_has_capture_volume,
  .has_channel = snd_mixer_selem_has_capture_channel,
  .get_range = snd_mixer_selem_get_capture_volume_range,
  .get_channel = snd_mixer_selem_get_capture_volume,
  .set_channel = snd_mixer_selem_set_capture_volume,
  .has_switch = snd_mixer_selem_has_capture_switch,
  .get_switch = snd_mixer_selem_get_capture_switch,
  .set_switch = snd_mixer_selem_set_capture_switch_all
};

gboolean alsa_source_prepare(GSource *source, gint *timeout)
{
  *timeout = -1;
  return FALSE;
}

gboolean alsa_source_check( GSource *source )
{
  gushort revents;

  snd_mixer_poll_descriptors_revents(mixer, pfds, pfdcount, &revents);
  return !!(revents & POLLIN);
}

gboolean alsa_source_dispatch( GSource *source,GSourceFunc cb, gpointer data)
{
  snd_mixer_handle_events(mixer);
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("alsa"));

  return TRUE;
}

static void alsa_source_finalize ( GSource *source )
{
  g_clear_pointer(&mixer,snd_mixer_close);
  g_clear_pointer(&pfds,g_free);
}

static GSourceFuncs alsa_source_funcs = {
  alsa_source_prepare,
  alsa_source_check,
  alsa_source_dispatch,
  alsa_source_finalize
};

#define alsa_source_bail(x) { g_source_destroy(x); return NULL; }

static GSource *alsa_source_subscribe ( gchar *name )
{
  GSource *source;

  source = g_source_new(&alsa_source_funcs, sizeof(GSource));

  if(snd_mixer_open(&mixer, 0) < 0)
    alsa_source_bail(source);
  if(snd_mixer_attach(mixer, name) < 0)
    alsa_source_bail(source);
  if(snd_mixer_selem_register(mixer, NULL, NULL) < 0)
    alsa_source_bail(source);
  if(snd_mixer_load(mixer) < 0)
    alsa_source_bail(source);
  pfdcount = snd_mixer_poll_descriptors_count (mixer);
  if(pfdcount <= 0)
    alsa_source_bail(source);
  pfds = g_malloc(sizeof(struct pollfd) * pfdcount);
  if(snd_mixer_poll_descriptors(mixer, pfds, pfdcount) < 0)
    alsa_source_bail(source);

  g_source_attach(source,NULL);
  g_source_set_priority(source,G_PRIORITY_DEFAULT);
  g_source_add_poll(source,(GPollFD *)pfds);

  return source;
}

void sfwbar_module_init ( ModuleApiV1 *api )
{
  main_src = alsa_source_subscribe("default");
}

static snd_mixer_elem_t *alsa_element_get ( gchar *name )
{
  snd_mixer_selem_id_t *sid;

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, name?name:"Master");
  return snd_mixer_find_selem(mixer, sid);
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

static gdouble alsa_volume_get ( snd_mixer_elem_t *element, mixer_api_t *api )
{
  glong min, max, vol;

  if(!api->has_volume(element))
    return 0;

  api->get_range(element, &min, &max);
  vol = alsa_volume_avg_get(element, api );
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

void *alsa_expr_func ( void **params, void *widget, void *event )
{
  snd_mixer_elem_t *element;
  gdouble *result;

  result = g_malloc0(sizeof(gdouble));
  if(!params || !params[0])
    return result;

  element = alsa_element_get(params[1]);

  if(!g_ascii_strcasecmp(params[0],"playback-volume"))
    *result = alsa_volume_get(element, &playback_api);
  else if(!g_ascii_strcasecmp(params[0],"capture-volume"))
    *result = alsa_volume_get(element, &capture_api);
  else if(!g_ascii_strcasecmp(params[0],"playback-mute"))
    *result = alsa_mute_get(element, &playback_api);
  else if(!g_ascii_strcasecmp(params[0],"capture-mute"))
    *result = alsa_mute_get(element, &capture_api);
  return result;
}

static void alsa_volume_adjust ( snd_mixer_elem_t *element, gchar *vstr,
    mixer_api_t *api )
{
  long min, max, vol, vdelta;
  gint i;

  if(!api->has_volume(element))
    return;

  api->get_range(element, &min, &max);
  vol = alsa_volume_avg_get(element, api);

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

  for(i=0;i<=SND_MIXER_SCHN_LAST;i++)
    if(api->has_channel(element, i))
    {
      api->get_channel(element, i, &vol);
      api->set_channel(element, i, CLAMP(vol + vdelta,min,max));
    }
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

static void alsa_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  snd_mixer_elem_t *element;

  element = alsa_element_get ( name );

  if(!g_ascii_strncasecmp(cmd,"playback-volume",15))
    alsa_volume_adjust(element, cmd+15, &playback_api);
  else if(!g_ascii_strncasecmp(cmd,"playback-mute",13))
    alsa_mute_set(element, cmd+13, &playback_api);
  else if(!g_ascii_strncasecmp(cmd,"capture-volume",14))
    alsa_volume_adjust(element, cmd+14, &capture_api);
  else if(!g_ascii_strncasecmp(cmd,"capture-mute",12))
    alsa_mute_set(element, cmd+12, &capture_api);
  else
    return;

  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("alsa"));
}

static void alsa_card_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!cmd)
    return;

  g_clear_pointer((GSource **)&main_src,g_source_destroy);
  main_src = alsa_source_subscribe(cmd);
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "Alsa",
  .parameters = "Ss",
  .function = alsa_expr_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};

ModuleActionHandlerV1 act_handler1 = {
  .name = "AlsaCmd",
  .function = alsa_action
};

ModuleActionHandlerV1 act_handler2 = {
  .name = "AlsaSetCard",
  .function = alsa_card_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  NULL
};
