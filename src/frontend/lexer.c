#include "lexer.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

const char* type_to_str[] = {
  "TOK_EOF",
  "TOK_ID",
  "TOK_STRING",
  "TOK_NUM",
  "TOK_LPAR",
  "TOK_RPAR",
  "TOK_LBRAC",
  "TOK_RBRAC",
  "TOK_LSQBR",
  "TOK_RSQBR",
  "TOK_SEMI",
  "TOK_COLON",
  "TOK_COMMA",
  "TOK_PLUS",
  "TOK_MINUS",
  "TOK_STAR",
  "TOK_DIV",
  "TOK_MOD",
  "TOK_HAT",
  "TOK_NOT",
  "TOK_EQ",
  "TOK_EQEQ",
  "TOK_GT",
  "TOK_GTEQ",
  "TOK_LT",
  "TOK_LTEQ",
  "TOK_EXCL",
  "TOK_EXCLEQ",
  "TOK_AMPER",
  "TOK_DBAMPER",
  "TOK_PIPE",
  "TOK_DBPIPE",
  "TOK_AT",
  "TOK_DOT",
  "TOK_3DOT",
  "TOK_IF",
  "TOK_ELSE",
  "TOK_WHILE",
  "TOK_FOR",
  "TOK_FN",
  "TOK_VAR",
  "TOK_RET",
  "TOK_SIGNED",
  "TOK_UNSIGNED",
  "TOK_IMPORT",
  "TOK_STRUCT",
  "TOK_ALIGN",
  "TOK_EXTERN",
  "TOK_PACKED",
  "TOK_BREAK",
  "TOK_CONTINUE"
};

const uint8_t singlechars[] = {
  ['('] = TOK_LPAR,
  [')'] = TOK_RPAR,
  ['{'] = TOK_LBRAC,
  ['}'] = TOK_RBRAC,
  ['['] = TOK_LSQBR,
  [']'] = TOK_RSQBR,
  [';'] = TOK_SEMI,
  [':'] = TOK_COLON,
  [','] = TOK_COMMA,
  ['+'] = TOK_PLUS,
  ['-'] = TOK_MINUS,
  ['*'] = TOK_STAR,
  ['/'] = TOK_DIV,
  ['%'] = TOK_MOD,
  ['^'] = TOK_HAT,
  ['~'] = TOK_NOT,
  ['='] = TOK_EQ,
  ['>'] = TOK_GT,
  ['<'] = TOK_LT,
  ['!'] = TOK_EXCL,
  ['&'] = TOK_AMPER,
  ['|'] = TOK_PIPE,
  ['@'] = TOK_AT,
  ['.'] = TOK_DOT,
};

