/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "css.h"
#include "scanner.h"
#include "window.h"
#include "gui/basewidget.h"
#include "util/file.h"
#include "util/string.h"

static void (*css_style_updated_original)(GtkWidget *);

void css_file_load ( gchar *name )
{
  GtkCssProvider *css;
  gchar *fname, *css_input, *css_string;

  if(!name)
    return;

  fname = get_xdg_config_file(name, NULL);
  if(!fname)
    return;
  if(g_file_get_contents(fname, &css_input, NULL, NULL))
  {
    css_string = css_legacy_preprocess(css_input, fname);
    g_free(css_input);
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, css_string, strlen(css_string), NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
    g_free(css_string);
  }
  g_free(fname);
}

static void css_custom_handle ( GtkWidget *widget )
{
  gboolean state;
  gdouble xalign;
  GtkAlign align;
  guint x;

  gtk_widget_style_get(widget, "visible", &state, NULL);
  if(state)
    gtk_widget_show(widget);
  else
  {
    if(GTK_IS_WINDOW(widget))
      window_collapse_popups(widget);
    gtk_widget_hide(widget);
  }
  if(!GTK_IS_EVENT_BOX(widget))
  {
    gtk_widget_style_get(widget, "hexpand", &state, NULL);
    gtk_widget_set_hexpand(widget, state);
    gtk_widget_style_get(widget, "vexpand", &state, NULL);
    gtk_widget_set_vexpand(widget, state);
    gtk_widget_style_get(widget, "halign", &align, NULL);
    gtk_widget_set_halign(widget, align);
    gtk_widget_style_get(widget, "valign", &align, NULL);
    gtk_widget_set_valign(widget, align);
  }
  if(GTK_IS_LABEL(widget))
  {
    gtk_widget_style_get(widget, "align", &xalign, NULL);
    gtk_label_set_xalign(GTK_LABEL(widget), xalign);
    gtk_widget_style_get(widget, "ellipsize", &state, NULL);
    gtk_label_set_ellipsize(GTK_LABEL(widget),
        state?PANGO_ELLIPSIZE_END:PANGO_ELLIPSIZE_NONE);

    /* don't set wrap property until size is allocated */
    if(gtk_widget_get_allocated_width(widget)>1)
    {
      gtk_widget_style_get(widget, "wrap", &state, NULL);
      gtk_label_set_line_wrap(GTK_LABEL(widget), state);
    }
  }
  if(IS_BASE_WIDGET(widget))
  {
    gtk_widget_style_get(base_widget_get_child(widget), "max-width", &x, NULL);
    base_widget_set_max_width(widget, x);
    gtk_widget_style_get(base_widget_get_child(widget), "max-height", &x,NULL);
    base_widget_set_max_height(widget, x);
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

  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_double("align","text alignment","text alignment",
      0.0,1.0,0.5, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_boolean("ellipsize","ellipsize text","ellipsize text",
      TRUE, G_PARAM_READABLE));

  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_boolean("hexpand","horizonal expansion","horizontal expansion",
       FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_boolean("vexpand","vertical expansion","vertical expansion",
      FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_boolean("wrap","wap label text","wrap label text",
       FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_boolean("visible","show/hide a widget","show/hide a widget",
      TRUE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_int("icon-size","icon size","icon size",
      0,500,48, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_uint("max-width","maximum width","maximum width",
      0,G_MAXUINT,0, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
    g_param_spec_uint("max-height","maximum height","maximum height",
      0,G_MAXUINT,0, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
      g_param_spec_boolean("row-homogeneous","row homogeneous",
        "make all rows within the grid equal height", TRUE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property(widget_class,
      g_param_spec_boolean("column-homogeneous","column homogeneous",
        "make all columns within the grid equal width", TRUE,
        G_PARAM_READABLE));

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
    {GTK_ALIGN_BASELINE,"baseline","baseline"},
    {0, NULL, NULL}};
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("halign","horizontal alignment","horizontal alignment",
      g_enum_register_static ("halign",align_types),
      GTK_ALIGN_FILL, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("valign","vertical alignment","vertical alignment",
      g_enum_register_static ("valign",align_types),
      GTK_ALIGN_FILL, G_PARAM_READABLE));
  static GEnumValue anchor_types [] = {
    {GDK_GRAVITY_NORTH_WEST,"Northwest","Northwest"},
    {GDK_GRAVITY_NORTH,"North","North"},
    {GDK_GRAVITY_NORTH_EAST,"Northeast","Northeast"},
    {GDK_GRAVITY_WEST,"West","West"},
    {GDK_GRAVITY_CENTER,"Center","Center"},
    {GDK_GRAVITY_EAST,"East","East"},
    {GDK_GRAVITY_SOUTH_WEST,"Southwest","Southwest"},
    {GDK_GRAVITY_SOUTH,"South","South"},
    {GDK_GRAVITY_SOUTH_EAST,"Southeast","Southeast"},
    {GDK_GRAVITY_STATIC,"Static","Static"},
    {0, "Default", "Default"},
    {0, NULL, NULL}};
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("widget-anchor","widget anchor","widget anchor for popup windows",
      g_enum_register_static ("widget-anchor",anchor_types),
      0, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("window-anchor","window anchor","window anchor for popup windows",
      g_enum_register_static ("window-anchor",anchor_types),
      0, G_PARAM_READABLE));

  css_style_updated_original = widget_class->style_updated;
  widget_class->style_updated = css_style_updated;

  css_str =
    "window { -GtkWidget-direction: bottom; } " \
    ".sensor { min-width: 1px; min-height: 1px; background: none; }" \
    ".hidden { -GtkWidget-visible: false; }"\
    "#hidden { -GtkWidget-visible: false; }"\
    ".flowgrid {-GtkWidget-row-homogeneous: true; }"\
    ".flowgrid {-GtkWidget-column-homogeneous: true; }";
  css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,css_str,strlen(css_str),NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(css);

  css_file_load(cssname);
}

GtkCssProvider *css_widget_apply ( GtkWidget *widget, gchar *css )
{
  GtkStyleContext *cont;
  GtkCssProvider *provider;

  if(!css)
    return NULL;

  cont = gtk_widget_get_style_context (widget);
  provider = gtk_css_provider_new();
  css = css_legacy_preprocess(css, NULL);
  gtk_css_provider_load_from_data(provider, css, strlen(css), NULL);
  gtk_style_context_add_provider (cont,
    GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(provider);
  g_free(css);

  return provider;
}

void css_widget_cascade ( GtkWidget *widget, gpointer data )
{
  css_custom_handle(widget);

  if(GTK_IS_CONTAINER(widget))
    gtk_container_forall(GTK_CONTAINER(widget), css_widget_cascade, NULL);
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

void css_set_class ( GtkWidget *widget, gchar *css_class, gboolean state )
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context(widget);
  if(state)
    gtk_style_context_add_class(context,css_class);
  else
    gtk_style_context_remove_class(context,css_class);
}

gchar *css_legacy_preprocess ( gchar *css_string, gchar *fname )
{
  gchar *old[] = { "#taskbar_normal", "#taskbar_active",
    "#taskbar_popup_normal", "#taskbar_popup_active", "#taskbar_group_normal",
    "#taskbar_group_active", "#taskbar_pager_normal", "#taskbar_pager_active",
    "#pager_normal", "#pager_visible", "#pager_focused", "#switcher_normal",
    "#switcher_active", "#tray_active", "#tray_attention", "#tray_passive",
    NULL };
  gchar *new[] = { "#taskbar_item", "#taskbar_item.focused", "#taskbar_popup",
    "#taskbar_popup.focused", "#taskbar_popup", "#taskbar_popup.focused",
    "#taskbar_pager", "#taskbar_pager.focused", "#pager_item",
    "#pager_item.visible", "#pager_item.focused", "#switcher_item",
    "#switcher_item.focused", "#tray_item", "#tray_item.urgent",
    "#tray_item.passive", NULL };
  gchar *tmp, *res, *fpath;
  value_t thickness;
  gsize i;

  res = g_strdup(css_string);
  for(i=0; old[i]; i++)
  {
    tmp = str_replace(res, old[i], new[i]);
    g_free(res);
    res = tmp;
  }
  if(fname)
  {
    fpath = g_path_get_dirname(fname);
    tmp = str_replace(res, "@css_file_path", fpath);
    g_free(fpath);
    g_free(res);
    res = tmp;
  }
  thickness = scanner_get_value(g_quark_from_static_string("thicknesshint"),
      SCANNER_TYPE_STR, TRUE, NULL);
  tmp = str_replace(res, "@bar_thickness", value_is_string(thickness)?
      value_get_string(thickness) : "20px");
  g_free(res);
  res = tmp;
  value_free(thickness);

  return res;
}
