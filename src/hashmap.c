#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

uint64_t hashmap_hash(const char* key) {
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < strlen(key); i++) {
    hash *= 0x100000001b3;
    hash ^= (unsigned char)key[i];
  }
  return hash;
}

hashmap_t* hashmap_create(size_t size, size_t max_collisions) {
  hashmap_t* hm = (hashmap_t*)malloc(sizeof(hashmap_t));
  hm->max_collisions = max_collisions;
  hm->size = size;
  hm->table = (hm_table_t*)malloc(sizeof(hm_table_t) * size);
  memset(hm->table, 0, sizeof(hm_table_t) * size);
  return hm;
}

void hashmap_add(hashmap_t* hm, char* key, void* data) {
start: {
  uint64_t hash = hashmap_hash(key) % (hm->size - 1);
  hm_table_t* table = &hm->table[hash];
  if (table->entries == NULL || table->collisions == 0) {
    table->entries = (hm_entry_t*)malloc(sizeof(hm_entry_t) * hm->max_collisions);
    memset(table->entries, 0, sizeof(hm_entry_t) * hm->max_collisions);
    table->collisions = 0;
  }
  if (table->collisions == hm->max_collisions) {
    hashmap_resize(hm, hm->size + 50, hm->max_collisions + 30);
    goto start;
  }
  uint64_t idx = table->collisions++;
  table->entries[idx].key = (char*)malloc(strlen(key));
  strcpy(table->entries[idx].key, key);
  table->entries[idx].data = data;
}
}

void* hashmap_get(hashmap_t* hm, char* key) {
  uint64_t hash = hashmap_hash(key) % (hm->size - 1);
  hm_table_t* table = &hm->table[hash];
  if (table->entries == NULL || table->collisions == 0) return NULL;

  for (uint64_t i = 0; i < table->collisions; i++) {
    if (!strcmp(key, table->entries[i].key)) return table->entries[i].data;
  }
  return NULL;
}

void hashmap_resize(hashmap_t* hm, uint64_t new_size, uint64_t new_collisions) {
  // Create a new table and re-hash everything into it (and free the old references)
  hm_table_t* new_table = (hm_table_t*)malloc(sizeof(hm_table_t) * new_size);
  memset(new_table, 0, sizeof(hm_table_t) * new_size);
  hm->max_collisions = new_collisions;
  for (uint64_t i = 0; i < hm->size; i++) {
    if (hm->table[i].entries == NULL || hm->table[i].collisions == 0) continue;
    for (uint64_t j = 0; j < hm->table[i].collisions; j++) {
      uint64_t hash = hashmap_hash(hm->table[i].entries[j].key) % (new_size - 1);
      hm_table_t* table = &new_table[hash];
      if (table->collisions == 0) {
        table->entries = (hm_entry_t*)malloc(sizeof(hm_entry_t) * hm->max_collisions);
        memset(table->entries, 0, sizeof(hm_entry_t) * hm->max_collisions);
      }
      uint64_t idx = table->collisions++;
      table->entries[idx].key = hm->table[i].entries[j].key;
      table->entries[idx].data = hm->table[i].entries[j].data;
    }
    free(hm->table[i].entries);
  }
  free(hm->table);
  hm->table = new_table;
  hm->size = new_size;
}

void hashmap_import(hashmap_t* to, hashmap_t* from) {
  for (uint64_t i = 0; i < from->size; i++) {
    if (from->table[i].entries == NULL || from->table[i].collisions == 0) continue;
    for (uint64_t j = 0; j < from->table[i].collisions; j++) {
      uint64_t hash = hashmap_hash(from->table[i].entries[j].key) % (to->size - 1);
      if (to->table[hash].collisions > 0) {
        for (uint64_t k = 0; k < to->table[hash].collisions; k++) {
          if (!strcmp(to->table[hash].entries[k].key, from->table[i].entries[j].key)) {
            continue; // Skip over, we already have this on the hashmap
          }
        }
      }
      hashmap_add(to, from->table[i].entries[j].key, from->table[i].entries[j].data);
    }
  }
}

void hashmap_destroy(hashmap_t* hm) {
  for (uint64_t i = 0; i < hm->size; i++) {
    if (hm->table[i].entries == NULL || hm->table[i].collisions == 0) continue;
    for (uint64_t j = 0; j < hm->table[i].collisions; j++) {
      free(hm->table[i].entries[j].key);
    }
    free(hm->table[i].entries);
  }
  free(hm->table);
  free(hm);
}

void hashmap_destroy_free_items(hashmap_t* hm) {
  for (uint64_t i = 0; i < hm->size; i++) {
    if (hm->table[i].entries == NULL || hm->table[i].collisions == 0) continue;
    for (uint64_t j = 0; j < hm->table[i].collisions; j++) {
      free(hm->table[i].entries[j].data);
      free(hm->table[i].entries[j].key);
    }
    free(hm->table[i].entries);
  }
  free(hm->table);
  free(hm);
}