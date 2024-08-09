/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "../workspace.h"
#include "cosmic-workspace-unstable-v1.h"

#define COSMIC_WORKSPACE_VERSION 1

static struct zcosmic_workspace_manager_v1 *workspace_manager;
static GList *workspace_groups, *workspaces_to_focus;

/* API */
static void api_set_workspace(workspace_t *ws)
{
  if(!workspace_manager)
    return;

  if(!ws->id || ws->id == PAGER_PIN_ID)
  {
    if(!ws->name)
      g_warning("Workspace: cosmic: unnamed pin datected");
    else if(!workspace_groups)
      g_warning("Workspace: cosmic: create is not supported by compositor");
    else
    {
      workspaces_to_focus = g_list_prepend(workspaces_to_focus,
          g_strdup(ws->name));
      zcosmic_workspace_group_handle_v1_create_workspace(
          workspace_groups->data, ws->name);
      zcosmic_workspace_manager_v1_commit(workspace_manager);
    }
  }
  else if(ws->state & WS_CAP_ACTIVATE)
  {
    g_debug("Workspace: cosmic: activating workspace %s", ws->name);
    zcosmic_workspace_handle_v1_activate(ws->id);
    zcosmic_workspace_manager_v1_commit(workspace_manager);
  }
  else
    g_warning("Workspace: cosmic: activation not supported by compositor");
}

static gboolean cosmic_workspace_get_can_create ( void )
{
  return !!workspace_groups;
}

static struct workspace_api api_impl = {
  .set_workspace = api_set_workspace,
  .get_geom = NULL,
  .get_can_create = cosmic_workspace_get_can_create,
};

/* Workspace */
static void workspace_handle_name(void *data,
    struct zcosmic_workspace_handle_v1 *workspace, const char *name)
{
  workspace_t *ws = data;
  GList *item;

  workspace_set_name(ws, name);

  if( !(item=g_list_find_custom(workspaces_to_focus, name,
          (GCompareFunc)g_strcmp0)) )
    return;

  g_free(item->data);
  workspaces_to_focus = g_list_remove(workspaces_to_focus, item);
  workspace_activate(ws);
}

static void workspace_handle_coordinates(void *data,
    struct zcosmic_workspace_handle_v1 *workspace,
    struct wl_array *coordinates)
{
}

static void workspace_handle_state(void *data,
    struct zcosmic_workspace_handle_v1 *workspace, struct wl_array *state)
{
  uint32_t *entry, wsstate = WS_STATE_VISIBLE;

  wl_array_for_each(entry, state)
  {
    switch (*entry)
    {
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_ACTIVE:
        wsstate |= WS_STATE_FOCUSED;
        break;
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_URGENT:
        wsstate |= WS_STATE_URGENT;
        break;
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_HIDDEN:
        wsstate &= ~WS_STATE_VISIBLE;
        break;
      default:
        g_info("Workspace: Unsupported cosmic-workspace state: %u", *entry);
    }
  }
  workspace_set_state(data, wsstate);
}

static void workspace_handle_capabilities(void *data,
    struct zcosmic_workspace_handle_v1 *workspace,
    struct wl_array *capabilities)
{
  uint32_t *entry, wscaps = 0;

  wl_array_for_each(entry, capabilities)
  {
    if(*entry ==
        ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_ACTIVATE)
      wscaps |= WS_CAP_ACTIVATE;
  }
  workspace_set_caps(data, wscaps);
}

static void workspace_handle_remove(void *data,
		struct zcosmic_workspace_handle_v1 *workspace)
{
  workspace_t *ws = data;

  zcosmic_workspace_handle_v1_destroy(workspace);

  if(!ws)
    return;

  g_debug("Workspace: cosmic: workspace '%s' destroyed", ws->name);
  workspace_unref(ws->id);
}

