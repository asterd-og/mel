#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "../frontend/parser.h"
#include "../list.h"
#include "../hashmap.h"
#include "../stack.h"

typedef struct {
  hashmap_t* members; // Gonna have a hashmap of objs, with the offset always starting at 0
} cg_struct_t;

typedef struct {
  bool glb;
  int offset;
  type_t* type;
  fun_t* fun;
  int item_count; // array
  cg_struct_t* _struct;
} cg_obj_t;

typedef struct cg_scope_t {
  hashmap_t* locl_objs;
  struct cg_scope_t* parent;
} cg_scope_t;

/*
 First Argument: %rdi
 Second Argument: %rsi
 Third Argument: %rdx
 Fourth Argument: %r10
 Fifth Argument: %r8
 Sixth Argument: %r9
*/

typedef struct {
  list_t* ast;
  FILE* out;
  int stack_offset;
  int label_cnt;
  int arr_count;
  uint16_t reg_bitmap; // used registers bitmap
  uint16_t rstr_bitmap; // restor registers bitmap
  fun_t* current_func;
  type_t* current_type;
  type_t* current_func_type;
  bool current_func_ret; // Check if current function has returned or not
  hashmap_t* glb_objs;
  hashmap_t* locl_objs;
  hashmap_t* structs; // Hashmap of cg_struct_t
  cg_scope_t* scope;
  list_t* strings;
  list_t* glb_vars; // To put in data section
  stack_t* reg_stack; // For function calls
} cg_t;

cg_t* cg_create(list_t* ast, char* out_fname);
int cg_reg_alloc(cg_t* cg);
void cg_reg_free(cg_t* cg, int reg);
int cg_new_offset(cg_t* cg, type_t* type);
int cg_expr(cg_t* cg, node_t* node);
char* cg_get_obj(cg_t* cg, token_t* name, node_t* node);
cg_obj_t* cg_find_object(cg_t* cg, char* name);
void cg_gen(cg_t* cg);