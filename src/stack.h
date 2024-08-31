#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
  int* data;
  uint64_t cap;
  uint64_t idx;
} stack_t;

stack_t* stack_create(uint64_t cap);
void stack_push(stack_t* stack, int value);
int stack_get(stack_t* stack, int rel);
int stack_pop(stack_t* stack);