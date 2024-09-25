#include "parser.h"
#include <stdarg.h>
#include <stdio.h>
#include "type_checker.h"
#include <stdlib.h>
#include <string.h>
#include "../file.h"
#include <libgen.h>

parser_t* parser_create(lexer_t* lexer) {
  parser_t* parser = (parser_t*)malloc(sizeof(parser_t));
  parser->lexer = lexer;
  parser->ast = list_create();
  parser->tychk = type_checker_create();
  return parser;
}

void parser_msgat(parser_t* parser) {
  int pos = parser->token->position.raw;
  int end = pos;
  while (parser->lexer->source[pos - 1] != '\n' && pos > 0) {
    pos--;
  }
  while (parser->lexer->source[end] != '\n' && end != 0) {
    end++;
  }
  fprintf(stderr, "%s:%zu ", parser->lexer->filename, parser->token->position.row);
  fprintf(stderr, "\n%.*s\n", (end - pos), parser->lexer->source + pos);
  for (size_t i = 0; i < parser->token->position.col - parser->token->text_len - 1; i++) {
    if (parser->lexer->source[i] == '\t') {
      fprintf(stderr, "\t");
      continue;
    }
    fprintf(stderr, " ");
  }
  fprintf(stderr, "\e[0;32m^");
  for (int i = 0; i < parser->token->text_len - 1; i++)
    fprintf(stderr, "~");
}

