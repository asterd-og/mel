#include "list.h"
#include <stdlib.h>
#include <string.h>

list_t* list_create() {
  list_t* l = (list_t*)malloc(sizeof(list_t));
  l->head = (list_item_t*)malloc(sizeof(list_item_t));
  l->head->next = l->head->prev = l->head;
  l->iterator = l->head;
  l->size = 0;
  return l;
}

void list_add(list_t* l, void* data) {
  list_item_t* item = (list_item_t*)malloc(sizeof(list_item_t));
  item->data = data;
  item->next = l->head;
  item->prev = l->head->prev;
  l->head->prev->next = item;
  l->head->prev = item;
  l->size++;
}

void list_import(list_t* to, list_t* from) {
  for (list_item_t* item = from->head->next; item != from->head; item = item->next) {
    list_add(to, item->data);
  }
}

void list_destroy(list_t* l, bool free_item_data) {
  list_item_t* next;
  for (list_item_t* item = l->head->next; item != l->head; item = next) {
    next = item->next;
    if (free_item_data)
      free(item->data);
    free(item);
  }

  free(l->head);
  free(l);
}