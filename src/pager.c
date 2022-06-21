/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include "sfwbar.h"
#include "pageritem.h"

static GList *pagers;
static GList *global_pins;
static workspace_t *focus;
static GList *workspaces;

void pager_add_pin ( GtkWidget *pager, gchar *pin )
{
  global_pins = g_list_append(global_pins,pin);
  if(!g_list_find_custom(g_object_get_data(G_OBJECT(pager),"pins"),pin,
        (GCompareFunc)g_strcmp0))
    g_object_set_data(G_OBJECT(pager),"pins",
        g_list_append(g_object_get_data(G_OBJECT(pager),"pins"),pin));
}

gint pager_comp_id ( workspace_t *a, gpointer id )
{
  return a->id - GPOINTER_TO_INT(id);
}

gint pager_comp_name ( workspace_t *a, gchar * b)
{
  return g_strcmp0(a->name,b);
}

gboolean workspace_is_focused ( workspace_t *ws )
{
  return (ws == focus);
}

void workspace_delete ( GList *item )
{
  GList *iter;
  workspace_t *ws;

  if(!item)
    return;

  ws = item->data;

  for(iter=global_pins;iter;iter=g_list_next(iter))
    if(!g_strcmp0(iter->data,ws->name))
    {
      ws->id = -1;
      ws->visible = FALSE;
      for(iter=pagers;iter;iter=g_list_next(iter))
        if(!g_list_find_custom(g_object_get_data(G_OBJECT(iter->data),"pins"),
              ws->name,(GCompareFunc)g_strcmp0))
            flow_grid_delete_child(iter->data,ws);
      return;
    }

  g_list_foreach(pagers,(GFunc)flow_grid_delete_child,ws);
  g_free(ws->name);
  g_free(ws);
  workspaces = g_list_delete_link(workspaces,item);
}

void workspace_new ( json_object *obj )
{
  gchar *name;
  gint id;
  GList *item;
  workspace_t *ws;

  id = json_int_by_name(obj,"id",0);

  item = g_list_find_custom(workspaces,GINT_TO_POINTER(id),
      (GCompareFunc)pager_comp_id);
  if(item)
  {
    ws = item->data;
    g_free(ws->name);
    ws->name = json_string_by_name(obj,"name");
    g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
  }
  else
  {
    name = json_string_by_name(obj,"name");
    item = g_list_find_custom(workspaces,name,(GCompareFunc)pager_comp_name);
    if(item)
    {
      ws = item->data;
      g_free(name);
    }
    else
    {
      ws = g_malloc0(sizeof(workspace_t));
      ws->name = name;
      workspaces = g_list_prepend(workspaces,ws);
      g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    }
    ws->id = id;
  }
  ws->visible = json_bool_by_name(obj,"visible",FALSE);
  if(json_bool_by_name(obj,"focused",FALSE))
  {
    g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
    focus = ws;
  }
}

void pager_event ( struct json_object *obj )
{
  gchar *change;
  gint id;
  struct json_object *current;
  GList *item;

  json_object_object_get_ex(obj,"current",&current);
  if(!current)
    return;

  id = json_int_by_name(current,"id",0);
  change = json_string_by_name(obj,"change");

  if(!g_strcmp0(change,"empty"))
    workspace_delete(g_list_find_custom(workspaces,GINT_TO_POINTER(id),
          (GCompareFunc)pager_comp_id));
  else
    workspace_new(current);

  if(!g_strcmp0(change,"focus"))
  {
    item = g_list_find_custom(workspaces,GINT_TO_POINTER(id),
        (GCompareFunc)pager_comp_id);
    if(item)
    {
      focus = item->data;
      g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
    }
  }
  
  g_free(change);
  g_list_foreach(pagers,(GFunc)flow_grid_update,NULL);
}


void pager_init ( GtkWidget *widget )
{
  pagers = g_list_prepend(pagers,widget);
}

void pager_populate ( void )
{
  gint sock;
  gint32 etype;
  struct json_object *robj = NULL;
  gint i;
  GList *item;
  workspace_t *ws;
  gchar *response;

  if(!workspaces)
  {
    sock=sway_ipc_open(3000);
    if(sock!=-1)
    {
      sway_ipc_send(sock,1,"");
      response = sway_ipc_poll(sock,&etype);
      close(sock);
      if(response!=NULL)
        robj = json_tokener_parse(response);

      if(robj && json_object_is_type(robj,json_type_array))
      {
        for(i=0;i<json_object_array_length(robj);i++)
          workspace_new(json_object_array_get_idx(robj,i));
        json_object_put(robj);
      }
      g_free(response);
    }
  }
  for(item = global_pins;item;item=g_list_next(item))
    if(!g_list_find_custom(workspaces,item->data,
          (GCompareFunc)pager_comp_name))

    {
      ws = g_malloc0(sizeof(workspace_t));
      ws->id = -1;
      ws->name = g_strdup(item->data);
      ws->visible = FALSE;
      workspaces = g_list_prepend(workspaces,ws);
      g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    }

  g_list_foreach(pagers,(GFunc)flow_grid_update,NULL);
}

