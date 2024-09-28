#include "parser.h"
#include <string.h>

type_t* parser_current_ty = NULL;

// Add_Expression = Term, { ("+" | "-"), Term }
node_t* parse_add_expr(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_term(parser);
  node_t* rhs = NULL;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PLUS:
        if (parser_current_ty && parser_current_ty->is_pointer) parser_current_ty = parser_current_ty->pointer;
        parser_consume(parser);
        rhs = parse_term(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value + rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_ADD;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_MINUS:
        if (parser_current_ty && parser_current_ty->is_pointer) parser_current_ty = parser_current_ty->pointer;
        parser_consume(parser);
        rhs = parse_term(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value - rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
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
node_t* parse_shift_expr(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_add_expr(parser);
  node_t* rhs = NULL;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_LT:
        if (parser_peek(parser)->type != TOK_LT) return lhs;
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_eat(parser, TOK_LT);
        parser_consume(parser);
        rhs = parse_add_expr(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value << rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_SHL;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_GT:
        if (parser_peek(parser)->type != TOK_GT) return lhs;
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_eat(parser, TOK_GT);
        parser_consume(parser);
        rhs = parse_add_expr(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value >> rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_SHR;
        temp->lhs = lhs;
        temp->rhs = rhs;
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

// Shift_Expression, { ("|" | "^" | "&"), Shift_Expression }
node_t* parse_bitwise_expr(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_shift_expr(parser);
  node_t* rhs = NULL;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PIPE:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_shift_expr(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value | rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_OR;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_HAT:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_shift_expr(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value ^ rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_XOR;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_AMPER:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_shift_expr(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value & rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_AND;
        temp->lhs = lhs;
        temp->rhs = rhs;
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

// Term = Factor, { ("*" | "/"), Factor }
node_t* parse_term(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_factor(parser);
  node_t* rhs;
  while (true) {
    token_t* op_tok = parser_consume(parser);
    switch (op_tok->type) {
      case TOK_STAR:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_factor(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value * rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_MUL;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_DIV:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_factor(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value / rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_DIV;
        temp->lhs = lhs;
        temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_MOD:
        if (parser_current_ty && parser_current_ty->is_pointer) {
          parser_error(parser, "Trying to do illegal pointer arithmetic operation.");
          return NULL;
        }
        parser_consume(parser);
        rhs = parse_factor(parser);
        temp = NEW_DATA(node_t);
        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
          temp->type = NODE_INT;
          temp->value = lhs->value % rhs->value;
          temp->tok = op_tok;
          free(lhs); free(rhs);
          lhs = temp;
          break;
        }
        temp->type = NODE_MOD;
        temp->lhs = lhs;
        temp->rhs = rhs;
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

// Factor = Int | ID | "(", Expression, ")"
node_t* parse_factor(parser_t* parser) {
  token_t* tok = parser->token;
  node_t* node;
  switch (tok->type) {
    case TOK_ID: {
      object_t* obj = parser_find_obj(parser, parse_str(tok));
      bool set_type = false;
      if (parser_current_ty == NULL) {
        parser_current_ty = obj->type;
        set_type = true;
      }
      if ((parser_peek(parser)->type == TOK_LPAR && obj->type->type->size == 0) ||
          (!obj->func && obj->type->type->size == 0)) {
        parser_error(parser, "Object has type void.");
        return NULL;
      }
      token_t* peek = parser_peek(parser);
      if (peek->type == TOK_LPAR) {
        node = parse_fn_call(parser, false);
        parser_current_ty = obj->type;
        node->lhs = NULL; node->rhs = NULL;
        parser_type_check(parser, obj->type, parser_current_ty);
        break;
      }
      if (obj->func) {
        parser_warn(parser, "'%s' is a function, not a variable.", obj->name);
        if (set_type)
          parser_current_ty->is_pointer = true;
      }
      node = parse_id(parser, parser_current_ty);
      parser_rewind(parser);
      break;
    }
    case TOK_NOT:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_NOT;
      node->tok = parser->token;
      node->lhs = parse_factor(parser);
      break;
    case TOK_EXCL:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_EXCL;
      node->tok = parser->token;
      node->lhs = parse_factor(parser);
      break;
    case TOK_FLOAT:
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_FLOAT;
      node->fvalue = parse_float(tok);
      node->tok = tok;
      break;
    case TOK_MINUS:
      parser_consume(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_NEG;
      node->tok = parser->token;
      node->lhs = parse_factor(parser);
      break;
    case TOK_AMPER: {
      parser_consume(parser);
      node_t* expr = parse_add_expr(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_REF;
      node->lhs = expr;
      parser_rewind(parser);
      break;
    }
    case TOK_AT: {
      parser_consume(parser);
      node_t* expr = parse_bitwise_expr(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_AT;
      node->lhs = expr;
      parser_rewind(parser);
      break;
    }
    case TOK_NUM:
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_INT;
      node->value = parse_num(tok);
      node->tok = tok;
      break;
    case TOK_CHAR: {
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_INT;
      char chr;
      if (tok->text[tok->text_len - 2] == '\\') {
        switch (tok->text[tok->text_len - 1]) {
          case 'a': chr = '\a'; break;
          case 'b': chr = '\b'; break;
          case 'f': chr = '\f'; break;
          case 'v': chr = '\v'; break;
          case 'n': chr = '\n'; break;
          case 't': chr = '\t'; break;
          case 'r': chr = '\r'; break;
          default: chr = tok->text[tok->text_len - 1];
        }
      } else {
        if (tok->text[tok->text_len - 1] == '\\') {
          parser_error(parser, "Invalid character literal.");
          return NULL;
        }
        chr = tok->text[tok->text_len - 1];
      }
      node->value = chr;
      node->tok = tok;
      break;
    }
    case TOK_FALSE:
    case TOK_TRUE:
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_INT;
      node->value = (tok->type == TOK_TRUE);
      node->tok = tok;
      break;
    case TOK_STRING:
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_STR;
      node->tok = tok;
      break;
    case TOK_LPAR:
      parser_consume(parser);
      node = parse_expression(parser, NULL);
      if (parser->token->type != TOK_RPAR) {
        parser_error(parser, "Unexpected token. Expected ')' but got '%.*s'.", parser->token->text_len, parser->token->text);
        return NULL;
      }
      break;
    case TOK_LT: {
      type_t* ty = parser_get_type(parser);
      parser_eat(parser, TOK_GT);
      parser_consume(parser);
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      parser_current_ty = NULL;
      node->lhs = parse_factor(parser);
      node->data = ty;
      node->type = NODE_CAST;
      parser_current_ty = ty;
      break;
    }
    case TOK_SIZEOF: {
      token_t* next = parser_consume(parser);
      int64_t size = 0;
      type_t* type;
      if (next->type == TOK_STAR) {
        parser_rewind(parser);
        type = parser_get_type(parser);
      } else {
        object_t* obj = parser_internal_find_obj(parser, parse_str(next), false);
        if (!obj) {
          parser_rewind(parser);
          type = parser_get_type(parser);
        } else {
          type = obj->type;
        }
      }
      if (type->is_arr) {
        type_t* temp = type;
        while (temp->pointer) {
          temp = temp->pointer;
          size += type->type->size * temp->arr_size->value;
        }
      } else if (type->is_pointer) {
        size = 8;
      } else {
        size = type->type->size;
      }
      node = NEW_DATA(node_t);
      memset(node, 0, sizeof(node_t));
      node->type = NODE_INT;
      node->value = size;
      node->tok = next;
      break;
    }
    default:
      parser_error(parser, "Unexpected factor '%.*s'", tok->text_len, tok->text);
      return NULL;
      break;
  }
  return node;
}

// Expression = Bitwise_Expression
node_t* parse_expression(parser_t* parser, type_t* ty) {
  parser_current_ty = ty;
  return parse_bitwise_expr(parser);
}

// lvalue, { ("+"), Factor }

node_t* parse_simple_expr(parser_t* parser) {
  node_t* temp;
  node_t* lhs = parse_lvalue(parser);
  node_t* rhs;
  parser_current_ty = NULL;
  while (true) {
    token_t* op_tok = parser->token;
    switch (op_tok->type) {
      case TOK_PLUS:
        parser_consume(parser);
        rhs = parse_term(parser);
        temp = NEW_DATA(node_t);
        temp->type = NODE_ADD;
        temp->lhs = lhs; temp->rhs = rhs;
        temp->tok = op_tok;
        lhs = temp;
        break;
      case TOK_MINUS:
        parser_consume(parser);
        rhs = parse_term(parser);
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

node_t* parse_expr(parser_t* parser, type_t* ty) {
  if (parser->token->type == TOK_LSQBR) {
    if (ty && ty->is_pointer && !ty->is_arr) {
      parser_error(parser, "Trying to initialise a pointer with an array.");
      return NULL;
    }
    parser_consume(parser);
    return parse_array(parser, ty);
  } else if (parser->token->type == TOK_AMPER) {
    return parse_ref(parser, ty);
  }
  node_t* node = parse_expression(parser, ty);
  if ((ty && parser_current_ty) && (ty->is_pointer && !parser_current_ty->is_pointer)) {
    parser_error(parser, "Trying to pass a non-pointer object to pointer. Try casting like '<Type>'.");
    return NULL;
  }
  return node;
}