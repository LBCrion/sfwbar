/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "window.h"

void window_ref_free ( GtkWidget *self )
{
  GList **refs;

  refs = g_object_get_data(G_OBJECT(self),"window_refs");

  if(!refs)
    return;

  g_list_free(*refs);
  g_free(refs);
  g_object_set_data(G_OBJECT(self),"window_refs", NULL);
}

void window_ref ( GtkWidget *self, GtkWidget *ref )
{
  GList **refs;

  refs = g_object_get_data(G_OBJECT(self),"window_refs");
  if(!refs)
  {
    g_object_set_data_full(G_OBJECT(self),"window_refs",
        g_malloc0(sizeof(GList *)),(GDestroyNotify)window_ref_free);
    refs = g_object_get_data(G_OBJECT(self),"window_refs");
  }

  if(refs && !g_list_find(*refs, ref))
    *refs = g_list_prepend(*refs ,ref);
  g_signal_connect(G_OBJECT(ref),"unmap",G_CALLBACK(window_unref),self);
}

void window_unref ( GtkWidget *ref, GtkWidget *self )
{
  GList **refs;
  void (*unref_func)( gpointer);

  refs = g_object_get_data(G_OBJECT(self),"window_refs");
  if(refs)
    *refs = g_list_remove(*refs, ref);
  if(!refs && (unref_func = g_object_get_data(G_OBJECT(self), "unref_func")))
    unref_func(self);
}

gboolean window_ref_check ( GtkWidget *self )
{
  GList **refs;

  refs = g_object_get_data(G_OBJECT(self),"window_refs");

  return !!(refs && *refs);
}

void window_set_unref_func ( GtkWidget *self, void (*func)(gpointer) )
{
  g_object_set_data(G_OBJECT(self), "unref_func", func);
}
