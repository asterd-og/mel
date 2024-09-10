#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct list_item {
  void* data;
  struct list_item* next;
  struct list_item* prev;
} list_item_t;

typedef struct {
  list_item_t* head;
  list_item_t* iterator;
  size_t size;
} list_t;

list_t* list_create();
void list_add(list_t* l, void* data);
void list_import(list_t* to, list_t* from);
void list_destroy(list_t* l, bool free_item_data);