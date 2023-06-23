/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include <alsa/asoundlib.h>
#include "../src/module.h"

typedef struct _alsa_source {
  GSource source;
  snd_mixer_t *mixer;
  snd_mixer_elem_t* element;
  snd_mixer_selem_id_t *sid;
  struct pollfd *pfds;
  int pfdcount;
} alsa_source_t;

ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;

alsa_source_t *main_src;
gdouble volume, cvolume;
gboolean mute, cmute;

void alsa_volume_get ( alsa_source_t *source )
{
  long min, max, vol;
  gint pb;

  if(snd_mixer_selem_has_playback_volume(source->element))
  {
    snd_mixer_selem_get_playback_volume_range(source->element, &min, &max);
    snd_mixer_selem_get_playback_volume(source->element, 0, &vol);
    volume = ((gdouble)vol-min)/(max-min);
  }
  else
    volume = 0.0;

  if(snd_mixer_selem_has_capture_volume(source->element))
  {
    snd_mixer_selem_get_capture_volume_range(source->element, &min, &max);
    snd_mixer_selem_get_capture_volume(source->element, 0, &vol);
    cvolume = (vol-min)/(max-min);
  }
  else
    cvolume = 0.0;

  if(snd_mixer_selem_has_playback_switch(source->element))
  {
    snd_mixer_selem_get_playback_switch(source->element, 0, &pb);
    mute = !pb;
  }
  else
    mute = 0;

  if(snd_mixer_selem_has_capture_switch(source->element))
  {
    snd_mixer_selem_get_capture_switch(source->element, 0, &pb);
    cmute = !pb;
  }
  else
    cmute = 0;

  MODULE_TRIGGER_EMIT("alsa");
}

gboolean alsa_source_prepare(GSource *source, gint *timeout)
{
  *timeout = -1;
  return FALSE;
}

gboolean alsa_source_check( alsa_source_t *source )
{
  gushort revents;

  snd_mixer_poll_descriptors_revents(source->mixer, source->pfds,
      source->pfdcount, &revents);
  return !!(revents & POLLIN);
}

gboolean alsa_source_dispatch( alsa_source_t *source,GSourceFunc cb, gpointer data)
{
  snd_mixer_handle_events(source->mixer);
  alsa_volume_get(source);

  return TRUE;
}

static void alsa_source_finalize ( alsa_source_t *source )
{
  g_clear_pointer(&source->sid,snd_mixer_selem_id_free);
  g_clear_pointer((&source->mixer),snd_mixer_close);
  g_clear_pointer((&source->pfds),g_free);
}

static GSourceFuncs alsa_source_funcs = {
  alsa_source_prepare,
  (gboolean (*)(GSource *))alsa_source_check,
  (gboolean (*)(GSource *, GSourceFunc, gpointer))alsa_source_dispatch,
  (void (*)(GSource *))alsa_source_finalize
};

#define alsa_source_bail(x) { g_source_destroy((GSource *)x); return NULL; }

static alsa_source_t *alsa_source_subscribe ( gchar *name )
{
  alsa_source_t *source;

  source = (alsa_source_t *)g_source_new(&alsa_source_funcs,
      sizeof(alsa_source_t));

  if(snd_mixer_open(&source->mixer, 0) < 0)
    alsa_source_bail(source);
  if(snd_mixer_attach(source->mixer, name) < 0)
    alsa_source_bail(source);
  if(snd_mixer_selem_register(source->mixer, NULL, NULL) < 0)
    alsa_source_bail(source);
  if(snd_mixer_load(source->mixer) < 0)
    alsa_source_bail(source);
  source->pfdcount = snd_mixer_poll_descriptors_count (source->mixer);
  if(source->pfdcount <= 0)
    alsa_source_bail(source);
  source->pfds = g_malloc(sizeof(struct pollfd) * source->pfdcount);
  if(snd_mixer_poll_descriptors(source->mixer, source->pfds,
        source->pfdcount) < 0)
    alsa_source_bail(source);
  if(source->sid)
    source->element = snd_mixer_find_selem(source->mixer, source->sid);

  g_source_attach((GSource *)source,NULL);
  g_source_set_priority((GSource *)source,G_PRIORITY_DEFAULT);
  g_source_add_poll((GSource *)source,(GPollFD *)source->pfds);

  return source;
}

