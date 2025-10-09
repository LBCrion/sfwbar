#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include <glib.h>

typedef struct _hash_table {
  GHashTable *table;
  GRecMutex mutex;
} hash_table_t;

hash_table_t *hash_table_new(GHashFunc hash, GEqualFunc equal );
hash_table_t *hash_table_new_full(GHashFunc hash, GEqualFunc equal,
    GDestroyNotify destroy_key, GDestroyNotify destroy_value);
gboolean hash_table_insert ( hash_table_t *hash, gpointer key, gpointer val );
gpointer hash_table_lookup ( hash_table_t *hash, gpointer key );
gboolean hash_table_remove ( hash_table_t *hash, gpointer key );
void hash_table_remove_all ( hash_table_t *hash );
gpointer hash_table_find ( hash_table_t *hash, GHRFunc pred, gpointer key);
void hash_table_lock( hash_table_t *hash );
void hash_table_unlock( hash_table_t *hash );

#endif
