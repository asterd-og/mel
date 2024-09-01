#include "parser.h"

// Add_Expression = Term, { ("+" | "-"), Term }
node_t* parse_add_expr(parser_t* parser, type_t* type) {
  node_t* temp;
  node_t* lhs = parse_term(parser, type);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PLUS:
        parser_consume(parser);
        rhs = parse_term(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_ADD;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_MINUS:
        parser_consume(parser);
        rhs = parse_term(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_SUB;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      default:
        return lhs;
        break;
    }
  }
  return NULL;
}

// Shift_Expression = Add_Expression, { ("<<" | ">>"), Add_Expression }
node_t* parse_shift_expr(parser_t* parser, type_t* type) {
  node_t* temp;
  node_t* lhs = parse_add_expr(parser, type);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_LT:
        if (parser_peek(parser)->type != TOK_LT) return lhs;
        parser_eat(parser, TOK_LT);
        parser_consume(parser);
        rhs = parse_add_expr(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_SHL;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      case TOK_GT:
        if (parser_peek(parser)->type != TOK_GT) return lhs;
        parser_eat(parser, TOK_GT);
        parser_consume(parser);
        rhs = parse_add_expr(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_SHR;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      default:
        return lhs;
        break;
    }
  }
  return NULL;
}

// Shift_Expression, { ("|" | "^" | "&"), Shift_Expression }
node_t* parse_bitwise_expr(parser_t* parser, type_t* type) {
  node_t* temp;
  node_t* lhs = parse_shift_expr(parser, type);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PIPE:
        parser_consume(parser);
        rhs = parse_shift_expr(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_OR;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      case TOK_HAT:
        parser_consume(parser);
        rhs = parse_shift_expr(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_XOR;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      case TOK_AMPER:
        parser_consume(parser);
        rhs = parse_shift_expr(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_AND;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      default:
        return lhs;
        break;
    }
  }
  return NULL;
}

// Term = Factor, { ("*" | "/"), Factor }
node_t* parse_term(parser_t* parser, type_t* type) {
  node_t* temp;
  node_t* lhs = parse_factor(parser, type);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser_consume(parser);
    switch (op_tok->type) {
      case TOK_STAR:
        parser_consume(parser);
        rhs = parse_factor(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_MUL;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      case TOK_DIV:
        parser_consume(parser);
        rhs = parse_factor(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_DIV;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      case TOK_MOD:
        parser_consume(parser);
        rhs = parse_factor(parser, type);
        temp = NEW_DATA(node_t);
        temp->type = NODE_MOD;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok->type;
        lhs = temp;
        break;
      default:
        return lhs;
        break;
    }
  }
  return NULL;
}

// Factor = Int | ID | "(", Expression, ")"
node_t* parse_factor(parser_t* parser, type_t* type) {
  token_t* tok = parser->token;
  node_t* node;
  switch (tok->type) {
    case TOK_ID: {
      object_t* obj = parser_find_obj(parser, parse_str(tok));
      if (type == NULL) {
        type = obj->type;
      }
      if (obj->type->type->size == 0 && !obj->type->is_pointer) {
        parser_error(parser, "Function returns void.");
        return NULL;
      }
      parser_type_check(parser, type, obj->type);
      token_t* peek = parser_peek(parser);
      if (peek->type == TOK_LPAR) {
        node = parse_fn_call(parser, false);
        break;
      } else if (peek->type == TOK_LSQBR) {
        node = parse_arr_idx(parser);
        break;
      }
      if (obj->func) {
        parser_error(parser, "'%s' is a function, not a variable.", obj->name);
        return NULL;
      }
      node = parse_id(parser);
      parser_rewind(parser);
      break;
    }
    case TOK_NOT:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      node->type = NODE_NOT;
      node->tok = parser->token;
      node->lhs = parse_factor(parser, type);
      break;
    case TOK_EXCL:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      node->type = NODE_EXCL;
      node->tok = parser->token;
      node->lhs = parse_factor(parser, type);
      break;
    case TOK_MINUS:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      node->type = NODE_NEG;
      node->tok = parser->token;
      node->lhs = parse_factor(parser, type);
      break;
    case TOK_AT: {
      parser_consume(parser);
      node_t* expr = parse_expression(parser, NULL);
      node = NEW_DATA(node_t);
      node->type = NODE_AT;
      node->lhs = expr;
      parser_rewind(parser);
      break;
    }
    case TOK_NUM:
      node = NEW_DATA(node_t);
      node->type = NODE_INT;
      node->value = parse_num(tok);
      node->tok = tok;
      break;
    case TOK_STRING:
      node = NEW_DATA(node_t);
      node->type = NODE_STR;
      node->tok = tok;
      break;
    case TOK_LPAR:
      parser_consume(parser);
      node = parse_expression(parser, type);
      if (parser->token->type != TOK_RPAR) {
        parser_error(parser, "Unexpected token. Expected ')' but got '%.*s'.", parser->token->text_len, parser->token->text);
        return NULL;
      }
      break;
    default:
      parser_error(parser, "Unexpected factor '%.*s'", tok->text_len, tok->text);
      return NULL;
      break;
  }
  return node;
}

// Expression = Bitwise_Expression
node_t* parse_expression(parser_t* parser, type_t* type) {
  return parse_bitwise_expr(parser, type);
}

// lvalue, { ("+"), Factor }

node_t* parse_simple_expr(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_lvalue(parser);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PLUS:
        parser_consume(parser);
        rhs = parse_term(parser, NULL);
        temp = NEW_DATA(node_t);
        temp->type = NODE_ADD;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_MINUS:
        parser_consume(parser);
        rhs = parse_term(parser, NULL);
        temp = NEW_DATA(node_t);
        temp->type = NODE_SUB;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_STAR...TOK_DIV:
        parser_error(parser, "Invalid binary operator.");
        return NULL;
      default:
        return lhs;
        break;
    }
  }
  return NULL;
}

node_t* parse_expr(parser_t* parser, type_t* type) {
  if (parser->token->type == TOK_LSQBR) {
    if (type->is_pointer) {
      parser_error(parser, "Trying to initialise a pointer with an array.");
      return NULL;
    }
    parser_consume(parser);
    return parse_array(parser, type);
  } else if (parser->token->type == TOK_AMPER) {
    return parse_ref(parser, type);
  }
  return parse_expression(parser, type);
}