#include "type_checker.h"
#include <stdlib.h>
#include <string.h>
#include "parser.h"

basetype_t* u8, *u16, *u32, *u64;
basetype_t* i8, *i16, *i32, *i64, *_void;
basetype_t* f32, *f64, *_bool;

type_t* ty_u64;

type_checker_t* type_checker_create() {
  type_checker_t* tychk = (type_checker_t*)malloc(sizeof(type_checker_t));
  tychk->types_hm = hashmap_create(150, 5);
  NEW_TYPE(_bool, "bool", 5, 1, 1, false, false)
  NEW_TYPE(u8, "u8", 3, 1, 1, false, false)
  NEW_TYPE(i8, "i8", 3, 1, 1, true, false)

  NEW_TYPE(u16, "u16", 4, 2, 2, false, false)
  NEW_TYPE(i16, "i16", 4, 2, 2, true, false)

  NEW_TYPE(u32, "u32", 4, 4, 4, false, false)
  NEW_TYPE(i32, "i32", 4, 4, 4, true, false)

  NEW_TYPE(u64, "u64", 4, 8, 8, false, false)
  NEW_TYPE(i64, "i64", 4, 8, 8, true, false)
  NEW_TYPE(_void, "void", 5, 0, 0, false, false)
  NEW_TYPE(f32, "f32", 4, 4, 4, true, true)
  NEW_TYPE(f64, "f64", 4, 8, 8, true, true)
  hashmap_add(tychk->types_hm, "bool", _bool);
  hashmap_add(tychk->types_hm, "u8", u8);   hashmap_add(tychk->types_hm, "i8", i8);
  hashmap_add(tychk->types_hm, "u16", u16); hashmap_add(tychk->types_hm, "i16", i16);
  hashmap_add(tychk->types_hm, "u32", u32); hashmap_add(tychk->types_hm, "i32", i32);
  hashmap_add(tychk->types_hm, "u64", u64); hashmap_add(tychk->types_hm, "i64", i64);
  hashmap_add(tychk->types_hm, "char", u8); hashmap_add(tychk->types_hm, "void", _void);
  hashmap_add(tychk->types_hm, "short", i16);
  hashmap_add(tychk->types_hm, "int", i32);
  hashmap_add(tychk->types_hm, "long", i64);
  hashmap_add(tychk->types_hm, "f32", f32);
  hashmap_add(tychk->types_hm, "f64", f64);
  ty_u64 = (type_t*)malloc(sizeof(type_t));
  memset(ty_u64, 0, sizeof(type_t));
  ty_u64->type = u64;
  return tychk;
}

basetype_t* type_checker_check(type_checker_t* tychk, token_t* tok) {
  char name[tok->text_len];
  strncpy(name, tok->text, tok->text_len);
  name[tok->text_len] = 0;
  basetype_t* type = (basetype_t*)hashmap_get(tychk->types_hm, name);
  return type;
}

void type_checker_add(type_checker_t* tychk, token_t* name, basetype_t* type) {
  char* pname = parse_str(name);
  type->name = pname;
  hashmap_add(tychk->types_hm, pname, type);
}