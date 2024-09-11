#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../hashmap.h"
#include "lexer.h"
#include "nodes.h"

#define NEW_TYPE(ty, _name, nlen, _size, _alignment, signed_)\
  ty = (basetype_t*)malloc(sizeof(basetype_t));\
  ty->_struct = false;\
  ty->size = _size;\
  ty->alignment = _alignment;\
  ty->_signed = signed_;\
  ty->name = (char*)malloc(nlen); strncpy(ty->name, _name, nlen);

typedef struct {
  hashmap_t* types_hm;
} type_checker_t;

extern type_t* ty_u64;

type_checker_t* type_checker_create();
basetype_t* type_checker_check(type_checker_t* tychk, token_t* tok);
void type_checker_add(type_checker_t* tychk, token_t* name, basetype_t* type);