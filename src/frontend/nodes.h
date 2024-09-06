#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../list.h"
#include "lexer.h"

enum {
  NODE_NULL,

  NODE_FN_DEF,
  NODE_FN_CALL,
  NODE_VAR_DEF,
  NODE_STMT,

  NODE_ADD, NODE_SUB,
  NODE_MUL, NODE_DIV,
  NODE_MOD, NODE_SHL,
  NODE_SHR, NODE_AND,
  NODE_OR, NODE_XOR,
  NODE_EXCL,

  NODE_INT, NODE_ID,
  NODE_ARRAY, NODE_STR,
  NODE_NEG,

  NODE_SKIP, // argument skip

  NODE_ASSIGN, NODE_ASADD,
  NODE_ASSUB, NODE_ASMUL,
  NODE_ASDIV, NODE_ASMOD,
  NODE_ASSHL, NODE_ASSHR,
  NODE_ASOR, NODE_ASAND,
  NODE_ASXOR,

  NODE_RET,

  NODE_COMPOUND,
  NODE_IF,
  NODE_FOR,
  NODE_WHILE,

  NODE_EQEQ, NODE_GT, NODE_GTEQ,
  NODE_LT, NODE_LTEQ, NODE_NOT,
  NODE_NOTEQ, NODE_DBAND, NODE_DBOR,

  NODE_ARR_IDX, NODE_AT, NODE_REF,
  NODE_STRUCT_ACC, NODE_3DOT
};

typedef struct node_t {
  int type;
  token_t* tok;

  int64_t value; // integer value in case of variable

  struct node_t* lhs;
  struct node_t* rhs;

  void* data; // var_t fun_t etc
} node_t;

typedef struct {
  bool _struct;
  bool _signed;
  int size; // in bytes
  int alignment;
  char* name;
  list_t* members; // Needs to be a list, so I can easily calculate the offset of X member
} basetype_t;

typedef struct type_t {
  bool is_pointer;
  int ptr_cnt; // single ptr, double ptr, etc...
  bool is_arr;
  bool _signed;
  int dimensions; // Array dimensions
  node_t* arr_size; // Array size (var arr: int[4] = ...) would be 4
  struct type_t* pointer;
  struct type_t* parent;
  basetype_t* type;
} type_t;

typedef struct {
  type_t* type;
  token_t* tok;
  char* name;

  bool func;
  bool var;
  list_t* params; // If func
  bool undef_params;
} object_t;

typedef struct scope_t {
  bool glob_scope;
  type_t* type; // if function
  hashmap_t* locl_obj;
  struct scope_t* parent;
} scope_t;

typedef struct {
  type_t* type;
  token_t* name;
  int alignment;
  bool _const;
  bool _static;
  bool initialised;
  bool glb;
} var_t;

typedef struct {
  type_t* type;
  bool _static;
  list_t* params;
  bool undef_params; // Undefined param count
  token_t* name;
  bool initialised;
  list_t* objs;
  list_t* body; // AST
} fun_t;

typedef struct {
  token_t* name;
  list_t* arguments;
} funcall_t;

typedef struct {
  list_t* expr_list;
} arr_t;

typedef struct {
  bool initialised;
  node_t* true_stmt; // In case of true
  node_t* false_stmt; // In case of false
} if_stmt_t;

typedef struct {
  bool custom_step;
  node_t* primary_stmt;
  node_t* step;
  node_t* body;
} for_stmt_t;

typedef struct {
  bool initialised;
  node_t* body;
} while_stmt_t;