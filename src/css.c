/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "bar.h"
#include "window.h"

static void (*css_style_updated_original)(GtkWidget *);

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

static void css_custom_handle ( GtkWidget *widget )
{
  gboolean state;
  gdouble xalign;
  GtkAlign align;
  guint x;

  gtk_widget_style_get(widget,"visible",&state,NULL);
  if(state)
    gtk_widget_show_now(widget);
  else
  {
    if(GTK_IS_WINDOW(widget))
      window_collapse_popups(widget);
    gtk_widget_hide(widget);
  }
  if(!GTK_IS_EVENT_BOX(widget))
  {
    gtk_widget_style_get(widget,"hexpand",&state,NULL);
    gtk_widget_set_hexpand(widget,state);
    gtk_widget_style_get(widget,"vexpand",&state,NULL);
    gtk_widget_set_vexpand(widget,state);
    gtk_widget_style_get(widget,"halign",&align,NULL);
    gtk_widget_set_halign(widget,align);
    gtk_widget_style_get(widget,"valign",&align,NULL);
    gtk_widget_set_valign(widget,align);
  }
  if(IS_BASE_WIDGET(widget))
  {
    gtk_widget_style_get(gtk_bin_get_child(GTK_BIN(widget)),"max-width",&x,
        NULL);
    base_widget_set_max_width(widget,x);
    gtk_widget_style_get(gtk_bin_get_child(GTK_BIN(widget)),"max-height",&x,
        NULL);
    base_widget_set_max_height(widget,x);
  }

  if(GTK_IS_LABEL(widget))
  {
    gtk_widget_style_get(widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(widget),xalign);
    gtk_widget_style_get(widget,"ellipsize",&state,NULL);
    gtk_label_set_ellipsize(GTK_LABEL(widget),
        state?PANGO_ELLIPSIZE_END:PANGO_ELLIPSIZE_NONE);
  }
}

static void css_style_updated ( GtkWidget *widget )
{
  css_style_updated_original(widget);
  css_custom_handle(widget);
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
    g_param_spec_boolean("ellipsize","ellipsize text","ellipsize text",
      TRUE, G_PARAM_READABLE));

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
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_uint("max-width","maximum width","maximum width",
      0,G_MAXUINT,0, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_uint("max-height","maximum height","maximum height",
      0,G_MAXUINT,0, G_PARAM_READABLE));

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
  static GEnumValue align_types [] = {
    {GTK_ALIGN_FILL,"fill","fill"},
    {GTK_ALIGN_START,"start","start"},
    {GTK_ALIGN_END,"end","end"},
    {GTK_ALIGN_CENTER,"center","center"},
    {GTK_ALIGN_BASELINE,"baseline","baseline"}};
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("halign","horizontal alignment","horizontal alignment",
      g_enum_register_static ("halign",align_types),
      GTK_ALIGN_FILL, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("valign","vertical alignment","vertical alignment",
      g_enum_register_static ("valign",align_types),
      GTK_ALIGN_FILL, G_PARAM_READABLE));

  css_style_updated_original = widget_class->style_updated;
  widget_class->style_updated = css_style_updated;

  css_str =
    "window { -GtkWidget-direction: bottom; } " \
    ".sensor { min-width: 1px; min-height: 1px; background: none; }" \
    ".hidden { -GtkWidget-visible: false; }";
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
  css_custom_handle(widget);

  if(GTK_IS_CONTAINER(widget))
    gtk_container_forall(GTK_CONTAINER(widget),css_widget_cascade,NULL);
}

void css_add_class ( GtkWidget *widget, gchar *css_class )
{
  GtkStyleContext *context;
  context = gtk_widget_get_style_context(widget);
  gtk_style_context_add_class(context,css_class);
}

void css_remove_class ( GtkWidget *widget, gchar *css_class )
{
  GtkStyleContext *context;
  context = gtk_widget_get_style_context(widget);
  gtk_style_context_remove_class(context,css_class);
}
