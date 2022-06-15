#ifndef __SNIITEM_H__
#define __SNIITEM_H__

#include "sfwbar.h" 

#define SNI_ITEM_TYPE            (sni_item_get_type())
#define SNI_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SNI_ITEM_TYPE, SniItem))
#define SNI_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SNI_ITEM_TYPE, SniItemClass))
#define IS_SNI_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SNI_ITEM_TYPE))
#define IS_SNI_ITEMCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SNI_ITEM_TYPE))

typedef struct _SniItem SniItem;
typedef struct _SniItemClass SniItemClass;

struct _SniItem
{
  FlowItem item;
};

struct _SniItemClass
{
  FlowItemClass parent_class;
};

typedef struct _SniItemPrivate SniItemPrivate;

struct _SniItemPrivate
{
  sni_item_t *sni;
  GtkWidget *icon;
};

GType sni_item_get_type ( void );

GtkWidget *sni_item_new( sni_item_t *win, GtkWidget *tray );
sni_item_t*sni_item_get_sni ( GtkWidget *self );
void sni_item_update ( GtkWidget *self );
gint sni_item_compare ( GtkWidget *a, GtkWidget *b );

#endif
