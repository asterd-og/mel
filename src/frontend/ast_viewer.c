#include "ast_viewer.h"
#include <stdio.h>

// This is a mess, but its just a way for me to visualise the AST, not an actual super important piece of the program.

int ident = 0;

void expr_view(node_t* node);
void fn_call_view(node_t* node, bool stmt);
void stmt_view(node_t* node, bool do_ident);

void term_view(node_t* node) {
  switch (node->type) {
    case NODE_INT:
      printf("%ld", node->value);
      break;
    case NODE_NOT:
      printf("~");
      term_view(node->lhs);
      break;
    case NODE_EXCL:
      printf("!");
      term_view(node->lhs);
      break;
    case NODE_NEG:
      printf("-");
      term_view(node->lhs);
      break;
    case NODE_ID:
      printf("%.*s", node->tok->text_len, node->tok->text);
      break;
    case NODE_STR:
      printf("\"%.*s\"", node->tok->text_len, node->tok->text);
      break;
    case NODE_ARR_IDX: {
      list_t* expr_list = (list_t*)node->data;
      printf("%.*s", node->tok->text_len, node->tok->text);
      for (list_item_t* expr = expr_list->head->next; expr != expr_list->head; expr = expr->next) {
        printf("[");
        expr_view((node_t*)expr->data);
        printf("]");
      }
      break;
    }
    case NODE_AT:
      printf("@");
      expr_view(node->lhs);
      break;
    case NODE_REF:
      printf("&");
      expr_view(node->lhs);
      break;
    case NODE_STRUCT_ACC:
      printf("%.*s", node->tok->text_len, node->tok->text);
      node_t* temp = node;
      while (temp->lhs) {
        printf(".");
        temp = temp->lhs;
        printf("%.*s", temp->tok->text_len, temp->tok->text);
      }
      break;
    case NODE_FN_CALL:
      fn_call_view(node, false);
      break;
    case NODE_ADD...NODE_XOR:
      expr_view(node);
      break;
  }
}

