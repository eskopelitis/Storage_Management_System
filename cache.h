#ifndef CACHE_H_
#define CACHE_H_

#include <stdbool.h>
#include <stdint.h>

#include "jbod.h"
#include "util.h"

typedef struct {
  bool valid;
  int disk_num;
  int block_num;
  uint8_t block[JBOD_BLOCK_SIZE];
  int clock_accesses;
} cache_entry_t;

/* Returns 1 on success and -1 on failure. Should allocate a space for
 * |num_entries| cache entries, each of type cache_entry_t. Calling it again
 * without first calling cache_destroy (see below) should fail. */
int cache_create(int num_entries);

/* Returns 1 on success and -1 on failure. Frees the space allocated by
 * cache_create function above. */
int cache_destroy(void);

/* Returns 1 on success and -1 on failure. Looks up the block located at
 * |disk_num| and |block_num| in cache and if found, copies the corresponding
 * block to |buf|, which must not be NULL. */
int cache_lookup(int disk_num, int block_num, uint8_t *buf);

/* Returns 1 on success and -1 on failure. Inserts an entry for |disk_num| and
 * |block_num| into cache. Returns -1 if there is already an existing entry in the cache
 * with |disk_num| and |block_num|.If there cache is full, should evict most
 * recently used entry and insert the new entry. */
int cache_insert(int disk_num, int block_num, const uint8_t *buf);

/* If the entry with |disk_num| and |block_num| exists, updates the
 * corresponding block with data from |buf| */
void cache_update(int disk_num, int block_num, const uint8_t *buf);

/* Returns true if cache is enabled and false if not. */
bool cache_enabled(void);

/* Prints the hit rate of the cache. */
void cache_print_hit_rate(void);

/* Resizes the cache to |new_size| entries. If |new_size| is smaller than the
 * current size, evicts the most recently used entries. If |new_size| is
 * larger than the current size, allocates new entries and initializes them to
 * invalid. */
int cache_resize(int new_size);

#endif
