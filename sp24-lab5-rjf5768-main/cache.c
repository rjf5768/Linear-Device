#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cached = 0;//variable used to check if cache is created or not

int cache_create(int num_entries) {
  //check conditions
  if (num_entries < 2 || num_entries > 4096 || cached == 1){//invalid parameters
    return -1;//failed
  }else{
    //initialize cache, create it
    cache = calloc(num_entries, sizeof(cache_entry_t)); 
    //updates variables
    cache_size = num_entries;
    cached = 1;
    //reset values
    num_queries = 0;
    num_hits = 0;
    return 1;//success
  }
  return -1;//failed
}

//initialize to determine current state for later use
int inserted = 0;

int cache_destroy(void) {
  if (cached == 1){
    free(cache);
    //update variables after destroy
    cache = NULL;
    cache_size = 0;
    cached = 0;
    inserted = 0;
    return 1;
  }else{
    return -1;
  }
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (cache_size == 0 || buf == NULL || cache == NULL){ //invalid parameters
    return -1;
  }
  if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255){ //invalid parameters
    return -1;
  }
  // check if its already inserted
  if (inserted == 0){ 
    return -1;
  }
  num_queries+=1;
  //loop through the whole cache
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && buf != NULL){//making sure fits the senario
    //updates variables
      num_hits+=1;
      clock+=1;
      cache[i].access_time = clock;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      return 1;     
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  if (cache_size == 0 || buf == NULL || cache == NULL){ //invalid parameters
    return;
  }
  else if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255){ //invalid parameters
    return;
  }
  //iterate the whole cache
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num ){
      // updates variables
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); 
      clock+=1;
      cache[i].access_time = clock;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //initializes variables for later use
  int cache_Addr = -1;
  int least;
  // check invalid parameters
  if (cached == 0 || buf == NULL || cache_size == 0){
    return -1;
  }
  if (disk_num > 16 || disk_num < 0 || block_num > 256 || block_num < 0){
    return -1;
  }
  inserted = 1;
  for (int i = 0; i < cache_size; i++){
    //check zero calloc contain 0
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && block_num != 0 && disk_num != 0){ 
    return -1;
    }
    if (cache[i].valid == 0){
      cache_Addr = i;
      break;
    }
  }
  // if out of space
  if (cache_Addr == -1){
    least = cache[0].access_time;
    cache_Addr = 0;
    for (int i = 1; i < cache_size; i++){
      if (cache[i].access_time < least){
        least = cache[i].access_time;
        cache_Addr = i;
      }
    }
  }
  // copy buf into the block 
  memcpy(cache[cache_Addr].block, buf, JBOD_BLOCK_SIZE); 
  // update disk number and block number
  cache[cache_Addr].disk_num = disk_num; 
  cache[cache_Addr].block_num = block_num;
  // the block that has data
  cache[cache_Addr].valid = 1; 
  clock+=1;
  cache[cache_Addr].access_time = clock;
  return 1;
}

bool cache_enabled(void) {
  //if there's somethings in cache return true
  return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