void parser_error(parser_t* parser, const char* fmt, ...) {
  parser_msgat(parser);
  fprintf(stderr, " \e[0;31mError: \e[0;39m");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

void parser_warn(parser_t* parser, const char* fmt, ...) {
  parser_msgat(parser);
  fprintf(stderr, " \e[0;33mWarning: \e[0;39m");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

token_t* parser_eat(parser_t* parser, uint8_t tok_type) {
  parser->lexer_iterator = parser->lexer_iterator->next;
  parser->token = (token_t*)parser->lexer_iterator->data;
  if (parser->token->type != tok_type) {
    parser_error(parser, "Unexpected token. Expected '%s' but got '%s'.", type_to_str[tok_type], type_to_str[parser->token->type]);
    return NULL;
  }
  return parser->token;
}

token_t* parser_peek(parser_t* parser) {
  token_t* data = (token_t*)parser->lexer_iterator->next->data;
  return data;
}

token_t* parser_consume(parser_t* parser) {
  parser->lexer_iterator = parser->lexer_iterator->next;
  parser->token = (token_t*)parser->lexer_iterator->data;
  return parser->token;
}

token_t* parser_expect(parser_t* parser, uint8_t tok_type) {
  if (parser->token->type != tok_type) {
    parser_error(parser, "Unexpected token. Expected '%s' but got '%s'.", type_to_str[tok_type], type_to_str[parser->token->type]);
    return NULL;
  }
  return parser->token;
}

token_t* parser_rewind(parser_t* parser) {
  parser->lexer_iterator = parser->lexer_iterator->prev;
  parser->token = (token_t*)parser->lexer_iterator->data;
  return parser->token;
}

int64_t parse_num(token_t* tok) {
  char num[tok->text_len + 1];
  strncpy(num, tok->text, tok->text_len);
  num[tok->text_len] = 0;
  return (tok->hex ? strtol(num, NULL, 16) : atoi(num));
}

int64_t parse_neg(token_t* tok) {
  char num[tok->text_len + 2];
  num[0] = '-';
  strncpy(num + 1, tok->text, tok->text_len);
  num[tok->text_len + 1] = 0;
  return (tok->hex ? strtol(num, NULL, 16) : atoi(num));
}

char* parse_str(token_t* tok) {
  char* str = malloc(tok->text_len + 1);
  strncpy(str, tok->text, tok->text_len);
  str[tok->text_len] = 0;
  return str;
}

scope_t* parser_new_scope(parser_t* parser) {
  scope_t* scope = NEW_DATA(scope_t);
  scope->glob_scope = false;
  scope->parent = parser->scope;
  scope->locl_obj = hashmap_create(50, 10); // 500 objects, hashmap can resize though.
  return scope;
}

object_t* parser_internal_find_obj(parser_t* parser, char* name, bool error) {
  object_t* obj;
  if (parser->scope->glob_scope) {
    obj = hashmap_get(parser->glb_obj, name);
  } else {
    scope_t* temp = parser->scope;
    while (!temp->glob_scope) {
      obj = hashmap_get(temp->locl_obj, name);
      if (obj) break;
      temp = temp->parent; // Traverse all parent scopes (this helps in nested if statements)
    }
    if (!obj) {
      obj = hashmap_get(parser->glb_obj, name);
    }
  }
  if (!obj) {
    if (error) {
      parser_error(parser, "Undefined reference to object '%s'.", name);
      free(name);
    }
    return NULL;
  }
  free(name); // we should always use parser_find_obj and pass name as a parse_str
  return obj;
}

object_t* parser_find_obj(parser_t* parser, char* name) {
  return parser_internal_find_obj(parser, name, true);
}

object_t* parser_new_obj(parser_t* parser, type_t* type, token_t* tok, char* name, bool var, bool glb, list_t* params) {
  // TODO: Check if object is being re-defined.
  hashmap_t* map;
  if (!glb) {
    if (parser->scope->glob_scope) map = parser->glb_obj;
    else map = parser->scope->locl_obj;
  } else {
    map = parser->glb_obj;
  }
  object_t* obj = NEW_DATA(object_t);
  obj->type = type; obj->tok = tok; obj->name = name;
  obj->var = var; obj->func = !var; obj->params = params;
  hashmap_add(map, name, obj);
  if (!glb) {
    list_add(parser->current_fn->objs, obj);
  }
  return obj;
}

// Checks if both types are the same
void parser_type_check(parser_t* parser, type_t* ty1, type_t* ty2) {
  // Disable type checking rn, because its really buggy FIXME
  /*if (ty2->is_arr && ty1->is_pointer) {
    if (ty2->pointer && ty2->pointer->is_arr) {
      parser_error(parser, "Trying to pass 2+D array into double (or more) pointer.");
    }
  }*/
  /*if (ty1->is_pointer && ty2->is_pointer) {
    if (ty1->type->size != ty2->type->size) {
      parser_warn(parser, "Size-mismatch between pointers of '%s' and '%s'.", ty1->type->name, ty2->type->name);
    }
  } else {
    if (ty1->type != ty2->type) {
      parser_warn(parser, "Type-mismatch between type '%s' and '%s'.", ty1->type->name, ty2->type->name);
    }
  }*/
}

type_t* parser_get_type(parser_t* parser) {
  token_t* first_tok = parser_consume(parser);
  token_t* type_tok = first_tok;
  basetype_t* bt = NULL;
  bool _signed = false;
  bool is_ptr = false;
  int ptr_cnt = 0;
  if (first_tok->type == TOK_STAR) {
    is_ptr = true;
    // Handle multiple * chars before type (***int) or just once (*int)
    while (parser->token->type == TOK_STAR) {
      ptr_cnt++;
      parser_consume(parser);
    }
    first_tok = parser->token;
    type_tok = first_tok;
  }
  switch (first_tok->type) {
    case TOK_ID: {
      bt = type_checker_check(parser->tychk, first_tok);
      if (bt) _signed = bt->_signed;
      break;
    }
    case TOK_UNSIGNED: {
      _signed = false;
      type_tok = parser_eat(parser, TOK_ID);
      bt = type_checker_check(parser->tychk, type_tok);
      break;
    }
    case TOK_SIGNED: {
      _signed = true;
      type_tok = parser_eat(parser, TOK_ID);
      bt = type_checker_check(parser->tychk, type_tok);
      break;
    }
    default:
      parser_error(parser, "Unexpected token. Expected 'signed', 'unsigned', or type but got '%.*s'.",
        first_tok->text_len, first_tok->text);
      break;
  }
  if (bt == NULL) {
    parser_error(parser, "Unknown type '%.*s'.", type_tok->text_len, type_tok->text);
    return NULL;
  }
  // FIXME:
  // Probably just use opaque pointers, or u64s? anyways, cant uncomment this or else cant make pointers
  // to structures inside themselves (linked list for example)
  /*if (bt->size == 0 && is_ptr) {
    parser_error(parser, "Can't make pointer of void. Try u64.\n");
    return NULL;
  }*/
  type_t* type = NEW_DATA(type_t);
  type->alignment = bt->alignment;
  type->type = bt;
  type->_signed = _signed;
  type->pointer = NULL;
  type->is_pointer = is_ptr; type->is_arr = false;
  if (is_ptr) type->alignment = 8;
  if (parser_peek(parser)->type == TOK_LSQBR) {
    if (type->type->size == 0) {
      parser_error(parser, "You can't create an array of type void.\n");
      return NULL;
    }
    type_t* temp;
    type_t* point_to = type;
    int count = ptr_cnt;
    do {
      count++;
      point_to->is_arr = true;
      parser_eat(parser, TOK_LSQBR);
      parser_consume(parser);
      node_t* expr = NEW_DATA(node_t);
      expr = parse_expr(parser, NULL);
      if (expr->type != NODE_INT) {
        parser_error(parser, "Expression has ID.");
        return NULL;
      }
      if (parser->token->type != TOK_RSQBR) {
        parser_error(parser, "Unexpected token. Expected ']' but got '%.*s'.", parser->token->text_len, parser->token->text);
        return NULL;
      }
      temp = NEW_DATA(type_t);
      temp->is_arr = false; temp->is_pointer = false;
      temp->arr_size = expr;
      temp->type = bt;
      temp->_signed = _signed;
      temp->parent = point_to;
      point_to->pointer = temp;
      point_to = temp;
    } while (parser_peek(parser)->type == TOK_LSQBR);
    type->dimensions = count; type->is_pointer = is_ptr; type->ptr_cnt = ptr_cnt;
    return type;
  }
  type->dimensions = type->ptr_cnt = ptr_cnt;
  return type;
}

node_t* parse_stmt(parser_t* parser) {
  node_t* ret = NULL;
  switch (parser->token->type) {
    case TOK_LBRAC:
      ret = parse_compound_stmt(parser);
      break;
    case TOK_ID: case TOK_AT: {
      token_t* peek = parser_peek(parser);
      if (peek->type == TOK_LPAR && parser->token->type != TOK_AT) {
        ret = parse_fn_call(parser, true);
      } else {
        // if (peek->type >= TOK_PLUS && peek->type <= TOK_DIV || peek->type == TOK_EQ)
        ret = parse_assign(parser, true);
      }
      parser_consume(parser); // skip ;
      break;
    }
    case TOK_IF:
      ret = parse_if(parser);
      break;
    case TOK_WHILE:
      ret = parse_while(parser);
      break;
    case TOK_FOR:
      ret = parse_for(parser);
      break;
    case TOK_RET:
      ret = parse_ret(parser);
      parser_consume(parser); // skip ;
      break;
    case TOK_BREAK:
    case TOK_CONTINUE:
      ret = parse_break_continue(parser);
      parser_consume(parser); // skip ;
      break;
    case TOK_VAR:
      ret = parse_var_decl(parser, false, false);
      parser_consume(parser); // skip ;
      break;
    case TOK_EOF: {
      if (!parser->scope->glob_scope) {
        parser_error(parser, "Expected '}'.");
        return NULL;
      }
      ret = NEW_DATA(node_t);
      ret->type = NODE_NULL;
      break;
    }
    default:
      parser_error(parser, "Unexpected statement.\n");
      break;
  }
  return ret;
}

void parse_import(parser_t* parser) {
  token_t* name_tok = parser_eat(parser, TOK_STRING);
  char* fname = parse_str(name_tok);
  char* lname = strdup(parser->lexer->filename);
  char* path = dirname(lname);
  char* temp = malloc(name_tok->text_len + 2 + strlen(path));
  strcpy(temp, path);
  strcat(temp, "/");
  strcat(temp, fname);
  char* name = temp;
  char* file = open_file(name); // TODO: Look for it firstly in the /usr/mel/include, then on current path
  if (!file) {
    free(temp);
    temp = malloc(name_tok->text_len + 2 + strlen("/usr/mel/include/"));
    strcpy(temp, "/usr/mel/include/");
    strcat(temp, fname);
    name = temp;
    file = open_file(name);
    if (!file) {
      parser_error(parser, "Couldn't open file '%s'.", fname);
    }
  }
  free(fname);
  lexer_t* lexer = lexer_create(file, name);
  lexer_lex(lexer);

  parser_t* import = parser_create(lexer);
  parser_parse(import);

  free(name);

  list_import(parser->ast, import->ast);

  hashmap_import(parser->glb_obj, import->glb_obj);
  hashmap_import(parser->tychk->types_hm, import->tychk->types_hm); // TODO: Stop importing primitives.

  hashmap_destroy(import->glb_obj);
  hashmap_destroy(import->tychk->types_hm);
  // FIXME: lexer_destroy(lexer); maybe dont destroy tokens!?
  list_destroy(import->ast, false);
  free(import->scope);
  free(import);
  parser_eat(parser, TOK_SEMI);
}

void parser_parse(parser_t* parser) {
  list_t* tok_list = parser->lexer->tok_list;
  parser->lexer_iterator = tok_list->head->next;
  parser->token = (token_t*)parser->lexer_iterator->data;
  scope_t* glob_scope = NEW_DATA(scope_t);
  glob_scope->glob_scope = true;
  glob_scope->parent = NULL;
  parser->glb_obj = hashmap_create(150, 10);
  parser->scope = glob_scope;
  while (((token_t*)(parser->lexer_iterator->data))->type != TOK_EOF) {
    node_t* node;
    switch (parser->token->type) {
      case TOK_IMPORT:
        parse_import(parser);
        parser_consume(parser); // skip ;
        break;
      case TOK_VAR:
        node = parse_var_decl(parser, false, false);
        parser_consume(parser); // skip ;
        list_add(parser->ast, node);
        break;
      case TOK_FN: // TODO: Handle static for fn declaration
        node = parse_fn_decl(parser);
        list_add(parser->ast, node);
        break;
      case TOK_STRUCT:
        parse_struct_decl(parser);
        break;
      case TOK_RBRAC:
      case TOK_EOF:
        goto exit;
        break;
      default:
        parser_error(parser, "Unexpected expression outside of scope.\n");
        break;
    }
  }
exit:
;
}