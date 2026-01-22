#ifndef __ENTRY_H__
#define __ENTRY_H__

#include "basewidget.h"

#define ENTRY_TYPE            (entry_get_type())
#define ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), ENTRY_TYPE, Entry))
#define ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ENTRY_TYPE, EntryClass))
#define IS_ENTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ENTRY_TYPE))
#define IS_ENTRYCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ENTRY_TYPE))

typedef struct _Entry Entry;
typedef struct _EntryClass EntryClass;

struct _Entry
{
  BaseWidget item;
};

struct _EntryClass
{
  BaseWidgetClass parent_class;
};

typedef struct _EntryPrivate EntryPrivate;

struct _EntryPrivate
{
  GtkWidget *entry;
};

GType entry_get_type ( void );

#endif
