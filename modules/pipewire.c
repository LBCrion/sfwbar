
#include "module.h"
#include "trigger.h"
#include "vm/vm.h"
#include <pipewire/pipewire.h>

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

static struct pw_thread_loop *loop;
static struct pw_context *ctx;
static struct pw_core *core;
static struct pw_registry *reg;
static struct spa_hook listener;
static gint audio_in, video_in, audio_out;
static GData *proxy_map;

typedef struct {
  struct spa_hook listener, obj_listener;
  struct pw_proxy *proxy;
  gint *counter;
  guint32 id;
} pw_node_t;

static void node_info_handle ( void *data, const struct pw_node_info *info )
{
//  pw_node_t *node = data;


}

static const struct pw_node_events node_events = {
  .version = PW_VERSION_NODE_EVENTS,
  .info = node_info_handle,
};

static void node_destroy ( void *data )
{
  pw_node_t *node = data;

  spa_hook_remove(&node->listener);
  spa_hook_remove(&node->obj_listener);
  *(node->counter) = *(node->counter) - 1;
  g_debug("pipewire: removed node id%u", node->id);
  trigger_emit("pipewire");
}

static const struct pw_proxy_events proxy_events = {
  .version = PW_VERSION_PROXY_EVENTS,
  .destroy = node_destroy,
};

static void pw_global_handle ( void *d, uint32_t id, uint32_t perms,
    const char *type, uint32_t version, const struct spa_dict *props)
{
  struct pw_proxy *proxy;
  const char *mclass;
  gint *counter;
  pw_node_t *node;

  if(g_datalist_id_get_data(&proxy_map, (GQuark)id))
    return;
 
  mclass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  g_debug("pipewire: class '%s'", mclass);
  if(!g_strcmp0(mclass, "Stream/Output/Audio"))
    counter = &audio_out;
  else if(!g_strcmp0(mclass, "Stream/Input/Audio"))
    counter = &audio_in;
  else if(!g_strcmp0(mclass, "Stream/Input/Video"))
    counter = &video_in;
  else
    return;

  if( !(proxy = pw_registry_bind(reg, id, type, version, sizeof(pw_node_t))) )
    return;

  *counter = *counter + 1;

  node = (pw_node_t *)pw_proxy_get_user_data(proxy);
  pw_proxy_add_listener(proxy, &node->listener, &proxy_events, node);
  pw_proxy_add_object_listener(proxy, &node->obj_listener, &node_events, node);
  node->id = id;
  node->proxy = proxy;
  node->counter = counter;
  g_datalist_id_set_data(&proxy_map, (GQuark)id, node);
  trigger_emit("pipewire");
  g_debug("pipewire: new node id%u type:%s/%d %s", id, type, version, mclass);
}

static void pw_global_remove_handle ( void *d, uint32_t id )
{
  pw_node_t *node;

  if( !(node = g_datalist_id_get_data(&proxy_map, (GQuark)id)) )
    return;

  g_datalist_id_remove_data(&proxy_map, (GQuark)id);
  pw_proxy_destroy(node->proxy);
}

static const struct pw_registry_events reg_events = {
  .version = PW_VERSION_REGISTRY_EVENTS,
  .global = pw_global_handle,
  .global_remove = pw_global_remove_handle
};

static const char *pw_connect ( void )
{
  if( !(loop = pw_thread_loop_new("test", NULL)) )
    return "unable to create a thread loop";
  pw_thread_loop_lock(loop);
  if( !(ctx = pw_context_new(pw_thread_loop_get_loop(loop), NULL, 0)) )
    return "unable to create a context";
  if( !(core = pw_context_connect(ctx, NULL, 0)) )
    return "unable to connect to a context";
  if( !(reg = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0)) )
    return "unable to get a registry";
  spa_zero(listener);
  pw_registry_add_listener(reg, &listener, &reg_events, NULL);

  return NULL;
}

static value_t pw_count_func ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "PipewireCount");
  vm_param_check_string(vm, p, 0, "PipewireCount");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "AudioIn"))
    return value_new_numeric(audio_in);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "VideoIn"))
    return value_new_numeric(video_in);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "AudioOut"))
    return value_new_numeric(audio_out);

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  const gchar *err;

  pw_init(NULL, 0); 
  if( (err = pw_connect()) )
  {
    g_debug("pipewire: %s\n", err);
    return FALSE;
  }

  g_datalist_init(&proxy_map);
  vm_func_add("PipewireCount", pw_count_func, FALSE, TRUE);
  pw_thread_loop_unlock(loop);
  pw_thread_loop_start(loop);

  return TRUE;
}