static const struct zcosmic_workspace_handle_v1_listener
    workspace_impl =
{
  .name = workspace_handle_name,
  .coordinates = workspace_handle_coordinates,
  .state = workspace_handle_state,
  .capabilities = workspace_handle_capabilities,
  .remove = workspace_handle_remove,
};

/* Group */
static void workspace_group_handle_capabilities(void *data,
    struct zcosmic_workspace_group_handle_v1 *workspace_group,
    struct wl_array *capabilities)
{
  uint32_t *entry;

  wl_array_for_each(entry, capabilities)
  {
    if(*entry == ZCOSMIC_WORKSPACE_GROUP_HANDLE_V1_ZCOSMIC_WORKSPACE_GROUP_CAPABILITIES_V1_CREATE_WORKSPACE)
      if(!g_list_find(workspace_groups, workspace_group))
        workspace_groups = g_list_prepend(workspace_groups, workspace_group);
  }
}

static void workspace_group_handle_output_enter(void *data,
    struct zcosmic_workspace_group_handle_v1 *workspace_group,
    struct wl_output *output)
{
  /* TODO: maybe implement */
}

static void workspace_group_handle_output_leave(void *data,
    struct zcosmic_workspace_group_handle_v1 *workspace_group,
    struct wl_output *output)
{
  /* TODO: maybe implement */
}

static void workspace_group_handle_workspace(void *data,
    struct zcosmic_workspace_group_handle_v1 *workspace_group,
    struct zcosmic_workspace_handle_v1 *workspace)
{
  workspace_t *ws;

  ws = workspace_new(workspace);
  workspace_set_state(ws, WS_STATE_VISIBLE);

  zcosmic_workspace_handle_v1_add_listener(workspace, &workspace_impl, ws);
}

static void workspace_group_handle_remove(void *data,
    struct zcosmic_workspace_group_handle_v1 *workspace_group)
{
  workspace_groups = g_list_remove(workspace_groups, workspace_group);
  zcosmic_workspace_group_handle_v1_destroy(workspace_group);
}

static const struct zcosmic_workspace_group_handle_v1_listener
	workspace_group_impl =
{
  .capabilities = workspace_group_handle_capabilities,
  .output_enter = workspace_group_handle_output_enter,
  .output_leave = workspace_group_handle_output_leave,
  .workspace = workspace_group_handle_workspace,
  .remove = workspace_group_handle_remove,
};

/* Manager */
static void workspace_manager_handle_workspace_group(void *data,
    struct zcosmic_workspace_manager_v1 *workspace_manager,
    struct zcosmic_workspace_group_handle_v1 *workspace_group)
{
  zcosmic_workspace_group_handle_v1_add_listener(
    workspace_group, &workspace_group_impl, NULL);
}

static void workspace_manager_handle_done(void *data,
    struct zcosmic_workspace_manager_v1 *workspace_manager)
{
  g_list_foreach(workspace_get_list(), (GFunc)workspace_commit, NULL);
}

static void workspace_manager_handle_finished(void *data,
    struct zcosmic_workspace_manager_v1 *manager)
{
  zcosmic_workspace_manager_v1_destroy(manager);
  workspace_manager = NULL;
}

static const struct zcosmic_workspace_manager_v1_listener
  workspace_manager_impl =
{
  .workspace_group = workspace_manager_handle_workspace_group,
  .done = workspace_manager_handle_done,
  .finished = workspace_manager_handle_finished,
};

/* Public API */
void cosmic_workspaces_register(struct wl_registry *registry,
    uint32_t global, uint32_t version)
{
  if (workspaces_supported())
  {
    g_info("Workspace: Not using cosmic-workspaces: custom IPC priority");
    return;
  }

  workspace_api_register(&api_impl);

  workspace_manager = wl_registry_bind(registry, global,
      &zcosmic_workspace_manager_v1_interface,
      MIN(version, COSMIC_WORKSPACE_VERSION));

  zcosmic_workspace_manager_v1_add_listener(workspace_manager,
      &workspace_manager_impl, NULL);
}