void expr_view(node_t* node) {
  if (!node->lhs && !node->rhs) {
    term_view(node);
    return;
  }
  node_t* lhs = (node->type == NODE_AT || node->type == NODE_REF || node->type == NODE_STRUCT_ACC ? node : node->lhs);
  printf("(");
  static char* table[] = {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"};
  term_view(lhs);
  if (node->rhs) {
    printf(" %s ", table[node->type - NODE_ADD]);
    term_view(node->rhs);
  }
  printf(")");
}

void expr_or_arr_view(node_t* node);

void arr_view(node_t* node) {
  arr_t* arr = (arr_t*)node->data;
  printf("[");
  for (list_item_t* item = arr->expr_list->head->next; item != arr->expr_list->head;
    item = item->next) {
    node_t* expr_or_arr = (node_t*)item->data;
    expr_or_arr_view(expr_or_arr);
    if (item->next != arr->expr_list->head) {
      printf(", ");
    }
  }
  printf("]");
}

void expr_or_arr_view(node_t* node) {
  if (node->type == NODE_ARRAY) {
    arr_view(node);
    return;
  }
  expr_view(node);
}

void type_view(type_t* ty) {
  if (ty->_signed != ty->type->_signed) {
    printf("%s ", (ty->_signed ? "signed" : "unsigned"));
  }
  printf("%s", ty->type->name);
  if (ty->is_pointer) {
    int stcnt = 0;
    type_t* temp = ty;
    while (temp->is_pointer) {
      stcnt++;
      temp = temp->pointer;
    }
    for (int i = 0; i < stcnt; i++) {
      printf("*");
    }
  } else if (ty->is_arr) {
    type_t* temp = ty;
    while (temp->is_arr) {
      temp = temp->pointer;
      printf("[");
      expr_view(temp->arr_size);
      printf("]");
    }
    temp = ty;
    while (temp->is_arr) {
      temp = temp->pointer;
    }
  }
}

void var_decl_view(node_t* node, bool param) {
  var_t* var = (var_t*)node->data;
  printf("var %.*s: ", var->name->text_len, var->name->text);
  type_view(var->type);
  if (!var->initialised) {
    if (!param) printf(";");
    return;
  }
  printf(" = ");
  expr_or_arr_view(node->lhs);
  if (!param) printf(";");
}

void fn_decl_view(node_t* node) {
  fun_t* fun = (fun_t*)node->data;
  printf("fn %.*s(", fun->name->text_len, fun->name->text);
  for (list_item_t* param = fun->params->head->next; param != fun->params->head;
    param = param->next) {
    node_t* node = (node_t*)param->data;
    var_decl_view(node, true);
    if (param->next != fun->params->head) {
      printf(", ");
    }
  }
  printf(") : ");
  type_view(fun->type);
  printf("%s\n", (fun->initialised ? " {" : ";"));
  if (!fun->initialised) return;
  ident++;
  ast_view(fun->body);
  ident--;
  printf("%*s", ident * 2, "");
  printf("}");
}

void fn_call_view(node_t* node, bool stmt) {
  funcall_t* fcall = (funcall_t*)node->data;
  printf("%.*s(", fcall->name->text_len, fcall->name->text);
  for (list_item_t* arg = fcall->arguments->head->next; arg != fcall->arguments->head;
    arg = arg->next) {
    node_t* node = (node_t*)arg->data;
    if (node->type == NODE_SKIP) {
      printf("(SKIP)");
    } else {
      expr_or_arr_view(node);
    }
    if (arg->next != fcall->arguments->head) {
      printf(", ");
    }
  }
  printf(")%s", (stmt ? ";" : ""));
}

void assign_view(node_t* node) {
  static char* op[] = {
    [NODE_ASSIGN] = "=", [NODE_ASADD] = "+=", [NODE_ASSUB] = "-=",
    [NODE_ASMUL] = "*=", [NODE_ASDIV] = "/=", [NODE_ASMOD] = "%=",
    [NODE_ASSHL] = "<<=", [NODE_ASSHR] = ">>=", [NODE_ASOR] = "|=",
    [NODE_ASAND] = "&=", [NODE_ASXOR] = "^="};
  term_view(node->lhs);
  printf(" %s ", op[node->type]);
  expr_view(node->rhs);
  printf(";");
}

void ret_view(node_t* node) {
  printf("ret");
  if (node->lhs) {
    printf(" ");
    expr_view(node->lhs);
  }
  printf(";");
}

void cond_view(node_t* cond) {
  static char* op[] = {"==", ">", ">=", "<", "<=", "!", "!=", "&", "&&", "|", "||"};
  if (cond->type >= NODE_EQEQ && cond->type <= NODE_DBOR && cond->type != NODE_NOT) {
    expr_view(cond->lhs);
    printf(" %s ", op[cond->type - NODE_EQEQ]);
    expr_view(cond->rhs);
  } else {
    expr_view(cond);
  }
}

void compound_view(node_t* node) {
  list_t* ast = (list_t*)node->data;
  printf("{\n");
  ident++;
  ast_view(ast);
  ident--;
  printf("%*s", ident * 2, "");
  printf("}");
}

void if_view(node_t* node) {
  if_stmt_t* stmt = (if_stmt_t*)node->data;
  printf("if (");
  cond_view(node->lhs);
  printf(") ");
  if (stmt->initialised) {
    stmt_view(stmt->true_stmt, false);
    if (stmt->false_stmt) {
      printf(" else ");
      stmt_view(stmt->false_stmt, false);
    }
  }
}

void for_view(node_t* node) {
  for_stmt_t* stmt = (for_stmt_t*)node->data;
  printf("for (");
  if (stmt->primary_stmt->type == NODE_VAR_DEF) { var_decl_view(stmt->primary_stmt, false); }
  else assign_view(stmt->primary_stmt);
  printf(" ");
  cond_view(node->lhs);
  if (stmt->custom_step) {
    printf("; ");
    assign_view(stmt->step);
  }
  printf(") ");
  stmt_view(stmt->body, false);
}

void while_view(node_t* node) {
  while_stmt_t* stmt = (while_stmt_t*)node->data;

  printf("while (");
  cond_view(node->lhs);
  printf(")");
  if (stmt->initialised) {
    printf(" ");
    compound_view(stmt->body);
  } else {
    printf(";");
  }
}

void stmt_view(node_t* node, bool do_ident) {
  if (do_ident) printf("%*s", ident * 2, "");
  switch (node->type) {
    case NODE_COMPOUND:
      compound_view(node);
      break;
    case NODE_FOR:
      for_view(node);
      printf("\n");
      break;
    case NODE_WHILE:
      while_view(node);
      printf("\n");
      break;
    case NODE_VAR_DEF:
      var_decl_view(node, false);
      printf("\n");
      break;
    case NODE_IF:
      if_view(node);
      printf("\n");
      break;
    case NODE_FN_CALL:
      fn_call_view(node, true);
      printf("\n");
      break;
    case NODE_FN_DEF:
      fn_decl_view(node);
      printf("\n");
      break;
    case NODE_RET:
      ret_view(node);
      printf("\n");
      break;
    case NODE_ASSIGN...NODE_ASXOR:
      assign_view(node);
      printf("\n");
      break;
  }
}

void ast_view(list_t* ast) {
  for (list_item_t* item = ast->head->next; item != ast->head; item = item->next) {
    node_t* node = (node_t*)item->data;
    stmt_view(node, true);
  }
}