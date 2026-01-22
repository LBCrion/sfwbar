/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "basewidget.h"
#include "entry.h"
#include "vm/vm.h"

G_DEFINE_TYPE_WITH_CODE (Entry, entry, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Entry))

static value_t entry_entry_text ( vm_t *vm, value_t p[], gint np )
{
  EntryPrivate *priv;
  GtkWidget *widget;

  widget = vm_widget_get(vm, np? value_get_string(p[0]) : NULL);

  if(!IS_ENTRY(widget) || !(priv = entry_get_instance_private(ENTRY(widget))))
    return value_na;

  return value_new_string(g_strdup(
        gtk_entry_get_text(GTK_ENTRY(priv->entry))));
}

static void entry_class_init ( EntryClass *kclass )
{
  vm_func_add("entrytext", entry_entry_text, FALSE, FALSE);
}

static void entry_hierarchy_cb (GtkWidget *self, GtkWidget *old, gpointer d)
{
  GtkWidget *parent;
  if( !(parent = gtk_widget_get_ancestor(self, GTK_TYPE_WINDOW)) )
    return;

  if(gtk_window_get_window_type(GTK_WINDOW(parent))==GTK_WINDOW_POPUP)
    g_warning("Entry in a Popup window will not consistently receive input");
}

static void entry_init ( Entry *self )
{
  EntryPrivate *priv;

  priv = entry_get_instance_private(ENTRY(self));

  priv->entry = gtk_entry_new();
  gtk_container_add(GTK_CONTAINER(self), priv->entry);
  g_signal_connect(G_OBJECT(self), "hierarchy-changed",
      G_CALLBACK(entry_hierarchy_cb), NULL);
}
