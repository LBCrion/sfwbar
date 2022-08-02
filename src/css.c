/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"

void css_file_load ( gchar *name )
{
  GtkCssProvider *css;
  gchar *fname;

  if(!name)
    return;

  fname = get_xdg_config_file(name,NULL);
  if(!fname)
    return;

  css = gtk_css_provider_new();
  gtk_css_provider_load_from_path(css,fname,NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(css);
  g_free(fname);
}

void css_init ( gchar *cssname )
{
  gchar *css_str;
  GtkCssProvider *css;
  GtkWidgetClass *widget_class = g_type_class_ref(GTK_TYPE_WIDGET);

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_double("align","text alignment","text alignment",
      0.0,1.0,0.5, G_PARAM_READABLE));

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("hexpand","horizonal expansion","horizontal expansion",
       FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("vexpand","vertical expansion","vertical expansion",
      FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("visible","show/hide a widget","show/hide a widget",
      TRUE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_int("icon-size","icon size","icon size",
      0,500,48, G_PARAM_READABLE));

  static GEnumValue dir_types [] = {
    {GTK_POS_TOP,"top","top"},
    {GTK_POS_BOTTOM,"bottom","bottom"},
    {GTK_POS_LEFT,"left","left"},
    {GTK_POS_RIGHT,"right","right"},
    {0,NULL,NULL}};
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("direction","direction","direction",
      g_enum_register_static ("direction",dir_types),
      GTK_POS_RIGHT, G_PARAM_READABLE));

  css_str = "window { -GtkWidget-direction: bottom; }";
  css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,css_str,strlen(css_str),NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(css);

  css_file_load(cssname);
}

void css_widget_apply ( GtkWidget *widget, gchar *css )
{
  GtkStyleContext *cont;
  GtkCssProvider *provider;

  if(!css)
    return;

  cont = gtk_widget_get_style_context (widget);
  provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
  gtk_style_context_add_provider (cont,
    GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(provider);
  g_free(css);
}

void css_widget_cascade ( GtkWidget *widget, gpointer data )
{
  gboolean state;
  gdouble xalign;

  gtk_widget_style_get(widget,"visible",&state,NULL);
  gtk_widget_set_visible(widget,state);
  if(!GTK_IS_EVENT_BOX(widget))
  {
    gtk_widget_style_get(widget,"hexpand",&state,NULL);
    gtk_widget_set_hexpand(widget,state);
    gtk_widget_style_get(widget,"vexpand",&state,NULL);
    gtk_widget_set_vexpand(widget,state);
  }

  if(GTK_IS_LABEL(widget))
  {
    gtk_widget_style_get(widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(widget),xalign);
  }

  if(GTK_IS_CONTAINER(widget))
    gtk_container_forall(GTK_CONTAINER(widget),css_widget_cascade,NULL);
}

