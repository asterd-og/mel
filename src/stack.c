#include "stack.h"
#include <stdlib.h>
#include <stdio.h>

stack_t* stack_create(uint64_t cap) {
  stack_t* stack = (stack_t*)malloc(sizeof(stack));
  stack->cap = cap;
  stack->data = (int*)malloc(cap * 4);
  stack->idx = 0;
  return stack;
}

void stack_push(stack_t* stack, int value) {
  if (stack->idx > stack->cap) {
    fprintf(stderr, "Error: Too many items in the stack.\n");
    exit(1);
  }
  stack->data[stack->idx++] = value;
}

int stack_get(stack_t* stack, int rel) {
  if (stack->idx == 0) return 0;
  return stack->data[stack->idx - rel];
}

int stack_pop(stack_t* stack) {
  if (stack->idx == 0) return 0;
  stack->idx--;
  return stack->data[stack->idx];
}