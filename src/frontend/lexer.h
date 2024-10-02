#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../list.h"
#include "../hashmap.h"

enum {
  TOK_EOF = 0,

  TOK_ID,
  TOK_STRING,
  TOK_NUM,
  TOK_CHAR,
  TOK_FLOAT,

  TOK_LPAR,
  TOK_RPAR,
  TOK_LBRAC,
  TOK_RBRAC,
  TOK_LSQBR,
  TOK_RSQBR,

  TOK_SEMI,
  TOK_COLON,

  TOK_COMMA,

  TOK_PLUS,
  TOK_MINUS,
  TOK_STAR,
  TOK_DIV,
  TOK_MOD,

  TOK_HAT, // ^
  TOK_NOT, // ~

  TOK_EQ,
  TOK_EQEQ,
  TOK_GT,
  TOK_GTEQ,
  TOK_LT,
  TOK_LTEQ,
  TOK_EXCL,
  TOK_EXCLEQ,
  TOK_AMPER,
  TOK_DBAMPER,
  TOK_PIPE,
  TOK_DBPIPE,

  TOK_AT,

  TOK_DOT,
  TOK_3DOT,

  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOR,
  TOK_FN,
  TOK_VAR,
  TOK_RET,
  TOK_SIGNED,
  TOK_UNSIGNED,
  TOK_IMPORT,
  TOK_STRUCT,
  TOK_ALIGN,
  TOK_EXTERN,
  TOK_PACKED,
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_SIZEOF,
  TOK_FALSE,
  TOK_TRUE,
  TOK_ENUM,
  TOK_SWITCH,
};

typedef struct {
  const char* filename;
  size_t row;
  size_t col;
  size_t raw;
} loc_t;

typedef struct {
  uint8_t type;
  char* text;
  int text_len;
  loc_t position;
  bool hex;
} token_t;

typedef struct {
  list_t* tok_list;
  char* filename;
  char* source;
  char c;
  loc_t position;

  hashmap_t* kw_hm;
} lexer_t;

extern const char* type_to_str[];

lexer_t* lexer_create(char* source, char* filename);
void lexer_advance(lexer_t* l);
void lexer_lex(lexer_t* l);
void lexer_destroy(lexer_t* l);