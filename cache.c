#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functions.

static cache_entry_t *cache = NULL;
static int cache_space;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  //cache exists
  if (cache != NULL) {
    return -1; 
  }
  if (num_entries < 2 || num_entries > 4096) {
    return -1; // cache size not in bounds
  }

  cache = calloc(num_entries, sizeof(cache_entry_t));
  if (!cache) {
    return -1; // Failed to allocate memory for the cache.
  }
  cache_size = num_entries;
  num_entries = 0;

  return 1; // Success.
}
int cache_destroy(void) {
  //returns -1 if nothing to destroy
  if (cache == NULL) {
    return -1;
  }
  free(cache);
  cache=NULL;
  cache_size = 0;
  cache_space = 0; //reset
  return 0;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //base case for buffer
  if (!buf) {
    return -1;
  }
  //increases total number of queries
  num_queries+=1;
  for (int i = 0; i < cache_size; i++) {
    if (cache[i].valid && cache[i].disk_num == disk_num &&cache[i].block_num == block_num) {
      //updates the clock for entry
      cache[i].clock_accesses = clock+=1;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      //counts number of hits
      num_hits+=1;
      return 1;
    }
  }
  //returns -1 when cache miss happens
  return -1; 
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  //nothing to update if buffer is NULL
   if (buf == NULL) {
    return;
  }
  for (int i = 0; i < cache_size; i++) {
    //checks if everything is valid to continue
    if (!cache[i].valid) { continue; }
    if (cache[i].disk_num != disk_num) { continue; }
    if (cache[i].block_num != block_num) { continue; }
    cache[i].clock_accesses = clock++;
    //copy to cache
    memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
    
    return;
  }
}


int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //check for buf and if disk and block nums are in bounds
  if (!buf ||
    !(disk_num >= 0 && disk_num < JBOD_NUM_DISKS) || 
    !(block_num >= 0 && block_num < JBOD_NUM_BLOCKS_PER_DISK) ||
    !cache_enabled()) {
    return -1;
  }

  int j = -1;
  //makes sure cache isn't full
  if (cache_space >= cache_size) {
    //finds most recent entry
    for(int i = 0; i < cache_size; i++) {
      if(cache[i].valid && cache[i].clock_accesses > cache[j].clock_accesses) {
        j = i;
      }
    }
  }
  //uses next avalaible space in cache
  else {
    j = cache_space;
  }
  //returns error if entry exists
  uint8_t buffer[JBOD_BLOCK_SIZE];
  if (cache_lookup(disk_num, block_num, buffer) == 1) {return -1;}
  memcpy(cache[j].block, buf, JBOD_BLOCK_SIZE);
  cache[j].valid = 1;
  cache[j].clock_accesses = clock+=1;

  cache[j].block_num = block_num;
  cache[j].disk_num = disk_num;
  //keeps track of amound of entries in cache
  if (cache_space < cache_size) {
    cache_space+=1;
  }

  return 1;
}
bool cache_enabled(void) {
  return cache != NULL;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  //check if cache is NULL 
   if(cache == NULL){
    return -1;
   }

   //try to resize cache
   cache_entry_t *new_cache = realloc(cache, new_num_entries*sizeof(cache_entry_t));
   if(new_cache == NULL){
    return -1;
   }
   //updates size and cache pointer
   cache = new_cache;
   cache_size = new_num_entries;
   return 0;
}