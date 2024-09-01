#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../list.h"
#include "lexer.h"
#include "nodes.h"
#include "type_checker.h"

#define NEW_DATA(type) (type*)malloc(sizeof(type))

typedef struct {
  lexer_t* lexer;
  list_t* ast;
  list_item_t* lexer_iterator;
  token_t* token;
  type_checker_t* tychk;
  scope_t* scope;
  hashmap_t* glb_obj;
  fun_t* current_fn;
} parser_t;

parser_t* parser_create(lexer_t* lexer);

void parser_error(parser_t* parser, const char* fmt, ...);
void parser_warn(parser_t* parser, const char* fmt, ...);
token_t* parser_eat(parser_t* parser, uint8_t tok_type);
token_t* parser_peek(parser_t* parser);
token_t* parser_consume(parser_t* parser);
token_t* parser_expect(parser_t* parser, uint8_t tok_type);
token_t* parser_rewind(parser_t* parser);

int64_t parse_num(token_t* tok);
int64_t parse_neg(token_t* tok);
char* parse_str(token_t* tok);

scope_t* parser_new_scope(parser_t* parser);

object_t* parser_find_obj(parser_t* parser, char* name);
object_t* parser_new_obj(parser_t* parser, type_t* type, token_t* tok, char* name, bool var, bool glb, list_t* params);
void parser_type_check(parser_t* parser, type_t* ty1, type_t* ty2);
type_t* parser_get_type(parser_t* parser);

node_t* parse_arr_idx(parser_t* parser);
node_t* parse_array(parser_t* parser, type_t* type);
node_t* parse_ref(parser_t* parser, type_t* type);
node_t* parse_expr(parser_t* parser, type_t* type);
node_t* parse_id(parser_t* parser);
node_t* parse_var_decl(parser_t* parser, bool param, bool struc_member);
list_t* parser_param_list(parser_t* parser, bool* undef_params);
list_t* parse_body(parser_t* parser);
node_t* parse_fn_decl(parser_t* parser);
list_t* parse_expr_list(parser_t* parser, list_t* expected_params, bool undef_params);
node_t* parse_fn_call(parser_t* parser, bool stmt);
node_t* parse_assign(parser_t* parser, bool exsemi);
node_t* parse_ret(parser_t* parser);
node_t* parse_condition(parser_t* parser);
node_t* parse_compound_stmt(parser_t* parser);
node_t* parse_if(parser_t* parser);
node_t* parse_for(parser_t* parser);
node_t* parse_while(parser_t* parser);

node_t* parse_add_expr(parser_t* parser, type_t* type);
node_t* parse_shift_expr(parser_t* parser, type_t* type);
node_t* parse_bitwise_expr(parser_t* parser, type_t* type);
node_t* parse_term(parser_t* parser, type_t* type);
node_t* parse_factor(parser_t* parser, type_t* type);
node_t* parse_expression(parser_t* parser, type_t* type);

node_t* parse_simple_expr(parser_t* parser);
node_t* parse_lvalue(parser_t* parser);

node_t* parse_stmt(parser_t* parser);
node_t* parse_fn_call(parser_t* parser, bool stmt);

void parser_parse(parser_t* parser);