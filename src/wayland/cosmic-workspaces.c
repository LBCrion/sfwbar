/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "../workspace.h"
#include "cosmic-workspace-unstable-v1.h"

#define COSMIC_WORKSPACE_VERSION 1

static struct zcosmic_workspace_manager_v1 *workspace_manager;
static struct wl_list cosmic_workspaces;
static GList *workspace_groups;

struct cosmic_workspace {
  char *name;
  gboolean hidden;
  gboolean urgent;
  gboolean focused;
  gboolean activation_supported;
  struct zcosmic_workspace_handle_v1 *wl_ws;
  workspace_t *ws;
  struct wl_list link;
};

/* API */
static void api_set_workspace(workspace_t *ws)
{
  struct cosmic_workspace *cw = ws->custom_data;

  if(!workspace_manager)
    return;

  if(!cw)
  {
    if(!ws->name)
      g_warning("Workspace is orphaned");
    else if(!workspace_groups)
      g_warning("Workspace creation is not supported by compositor");
    else
      zcosmic_workspace_group_handle_v1_create_workspace(
          workspace_groups->data, ws->name);
  }
  else if(cw->activation_supported)
  {
    g_debug("Activating workspace %s", cw->name);
    zcosmic_workspace_handle_v1_activate(cw->wl_ws);
    zcosmic_workspace_manager_v1_commit(workspace_manager);
  }
  else
    g_warning("Workspace activation not supported by compositor");
}

static struct workspace_api api_impl = {
  .set_workspace = api_set_workspace,
  .get_geom = NULL,
};

/* Workspace */
static void workspace_handle_name(void *data,
    struct zcosmic_workspace_handle_v1 *workspace, const char *name)
{
  struct cosmic_workspace *cw = data;

  g_free(cw->name);
  cw->name = g_strdup(name);
}

static void workspace_handle_coordinates(void *data,
    struct zcosmic_workspace_handle_v1 *workspace,
    struct wl_array *coordinates)
{
}

static void workspace_handle_state(void *data,
    struct zcosmic_workspace_handle_v1 *workspace, struct wl_array *state)
{
  struct cosmic_workspace *cw = data;
  uint32_t *entry;

  cw->focused = FALSE;
  cw->hidden = FALSE;
  cw->urgent = FALSE;

  wl_array_for_each(entry, state)
  {
    switch (*entry)
    {
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_ACTIVE:
        cw->focused = TRUE;
        break;
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_URGENT:
        cw->urgent = TRUE;
        break;
      case ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_HIDDEN:
        cw->hidden = TRUE;
        break;
      default:
        g_info("Unsupported cosmic-workspace state: %u", *entry);
    }
  }

  g_debug("Workspace %s is now focused: %s (%s)",
    cw->name, cw->focused ? "yes" : "no", cw->hidden ? "hidden" : "visible");
}

static void workspace_handle_capabilities(void *data,
    struct zcosmic_workspace_handle_v1 *workspace,
    struct wl_array *capabilities)
{
  struct cosmic_workspace *ws = data;
  uint32_t *entry;

  wl_array_for_each(entry, capabilities)
  {
    if(*entry ==
        ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_ACTIVATE)
      ws->activation_supported = TRUE;
  }
}

static void workspace_handle_remove(void *data,
		struct zcosmic_workspace_handle_v1 *workspace)
{
  struct cosmic_workspace *cw = data;

  zcosmic_workspace_handle_v1_destroy(workspace);

  if (cw->ws)
  {
    g_debug("Workspace %s destroyed", cw->name);
    cw->ws->custom_data = NULL;
    workspace_unref(cw->ws->id);
  }
  wl_list_remove(&cw->link);
  g_free(cw->name);
  g_free(cw);
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
    if(*entry == ZCOSMIC_WORKSPACE_GROUP_HANDLE_V1_CREATE_WORKSPACE)
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
  struct cosmic_workspace *cw = g_malloc0(sizeof(*cw));

  cw->name = NULL;
  cw->hidden = TRUE;
  cw->urgent = FALSE;
  cw->focused = FALSE;
  cw->ws = NULL;
  cw->wl_ws = workspace;
  cw->activation_supported = FALSE;
  wl_list_insert(cosmic_workspaces.prev, &cw->link);

  zcosmic_workspace_handle_v1_add_listener(workspace, &workspace_impl, cw);
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

/* Helpers */
static workspace_t *create_sfwbar_ws(struct cosmic_workspace *workspace)
{
  uint32_t obj_id = wl_proxy_get_id((struct wl_proxy *)workspace->wl_ws);
  workspace_t ws;

  ws.id = GINT_TO_POINTER(obj_id);
  ws.custom_data = workspace;
  ws.name = workspace->name;
  ws.focused = workspace->focused;
  ws.visible = !workspace->hidden;

  workspace_new(&ws);
  return workspace_from_id(ws.id);
}

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
  struct cosmic_workspace *cw;

  wl_list_for_each(cw, &cosmic_workspaces, link)
  {
    if(!cw->ws)
    {
      cw->ws = create_sfwbar_ws(cw);
      g_debug("Workspace created: %s", cw->name);
    }
    if(cw->focused)
      workspace_set_focus(cw->ws->id);
    if (cw->ws->visible == cw->hidden)
      g_info("Should notify about visibility change for %s, now %s",
          cw->name, cw->hidden ? "hidden" : "visible");
    if (g_strcmp0(cw->ws->name, cw->name))
    {
      g_debug("Workspace name change to %s", cw->name);
      workspace_set_name(cw->ws, cw->name);
    }
  }
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
    g_info("Not using cosmic-workspaces: custom IPC priority");
    return;
  }

  wl_list_init(&cosmic_workspaces);
  workspace_api_register(&api_impl);

  workspace_manager = wl_registry_bind(registry, global,
      &zcosmic_workspace_manager_v1_interface,
      MIN(version, COSMIC_WORKSPACE_VERSION));

  zcosmic_workspace_manager_v1_add_listener(workspace_manager,
      &workspace_manager_impl, NULL);
}