void lexer_error(lexer_t* l, const char* fmt, ...) {
  int pos = l->position.raw;
  int end = pos;
  while (l->source[pos - 1] != '\n' && pos > 0) {
    pos--;
  }
  while (l->source[end] != '\n' && end != 0) {
    end++;
  }
  int ident = fprintf(stderr, "%s:%zu", l->filename, l->position.row);
  fprintf(stderr, "\n%.*s\n", (end - pos), l->source + pos);
  for (int i = 0; i < l->position.col - 1; i++) {
    if (l->source[i] == '\t') {
      fprintf(stderr, "\t");
      continue;
    }
    fprintf(stderr, " ");
  }
  fprintf(stderr, "\e[0;32m^ \e[0;31mError: \e[0;39m");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

void lexer_prepare_hashmaps(lexer_t* l) {
  l->kw_hm = hashmap_create(30, 5);
  char* kw_list[] = {"if", "else", "while", "for", "fn", "var", "ret", "signed", "unsigned", "import", "struct", "align", "extern", "packed", "break", "continue"};
  for (int i = 0; i < 16; i++) {
    hashmap_add(l->kw_hm, kw_list[i], (void*)(TOK_IF + i));
  }
}

lexer_t* lexer_create(char* source, char* filename) {
  lexer_t* l = (lexer_t*)malloc(sizeof(lexer_t));
  l->source = source;
  l->c = 0;
  l->position.col = l->position.row = 1;
  l->position.raw = 0;
  l->filename = filename;
  l->position.filename = l->filename;
  l->tok_list = list_create();

  lexer_prepare_hashmaps(l);

  return l;
}

token_t* lexer_create_token(lexer_t* l, uint8_t type, char* text) {
  token_t* tok = (token_t*)malloc(sizeof(token_t));
  tok->type = type;
  tok->text = text;
  tok->text_len = (l->source + l->position.raw) - text;
  tok->position = l->position;
  return tok;
}

int lexer_get_kw(lexer_t* l, char* start, char* end) {
  char kw[end - start + 1];
  strncpy(kw, start, end - start);
  kw[end - start] = 0;
  return (int)hashmap_get(l->kw_hm, kw);
}

void lexer_advance(lexer_t* l) {
  l->c = l->source[l->position.raw];
  switch (l->c) {
    case '\n':
      l->position.col = 1;
      l->position.row++;
      l->position.raw++;
      break;
    case ' ':
    case '\t':
      l->position.col++;
      l->position.raw++;
      break;
    case '0'...'9': {
      bool hex = false;
      if (l->c == '0') {
        if (l->source[l->position.raw + 1] == 'x') {
          l->position.raw += 2;
          l->position.col += 2;
          hex = true;
        }
      }
      char* start = l->source + l->position.raw;
      do {
        if (hex && tolower(l->source[l->position.raw]) > 'f') {
          lexer_error(l, "Invalid character!\n");
        }
        l->position.col++;
        l->c = l->source[++l->position.raw];
      } while (isdigit(l->c) || (hex && isalpha(l->c)));
      token_t* tok = lexer_create_token(l, TOK_NUM, start);
      tok->hex = hex;
      list_add(l->tok_list, tok);
      break;
    }
    case '_':
    case 'a'...'z':
    case 'A'...'Z': {
      char* start = l->source + l->position.raw;
      do {
        l->position.col++;
        l->c = l->source[++l->position.raw];
      } while (isalnum(l->c) || l->c == '_');
      int kw = lexer_get_kw(l, start, l->source + l->position.raw);
      list_add(l->tok_list, lexer_create_token(l, (kw > 0 ? kw : TOK_ID), start));
      break;
    }
    case '"': {
      l->position.raw++;
      l->position.col++;
      char* start = l->source + l->position.raw;
      do {
        l->position.col++;
        l->c = l->source[++l->position.raw];
      } while (l->c != '"');
      list_add(l->tok_list, lexer_create_token(l, TOK_STRING, start));
      l->position.raw++;
      l->position.col++;
      break;
    }
    case '(': case ')':
    case '{': case '}':
    case '[': case ']':
    case ';': case ':':
    case '+': case '-':
    case '*': case '/': {
      if (l->c == '/') {
        if (l->source[l->position.raw + 1] == '/') {
          // Ignore line!
          do {
            l->position.col++;
            l->c = l->source[++l->position.raw];
          } while (l->c != '\n');
          break;
        } else if (l->source[l->position.raw + 1] == '*') {
          l->position.col++;
          // Multiline comment
          do {
            l->position.col++;
            l->c = l->source[++l->position.raw];
            if (l->c == '\n') {
              l->position.row += 1;
              l->position.col = 1;
            }
          } while (l->source[l->position.raw] != '*' || l->source[l->position.raw + 1] != '/');
          l->position.raw += 2;
          l->position.col += 1;
          break;
        }
      }
    }
    case ',': case '@':
    case '^': case '~':
    case '%':
      l->position.raw++;
      l->position.col++;
      list_add(l->tok_list, lexer_create_token(l, singlechars[l->c], l->source + (l->position.raw - 1)));
      break;
    case '=': case '>': case '<': case '!': case '&': case '|': {
      uint8_t type = singlechars[l->c];
      char* start = l->source + l->position.raw;
      char expect_char = '=';
      if (l->c == '&' || l->c == '|') expect_char = l->c;
      if (l->source[l->position.raw + 1] == expect_char) {
        type++;
        l->position.raw++;
        l->position.col++;
      }
      l->position.raw++;
      l->position.col++;
      list_add(l->tok_list, lexer_create_token(l, type, start));
      break;
    }
    case '.': {
      char* start = l->source + l->position.raw;
      l->position.raw++;
      l->position.col++;
      uint8_t type = singlechars[l->c];
      if (l->source[l->position.raw] == '.' && l->source[l->position.raw + 1] == '.') {
        // ...
        l->position.raw += 2;
        l->position.col += 2;
        type = TOK_3DOT;
      }
      list_add(l->tok_list, lexer_create_token(l, type, start));
      break;
    }
    default:
      lexer_error(l, "Unknown character.");
      break;
  }
}

void lexer_lex(lexer_t* l) {
  while (l->source[l->position.raw] != '\0') {
    lexer_advance(l);
  }
  list_add(l->tok_list, lexer_create_token(l, TOK_EOF, l->source + l->position.raw));
}

void lexer_destroy(lexer_t* l) {
  list_destroy(l->tok_list, true);
  hashmap_destroy(l->kw_hm);
  free(l);
}