static void alsa_set_element ( alsa_source_t *source, gchar *name )
{
  g_clear_pointer(&source->sid,snd_mixer_selem_id_free);
  snd_mixer_selem_id_malloc(&source->sid);
  snd_mixer_selem_id_set_index(source->sid, 0);
  snd_mixer_selem_id_set_name(source->sid, name);
  source->element = snd_mixer_find_selem(source->mixer, source->sid);
}

void sfwbar_module_init ( ModuleApiV1 *api )
{
  alsa_source_t *source;

  sfwbar_module_api = api;
  source = alsa_source_subscribe("default");
  main_src = source;
  alsa_set_element(source,"Master");
}

void *alsa_expr_func ( void **params, void *widget, void *event )
{
  gdouble *result;

  if(!params || !params[0])
    return g_strdup("");

  result = g_malloc0(sizeof(gdouble));

  if(!g_ascii_strcasecmp(params[0],"playback-volume"))
    *result = volume * 100;
  if(!g_ascii_strcasecmp(params[0],"playback-muted"))
    *result = mute;
  if(!g_ascii_strcasecmp(params[0],"capture-volume"))
    *result = cvolume * 100;
  if(!g_ascii_strcasecmp(params[0],"capture-muted"))
    *result = cmute;

  return result;
}

static gdouble alsa_volume_calc ( gchar *vstr, gdouble old )
{
  gdouble vol;

  if(!vstr)
    return old;

  while(*vstr==' ')
    vstr++;

  vol = g_ascii_strtod(vstr,NULL)/100;
  if(*vstr=='+' || *vstr=='-')
    vol += old;

  return vol;
}

static void alsa_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  long min, max;

  if(!g_ascii_strncasecmp(cmd,"playback-volume",15))
  {
    if(snd_mixer_selem_has_playback_volume(main_src->element))
    {
      snd_mixer_selem_get_playback_volume_range(main_src->element, &min, &max);
      snd_mixer_selem_set_playback_volume_all(main_src->element, 
         min + alsa_volume_calc(cmd+15, volume) * (max-min));
    }
  }
  else if(!g_ascii_strncasecmp(cmd,"playback-mute",13))
  {
    if(snd_mixer_selem_has_playback_switch(main_src->element))
    {
      cmd+=13;
      while(*cmd==' ')
        cmd++;
      if(!g_ascii_strcasecmp(cmd,"on"))
        snd_mixer_selem_set_playback_switch_all(main_src->element,0);
      if(!g_ascii_strcasecmp(cmd,"off"))
        snd_mixer_selem_set_playback_switch_all(main_src->element,1);
      if(!g_ascii_strcasecmp(cmd,"toggle"))
        snd_mixer_selem_set_playback_switch_all(main_src->element,mute);
    }
  }
  else if(!g_ascii_strncasecmp(cmd,"capture-volume",14))
  {
    if(snd_mixer_selem_has_capture_volume(main_src->element))
    {
      snd_mixer_selem_get_capture_volume_range(main_src->element, &min, &max);
      snd_mixer_selem_set_capture_volume_all(main_src->element, 
         min + alsa_volume_calc(cmd+14, cvolume) * (max-min));
    }

  }
  else if(!g_ascii_strncasecmp(cmd,"capture-mute",12))
  {
    if(snd_mixer_selem_has_capture_switch(main_src->element))
    {
      cmd+=12;
      while(*cmd==' ')
        cmd++;
      if(!g_ascii_strcasecmp(cmd,"on"))
        snd_mixer_selem_set_capture_switch_all(main_src->element,0);
      if(!g_ascii_strcasecmp(cmd,"off"))
        snd_mixer_selem_set_capture_switch_all(main_src->element,1);
      if(!g_ascii_strcasecmp(cmd,"toggle"))
        snd_mixer_selem_set_capture_switch_all(main_src->element,mute);
    }
  }
  else
    return;

  alsa_volume_get(main_src);
}

static void alsa_card_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!cmd)
    return;

  g_clear_pointer((GSource **)&main_src,g_source_destroy);
  main_src = alsa_source_subscribe(cmd);
  alsa_volume_get(main_src);
}

static void alsa_element_action ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!cmd)
    return;

  alsa_set_element(main_src, cmd);
  alsa_volume_get(main_src);
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "Alsa",
  .parameters = "S",
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

ModuleActionHandlerV1 act_handler3 = {
  .name = "AlsaSetElement",
  .function = alsa_element_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  &act_handler3,
  NULL
};
