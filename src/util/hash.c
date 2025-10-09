#include "util/hash.h"

hash_table_t *hash_table_new(GHashFunc hash_func, GEqualFunc equal_func )
{
  return hash_table_new_full(hash_func, equal_func, NULL, NULL);
}

hash_table_t *hash_table_new_full(GHashFunc hash_func, GEqualFunc equal_func,
    GDestroyNotify destroy_key, GDestroyNotify destroy_value)
{
  hash_table_t *hash = g_malloc0(sizeof(hash_table_t));
  hash->table = g_hash_table_new_full(hash_func, equal_func, destroy_key,
      destroy_value);
  return hash;
}

gboolean hash_table_insert ( hash_table_t *hash, gpointer key, gpointer val )
{
  gboolean result;

  hash_table_lock(hash);
  result = g_hash_table_insert(hash->table, key, val);
  hash_table_unlock(hash);

  return result;
}

gpointer hash_table_lookup ( hash_table_t *hash, gpointer key )
{
  gpointer value;

  hash_table_lock(hash);
  value = g_hash_table_lookup(hash->table, key);
  hash_table_unlock(hash);

  return value;
}

gboolean hash_table_remove ( hash_table_t *hash, gpointer key )
{
  gboolean result;

  hash_table_lock(hash);
  result = g_hash_table_remove(hash->table, key);
  hash_table_unlock(hash);

  return result;
}

void hash_table_remove_all ( hash_table_t *hash )
{
  hash_table_lock(hash);
  g_hash_table_remove_all(hash->table);
  hash_table_unlock(hash);
  g_free(hash);
}

gpointer hash_table_find ( hash_table_t *hash, GHRFunc pred, gpointer key)
{
  gpointer value;

  hash_table_lock(hash);
  value = g_hash_table_find(hash->table, pred, key);
  hash_table_unlock(hash);

  return value;
}

void hash_table_lock( hash_table_t *hash )
{
  g_rec_mutex_lock(&hash->mutex);
}

void hash_table_unlock( hash_table_t *hash )
{
  g_rec_mutex_unlock(&hash->mutex);
}
