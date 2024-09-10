#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
  char* key;
  void* data;
} hm_entry_t;

typedef struct {
  hm_entry_t* entries;
  size_t collisions;
} hm_table_t;

typedef struct {
  size_t size;
  size_t max_collisions;
  hm_table_t* table;
} hashmap_t;

hashmap_t* hashmap_create(size_t size, size_t max_collisions);
uint64_t hashmap_hash(const char* key);
void hashmap_add(hashmap_t* hm, char* key, void* data);
void* hashmap_get(hashmap_t* hm, char* key);
void hashmap_resize(hashmap_t* hm, uint64_t new_size);
void hashmap_import(hashmap_t* to, hashmap_t* from);
void hashmap_destroy(hashmap_t* hm);