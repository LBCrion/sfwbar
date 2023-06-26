/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include <alsa/asoundlib.h>
#include "../src/module.h"

static GSource *main_src;
static snd_mixer_t *mixer;
static  struct pollfd *pfds;
static  int pfdcount;

ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;

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
  MODULE_TRIGGER_EMIT("alsa");

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
  sfwbar_module_api = api;
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

void *alsa_expr_func ( void **params, void *widget, void *event )
{
  gdouble *result;
  gchar *dname;
  snd_mixer_elem_t *element;
  glong min, max, vol;
  gint pb;

  result = g_malloc0(sizeof(gdouble));
  if(!params || !params[0])
    return result;

  dname = params[1]?params[1]:params[0];
  element = alsa_element_get( params[1]?params[0]:NULL);

  if(!g_ascii_strcasecmp(dname,"playback-volume"))
  {
    if(snd_mixer_selem_has_playback_volume(element))
    {
      snd_mixer_selem_get_playback_volume_range(element, &min, &max);
      snd_mixer_selem_get_playback_volume(element, 0, &vol);
      *result = ((gdouble)vol-min)/(max-min)*100;
    }
  }
  if(!g_ascii_strcasecmp(dname,"capture-volume"))
  {
    if(snd_mixer_selem_has_capture_volume(element))
    {
      snd_mixer_selem_get_capture_volume_range(element, &min, &max);
      snd_mixer_selem_get_capture_volume(element, 0, &vol);
      *result = (vol-min)/(max-min)*100;
    }
  }
  if(!g_ascii_strcasecmp(dname,"playback-muted"))
  {
    if(snd_mixer_selem_has_playback_switch(element))
    {
      snd_mixer_selem_get_playback_switch(element, 0, &pb);
      *result = !pb;
    }
  }
  if(!g_ascii_strcasecmp(dname,"capture-muted"))
  {
    if(snd_mixer_selem_has_capture_switch(element))
    {
      snd_mixer_selem_get_capture_switch(element, 0, &pb);
      *result = !pb;
    }
  }
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
  snd_mixer_elem_t *element;
  long min, max, vol;
  gint pb;

  element = alsa_element_get ( name );

  if(!g_ascii_strncasecmp(cmd,"playback-volume",15))
  {
    if(snd_mixer_selem_has_playback_volume(element))
    {
      snd_mixer_selem_get_playback_volume_range(element, &min, &max);
      snd_mixer_selem_get_playback_volume(element, 0, &vol);
      snd_mixer_selem_set_playback_volume_all(element, 
         min + alsa_volume_calc(cmd+15, vol) * (max-min));
    }
  }
  else if(!g_ascii_strncasecmp(cmd,"playback-mute",13))
  {
    if(snd_mixer_selem_has_playback_switch(element))
    {
      cmd+=13;
      while(*cmd==' ')
        cmd++;
      if(!g_ascii_strcasecmp(cmd,"on"))
        snd_mixer_selem_set_playback_switch_all(element,0);
      if(!g_ascii_strcasecmp(cmd,"off"))
        snd_mixer_selem_set_playback_switch_all(element,1);
      if(!g_ascii_strcasecmp(cmd,"toggle"))
      {
        snd_mixer_selem_get_playback_switch(element, 0, &pb);
        snd_mixer_selem_set_playback_switch_all(element,!pb);
      }
    }
  }
  else if(!g_ascii_strncasecmp(cmd,"capture-volume",14))
  {
    if(snd_mixer_selem_has_capture_volume(element))
    {
      snd_mixer_selem_get_capture_volume_range(element, &min, &max);
      snd_mixer_selem_get_capture_volume(element, 0, &vol);
      snd_mixer_selem_set_capture_volume_all(element, 
         min + alsa_volume_calc(cmd+14, vol) * (max-min));
    }

  }
  else if(!g_ascii_strncasecmp(cmd,"capture-mute",12))
  {
    if(snd_mixer_selem_has_capture_switch(element))
    {
      cmd+=12;
      while(*cmd==' ')
        cmd++;
      if(!g_ascii_strcasecmp(cmd,"on"))
        snd_mixer_selem_set_capture_switch_all(element,0);
      if(!g_ascii_strcasecmp(cmd,"off"))
        snd_mixer_selem_set_capture_switch_all(element,1);
      if(!g_ascii_strcasecmp(cmd,"toggle"))
      {
        snd_mixer_selem_get_capture_switch(element, 0, &pb);
        snd_mixer_selem_set_capture_switch_all(element,!pb);
      }
    }
  }
  else
    return;

  MODULE_TRIGGER_EMIT("alsa");
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

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  NULL
};
