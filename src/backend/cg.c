#include "cg.h"
#include <stdlib.h>
#include "x86asm.h"
#include <stdio.h>
#include <string.h>

cg_t* cg_create(list_t* ast, char* out_fname) {
  cg_t* cg = (cg_t*)malloc(sizeof(cg_t));
  cg->ast = ast;
  cg->out = fopen(out_fname, "w");
  cg->reg_bitmap = 0;
  cg->glb_objs = hashmap_create(50, 5);
  cg->strings = list_create();
  cg->glb_vars = list_create();
  cg->reg_stack = stack_create(100);
  cg->structs = hashmap_create(50, 5);
  cg->scope = NULL;
  cg->label_cnt = 0;
  if (!cg->out) {
    fprintf(stderr, "Error while opening output file.\n");
    exit(1);
  }

  return cg;
}

int cg_reg_alloc(cg_t* cg) {
  for (uint16_t i = 0; i < MAX_REGS; i++) {
    if ((cg->reg_bitmap & (1 << i)) == 0) {
      cg->reg_bitmap |= (1 << i);
      return i;
    }
  }
  fprintf(stderr, "Error out of registers.\n"); // Should never reach here... tf we did wrong?
  exit(1);
}

void cg_reg_free(cg_t* cg, int reg) {
  if ((cg->reg_bitmap & (1 << reg)) == 0) return;
  cg->reg_bitmap &= ~(1 << reg);
}

int cg_new_offset(cg_t* cg, type_t* type) {
  if (type->type->_struct && !type->is_pointer) {
    cg->stack_offset += type->type->size;
  } else {
    cg->stack_offset += x86_get_size(type);
  }
  return -cg->stack_offset;
}

void cg_new_scope(cg_t* cg) {
  cg_scope_t* scope = NEW_DATA(cg_scope_t);
  scope->locl_objs = hashmap_create(50, 5);
  scope->parent = cg->scope;
  cg->scope = scope;
}

void cg_back_scope(cg_t* cg) {
  cg->current_func_ret = false;
  cg->scope = cg->scope->parent;
}

cg_obj_t* cg_new_object(cg_t* cg, char* name, bool glb, int offset, type_t* type, fun_t* fun) {
  cg_obj_t* obj = NEW_DATA(cg_obj_t);
  obj->glb = glb;
  obj->offset = offset;
  obj->type = type;
  obj->fun = fun;
  hashmap_t* hm = (glb ? cg->glb_objs : cg->scope->locl_objs);
  hashmap_add(hm, name, obj);
  return obj;
}

cg_obj_t* cg_find_object(cg_t* cg, char* name) {
  cg_obj_t* obj = NULL;
  cg_scope_t* scope = cg->scope;
  while (!obj) {
    obj = hashmap_get(scope->locl_objs, name);
    scope = scope->parent;
    if (scope == NULL) break;
  }
  if (!obj) {
    obj = hashmap_get(cg->glb_objs, name);
  }
  return obj;
}

void cg_stmt(cg_t* cg, node_t* node);
int cg_fn_call(cg_t* cg, node_t* node, bool arg);
int cg_struct_acc(cg_t* cg, cg_obj_t* obj, node_t* node, int off, bool assign);
int cg_get_struct_member(cg_t* cg, node_t* node, type_t* type);
int cg_arr_idx(cg_t* cg, node_t* node);

// Return register of the resulting expression.
int cg_expr(cg_t* cg, node_t* node) {
  int lhs_reg = 0; int rhs_reg = 0; 
  if (node->lhs && node->type != NODE_STRUCT_ACC) lhs_reg = cg_expr(cg, node->lhs);
  if (node->rhs && node->type != NODE_STRUCT_ACC) rhs_reg = cg_expr(cg, node->rhs);
  int reg = 0;
  switch (node->type) {
    case NODE_ADD:
      reg = x86_add(cg, lhs_reg, rhs_reg);
      break;
    case NODE_SUB:
      reg = x86_sub(cg, lhs_reg, rhs_reg);
      break;
    case NODE_MUL:
      reg = x86_mul(cg, lhs_reg, rhs_reg);
      break;
    case NODE_DIV:
    case NODE_MOD:
      reg = x86_div(cg, lhs_reg, rhs_reg, (node->type == NODE_MOD));
      break;
    case NODE_AND:
      reg = x86_and(cg, lhs_reg, rhs_reg);
      break;
    case NODE_OR:
      reg = x86_or(cg, lhs_reg, rhs_reg);
      break;
    case NODE_XOR:
      reg = x86_xor(cg, lhs_reg, rhs_reg);
      break;
    case NODE_EXCL:
      reg = x86_excl(cg, lhs_reg);
      break;
    case NODE_SHL:
    case NODE_SHR:
      reg = x86_sh(cg, lhs_reg, rhs_reg, (node->type == NODE_SHL));
      break;
    case NODE_NOT:
      reg = x86_not(cg, lhs_reg);
      break;
    case NODE_INT:
      reg = x86_load_int(cg, node->value);
      break;
    case NODE_NEG:
      // TODO :)
      break;
    case NODE_ID:
      reg = x86_load_id(cg, parse_str(node->tok), node);
      break;
    case NODE_STR:
      reg = x86_load_str(cg, node->tok);
      break;
    case NODE_FN_CALL:
      reg = cg_fn_call(cg, node, true);
      break;
    case NODE_ARR_IDX: {
      int idx = cg_arr_idx(cg, node);
      reg = x86_idx_arr(cg, idx, parse_str(node->tok), false);
      cg_reg_free(cg, idx);
      break;
    }
    case NODE_REF:
      reg = x86_ref(cg, node, true);
      break;
    case NODE_AT: {
      int ref = x86_ref(cg, node, false);
      reg = cg_reg_alloc(cg);
      fprintf(cg->out, "\tmov %s, [%s]\n", x86_get_reg(cg, reg), x86_get_reg(cg, ref));
      cg_reg_free(cg, ref);
      break;
    }
    case NODE_STRUCT_ACC: {
      // FIXME: Also remember to load globals!
      // TODO: We can move this obj stuff to the cg_struct_acc
      int off = cg_get_struct_member(cg, node, NULL);
      char* name_str = parse_str(node->tok);
      cg_obj_t* obj = cg_find_object(cg, name_str);
      free(name_str);
      reg = cg_struct_acc(cg, obj, node, off, false);
      cg_reg_free(cg, off);
      break;
    }
  }
  return reg;
}

int cg_expr_or_arr(cg_t* cg, node_t* node);

int cg_arr(cg_t* cg, node_t* node) {
  arr_t* arr = (arr_t*)node->data;
  cg->arr_count++;
  int first_off = cg->stack_offset;
  for (list_item_t* item = arr->expr_list->head->next; item != arr->expr_list->head; item = item->next) {
    node_t* expr_or_arr = (node_t*)item->data;
    int reg = cg_expr_or_arr(cg, expr_or_arr);
    if (reg >= 0) {
      fprintf(cg->out, "\tmov %s [rbp%d], %s\n", word_sizes[x86_get_size(cg->current_type)],
        -cg->stack_offset, x86_get_reg(cg, reg));
      cg_reg_free(cg, reg);
      cg->stack_offset -= x86_get_size(cg->current_type);
    }
  }
  cg->arr_count--;
  return -first_off;
}

int cg_expr_or_arr(cg_t* cg, node_t* node) {
  if (node->type == NODE_ARRAY) {
    return cg_arr(cg, node);
  }
  return cg_expr(cg, node);
}

void cg_fn_def(cg_t* cg, node_t* node) {
  fun_t* fun = (fun_t*)node->data;
  if (!fun->initialised) {
    fprintf(cg->out, "[extern %.*s]\n", fun->name->text_len, fun->name->text);
    cg_new_object(cg, parse_str(fun->name), true, 0, fun->type, fun);
    return;
  }
  cg->stack_offset = 0;
  cg->current_func_type = fun->type;
  cg->current_func = fun;
  cg->locl_objs = hashmap_create(50, 5);
  cg->label_cnt = 0;
  cg_new_scope(cg);
  fprintf(cg->out, "[global %.*s]\n", fun->name->text_len, fun->name->text);
  fprintf(cg->out, "%.*s:\n", fun->name->text_len, fun->name->text);
  x86_setup_stack(cg);
  int count = 0;
  for (list_item_t* item = fun->params->head->next; item != fun->params->head; item = item->next) {
    node_t* node = (node_t*)item->data;
    var_t* var = (var_t*)node->data;
    cg->current_type = var->type;
    int offset = x86_new_param(cg, count++, (var->type->is_pointer ? ty_u64 : var->type));
    cg_new_object(cg, parse_str(var->name), false, offset, var->type, NULL);
    cg->reg_bitmap = 0;
  }
  cg_new_object(cg, parse_str(fun->name), true, 0, fun->type, fun);
  for (list_item_t* item = fun->body->head->next; item != fun->body->head; item = item->next) {
    cg_stmt(cg, (node_t*)item->data);
  }
  if (!cg->current_func_ret) {
    x86_end_stack(cg);
    fprintf(cg->out, "\tret\n");
  }
  cg_back_scope(cg);
}

cg_struct_t* cg_define_struct_members(type_t* type) {
  hashmap_t* hm = hashmap_create(50, 5);
  list_t* list = type->type->members;
  int off = 0;
  int arr_size = 0;
  for (list_item_t* item = list->head->next; item != list->head; item = item->next) {
    var_t* var = (var_t*)item->data;
    cg_obj_t* obj = (cg_obj_t*)malloc(sizeof(cg_obj_t));
    obj->glb = false;
    obj->offset = off;
    obj->type = var->type;
    if (var->type->is_pointer) {
      off += 8;
    } else if (var->type->is_arr) {
      type_t* temp = var->type;
      while (temp->pointer) {
        temp = temp->pointer;
        off += var->type->type->size * temp->arr_size->value;
        arr_size += var->type->type->size * temp->arr_size->value;
      }
    } else {
      if (var->type->type->_struct) {
        obj->_struct = cg_define_struct_members(var->type);
      }
      off += var->type->type->size;
    }
    char* pname = parse_str(var->name);
    hashmap_add(hm, pname, obj);
    free(pname);
  }
  cg_struct_t* _struct = (cg_struct_t*)malloc(sizeof(cg_struct_t));
  _struct->members = hm;
  return _struct;
}

cg_struct_t* cg_get_struct(cg_t* cg, type_t* type) {
  cg_struct_t* _struct = hashmap_get(cg->structs, type->type->name);
  if (!_struct) {
    _struct = cg_define_struct_members(type);
    hashmap_add(cg->structs, type->type->name, _struct);
  }
  return _struct;
}

void cg_var_def(cg_t* cg, node_t* node) {
  var_t* var = (var_t*)node->data;
  if (var->glb) {
    // TODO: Check if its an array or string
    char* buf = (char*)malloc(var->name->text_len + 25);
    sprintf(buf, "%.*s: %s %ld\n\n", var->name->text_len, var->name->text,
      dsizes[x86_get_size(var->type)], node->lhs->value);
    list_add(cg->glb_vars, buf);
    cg_new_object(cg, parse_str(var->name), true, 0, var->type, NULL);
    cg->reg_bitmap = 0;
    return;
  }
  cg->current_type = var->type;
  int offset = 0;
  int item_count = 0;
  int reg = 0;
  int stack_off = 0;
  int final_size;
  cg_struct_t* _struct;
  if (var->type->is_arr) {
    final_size = var->type->pointer->arr_size->value;
    int old_size = 0;
    type_t* temp = var->type->pointer;
    while (temp->pointer) {
      temp = temp->pointer;
      final_size *= temp->arr_size->value;
    }
    if (var->initialised) {
      cg->stack_offset += cg->current_type->type->size * final_size;
      stack_off = cg->stack_offset;
    }
  }
  if (var->type->type->_struct) {
    _struct = cg_get_struct(cg, var->type);
  }
  if (var->initialised) {
    reg = cg_expr_or_arr(cg, node->lhs);
    if (node->lhs->type == NODE_ARRAY) {
      cg->stack_offset = stack_off;
    }
  }

  if (!var->type->is_arr) {
    cg->current_type = var->type;
    offset = x86_new_local(cg, reg, var->type, var->initialised);
    cg_reg_free(cg, reg);
  } else {
    if (!var->initialised && var->type->is_arr) {
      cg->stack_offset += cg->current_type->type->size * final_size;
      offset = -cg->stack_offset;
    } else {
      offset = reg;
    }
  }
  cg_new_object(cg, parse_str(var->name), false, offset, var->type, NULL)->_struct = _struct;
  cg->reg_bitmap = 0;
}

void cg_ret(cg_t* cg, node_t* node) {
  cg->current_type = cg->current_func_type;
  if (node->lhs) {
    int reg = cg_expr(cg, node->lhs);
    x86_ret(cg, reg);
    cg_reg_free(cg, reg);
  }
  x86_end_stack(cg);
  fprintf(cg->out, "\tret\n");
  cg->current_func_ret = true;
  cg->reg_bitmap = 0;
}

int cg_fn_call(cg_t* cg, node_t* node, bool arg) {
  funcall_t* fcall = (funcall_t*)node->data;
  char* name = parse_str(fcall->name);
  cg_obj_t* obj = cg_find_object(cg, name);
  list_item_t* param_iterator = obj->fun->params->head->next; // We dont check if its a function cuz parser takes care of that.
  int count = fcall->arguments->size; int pos = 0;
  int align = 0;
  x86_save_used_regs(cg);
  if (count <= 6) {
    for (list_item_t* item = fcall->arguments->head->next; item != fcall->arguments->head; item = item->next) {
      node_t* arg = (node_t*)item->data;
      type_t* param_type = ty_u64;
      if (param_iterator != obj->fun->params->head) {
        node_t* param = (node_t*)param_iterator->data;
        param_type = ((var_t*)param->data)->type;
        param_iterator = param_iterator->next;
      }
      cg->reg_bitmap |= (1 << pos);
      count--;
      cg->current_type = param_type;
      int reg = cg_expr(cg, arg);
      cg->current_type = param_type; // why? idk.
      fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, pos++), x86_get_reg(cg, reg));
    }
  } else {
    pos = fcall->arguments->size - 1;
    param_iterator = obj->fun->params->head->prev;
    align = (fcall->arguments->size - 6) * 8;
    fprintf(cg->out, "\tsub rsp, %d\n", align);
    for (list_item_t* item = fcall->arguments->head->prev; item != fcall->arguments->head; item = item->prev) {
      node_t* arg = (node_t*)item->data;
      type_t* param_type = ty_u64;
      if (param_iterator != obj->fun->params->head) {
        node_t* param = (node_t*)param_iterator->data;
        param_type = ((var_t*)param->data)->type;
        param_iterator = param_iterator->prev;
      }
      if (pos < 6) cg->reg_bitmap |= (1 << pos);
      count--;
      cg->current_type = param_type;
      int reg = cg_expr(cg, arg);
      cg_reg_free(cg, reg);
      cg->current_type = param_type;
      if (pos < 6) {
        fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, pos), x86_get_reg(cg, reg));
      } else {
        fprintf(cg->out, "\tpush %s\n", regs64[reg]);
      }
      pos--;
    }
  }
  int reg = x86_fn_call(cg, name, arg, align);
  free(name);
  if (arg)
    return reg;
  if (reg >= 0) cg_reg_free(cg, reg);
  cg->reg_bitmap = 0;
  return -1;
}

int get_arr_sz(type_t* type) {
  if (type->is_pointer) {
    if (type->pointer) return 8;
    return 1;
  }
  if (type->is_arr) return type->arr_size->value;
  return 8;
}

// Returns the register with the address of the item accessed
int cg_arr_idx(cg_t* cg, node_t* node) {
  list_t* expr_list = (list_t*)node->data;
  cg_obj_t* obj = cg_find_object(cg, parse_str(node->tok));
  type_t* temp = obj->type;
  int reg = -1;
  int to_add = -1;
  uint16_t bitmap = cg->reg_bitmap;
  for (list_item_t* expr = expr_list->head->next; expr != expr_list->head; expr = expr->next) {
    if (temp->pointer && (temp->pointer->is_arr || temp->pointer->is_pointer))
      temp = temp->pointer;
    reg = cg_expr(cg, (node_t*)expr->data); // reg now contains the indexer
    type_t* temp2 = temp->pointer;
    int this_size = -1;
    while (temp2->pointer) {
      int load_size = x86_load_int(cg, get_arr_sz(temp2));
      if (temp2->pointer) {
        int s2nd_size = x86_load_int(cg, get_arr_sz(temp2->pointer));
        this_size = x86_mul(cg, s2nd_size, load_size);
        temp2 = temp2->pointer;
      }
    }
    if (expr->next != expr_list->head) {
      if (this_size == -1) {
        this_size = x86_load_int(cg, get_arr_sz(temp));
      }
      reg = x86_mul(cg, reg, this_size);
    }
    if (to_add >= 0)
      reg = x86_add(cg, reg, to_add);
    if (this_size >= 0)
      cg_reg_free(cg, this_size);
    to_add = reg;
    // Now we should add this indexer to the array
  }
  bitmap |= (1 << reg);
  int size = x86_load_int(cg, obj->type->type->size);
  x86_mul(cg, reg, size);
  cg->reg_bitmap = bitmap;
  return reg;
}

int cg_struct_acc(cg_t* cg, cg_obj_t* obj, node_t* node, int off, bool assign) {
  int ptr = cg_reg_alloc(cg);
  if (obj->type->is_pointer) {
    fprintf(cg->out, "\tlea %s, [rbp%d]\n", x86_get_reg(cg, ptr), obj->offset);
    x86_add(cg, ptr, off);
    if (!assign) {
      fprintf(cg->out, "\tmov %s, [%s]\n", regs64[ptr], regs64[ptr]);
    }
  } else {
    // TODO: What if global?!?!?!?!?!?
    fprintf(cg->out, "\tmov %s, [rbp%d+%s]\n", x86_get_reg(cg, ptr), obj->offset, regs64[off]);
  }
  return ptr;
}

int cg_get_struct_member(cg_t* cg, node_t* node, type_t* type) {
  if (!type) {
    char* name = parse_str(node->tok);
    cg_obj_t* obj = cg_find_object(cg, name);
    free(name);
    type = obj->type;
  }
  cg_struct_t* _struct = cg_get_struct(cg, type);
  char* name = parse_str(node->lhs->tok);
  cg_obj_t* obj = hashmap_get(_struct->members, name);
  int reg = -1;
  if (obj->_struct && !obj->type->is_pointer) {
    reg = cg_get_struct_member(cg, node->lhs, obj->type);
    int off_reg = x86_load_int(cg, obj->offset);
    x86_add(cg, reg, off_reg);
  } else {
    // TODO
  }
  free(name);
  if (reg == -1) {
    reg = x86_load_int(cg, obj->offset);
  }
  return reg;
}

char* cg_get_obj(cg_t* cg, token_t* name, node_t* node) {
  type_t* type;
  char* output = (char*)malloc(40);
  if (node->type == NODE_ARR_IDX) {
    char* name_str = parse_str(node->tok);
    cg_obj_t* obj = cg_find_object(cg, name_str);
    cg->current_type = obj->type;
    type = obj->type;
    int idx = cg_arr_idx(cg, node);
    if (obj->type->is_pointer) {
      // FIXME support 2D+ arrays
      int reg = x86_idx_arr(cg, idx, name_str, true);
      sprintf(output, "[%s]", regs64[reg]);
    } else {
      sprintf(output, "[rbp%d+%s]", obj->offset, regs64[idx]);
    }
    free(name_str);
  } else if (node->type == NODE_ID) {
    char* name_str = parse_str(node->tok);
    cg_obj_t* obj = cg_find_object(cg, name_str);
    cg->current_type = obj->type;
    type = obj->type;
    if (obj->glb) {
      if (node->tok->text_len > 40) {
        output = realloc(output, name->text_len + 6);
      }
      sprintf(output, "[%s]", name_str);
    } else {
      sprintf(output, "[rbp%d]", obj->offset);
    }
    free(name_str);
  } else if (node->type == NODE_AT) {
    int reg = x86_ref(cg, node, false);
    sprintf(output, "[%s]", x86_get_reg(cg, reg));
  } else if (node->type == NODE_STRUCT_ACC) {
    // FIXME: Check if var is global
    char* name_str = parse_str(node->tok);
    cg_obj_t* obj = cg_find_object(cg, name_str);
    int reg = cg_get_struct_member(cg, node, NULL);
    if (obj->type->is_pointer) {
      int ptr = cg_struct_acc(cg, obj, node, reg, true);
      sprintf(output, "[%s]", regs64[ptr]);
      cg_reg_free(cg, reg);
    } else {
      sprintf(output, "[rbp%d+%s]", obj->offset, regs64[reg]);
    }
    free(name_str);
  }
  return output;
}

void cg_assign(cg_t* cg, node_t* node) {
  type_t* type;
  char* output = cg_get_obj(cg, node->tok, node->lhs);
  type = cg->current_type;
  int expr = cg_expr(cg, node->rhs);
  switch (node->type) {
    case NODE_ASSIGN:
      fprintf(cg->out, "\tmov %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
    case NODE_ASADD:
      fprintf(cg->out, "\tadd %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
    case NODE_ASSUB:
      fprintf(cg->out, "\tsub %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
    case NODE_ASMUL: {
      int reg = cg_reg_alloc(cg);
      fprintf(cg->out, "\tmov %s, %s %s\n", x86_get_reg(cg, reg), word_sizes[x86_get_size(type)], output);
      fprintf(cg->out, "\timul %s, %s\n", x86_get_reg(cg, reg), x86_get_reg(cg, expr));
      fprintf(cg->out, "\tmov %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, reg));
      cg_reg_free(cg, reg);
      break;
    }
    case NODE_ASDIV: case NODE_ASMOD: {
      int reg = cg_reg_alloc(cg);
      fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, reg), x86_get_reg(cg, 2));
      fprintf(cg->out, "\tmov %s, %s %s\n", x86_get_ax(type), word_sizes[x86_get_size(type)], output);
      fprintf(cg->out, "\tcqo\n");
      fprintf(cg->out, "\txor %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, 2));
      fprintf(cg->out, "\tidiv %s\n", x86_get_reg(cg, expr));
      if (node->type != NODE_ASMOD) {
        fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, reg));
        fprintf(cg->out, "\tmov %s %s, %s\n", word_sizes[x86_get_size(type)],
          output, x86_get_ax(type));
      } else {
        fprintf(cg->out, "\tmov %s %s, %s\n", word_sizes[x86_get_size(type)],
          output, x86_get_reg(cg, 2));
        fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, reg));
      }
      cg_reg_free(cg, reg);
      break;
    }
    case NODE_ASSHL: case NODE_ASSHR: {
      int temp = 0;
      bool rstor = false;
      if (expr != 3) {
        if ((cg->reg_bitmap & (1 << 3)) > 0) {
          // RCX is being used
          temp = cg_reg_alloc(cg);
          fprintf(cg->out, "\tmov %s, rcx\n", regs64[temp]);
          rstor = true;
        }
        fprintf(cg->out, "\tmov rcx, %s\n", regs64[expr]);
      }
      fprintf(cg->out, "\t%s %s %s, cl\n", ((node->type == NODE_ASSHL) ? "shl" : "shr"), 
        word_sizes[x86_get_size(type)], output);
      if (rstor) {
        fprintf(cg->out, "\tmov rcx, %s\n", regs64[temp]);
        cg_reg_free(cg, rstor);
      }
      break;
    }
    case NODE_ASOR:
      fprintf(cg->out, "\tor %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
    case NODE_ASAND:
      fprintf(cg->out, "\tand %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
    case NODE_ASXOR:
      fprintf(cg->out, "\txor %s %s, %s\n", word_sizes[x86_get_size(type)],
       output, x86_get_reg(cg, expr));
      break;
  }
  free(output);
  cg_reg_free(cg, expr);
  cg->reg_bitmap = 0;
}

char* cg_cond(cg_t* cg, node_t* cond, bool invert) {
  // TODO: Implement the rest of conditions
  static char* op_invert[] = {"jne", "jle", "jl", "jge", "jg", "!", "je", "&", "&&", "|", "||"};
  static char* op[] = {"je", "jg", "jge", "jl", "jle", "!", "jne", "&", "&&", "|", "||"};
  if (cond->type >= NODE_EQEQ && cond->type <= NODE_DBOR && cond->type != NODE_NOT) {
    int reg1 = cg_expr(cg, cond->lhs);
    int reg2 = cg_expr(cg, cond->rhs);
    fprintf(cg->out, "\tcmp %s, %s\n", x86_get_reg(cg, reg1), x86_get_reg(cg, reg2));
    cg->reg_bitmap = 0;
    return (invert ? op_invert : op)[cond->type - NODE_EQEQ];
  } else {
    int reg1 = cg_expr(cg, cond);
    cg->reg_bitmap = 0;
    fprintf(cg->out, "\tcmp %s, 0\n", x86_get_reg(cg, reg1));
    return (invert ? "jle" : "jg");
  }
}

void cg_for_loop(cg_t* cg, node_t* node) {
  for_stmt_t* stmt = (for_stmt_t*)node->data;
  cg_new_scope(cg);
  if (stmt->primary_stmt->type == NODE_VAR_DEF) cg_var_def(cg, stmt->primary_stmt);
  else cg_assign(cg, stmt->primary_stmt);
  int next_lbl = cg->label_cnt++;
  int stmt_lbl = cg->label_cnt++;
  fprintf(cg->out, "\tjmp .L%d\n", next_lbl);
  fprintf(cg->out, ".L%d:\n", stmt_lbl);
  cg_stmt(cg, stmt->body);
  cg_assign(cg, stmt->step);
  fprintf(cg->out, ".L%d:\n", next_lbl);
  char* cond = cg_cond(cg, node->lhs, false);
  fprintf(cg->out, "\t%s .L%d\n", cond, stmt_lbl);
  cg_back_scope(cg);
  cg->reg_bitmap = 0;
}

void cg_if_stmt(cg_t* cg, node_t* node) {
  if_stmt_t* stmt = (if_stmt_t*)node->data;
  char* cond = cg_cond(cg, node->lhs, true);
  int false_lbl = cg->label_cnt++;
  int end_lbl = false_lbl;
  if (stmt->false_stmt) {
    end_lbl = cg->label_cnt++;
  }
  fprintf(cg->out, "\t%s .L%d\n", cond, false_lbl);
  cg_new_scope(cg);
  cg_stmt(cg, stmt->true_stmt);
  cg_back_scope(cg);
  if (stmt->false_stmt) {
    fprintf(cg->out, "\tjmp .L%d\n", end_lbl);
  }
  fprintf(cg->out, ".L%d:\n", false_lbl);
  if (stmt->false_stmt) {
    cg_new_scope(cg);
    cg_stmt(cg, stmt->false_stmt);
    cg_back_scope(cg);
    fprintf(cg->out, ".L%d:\n", end_lbl);
  }
}

void cg_while_loop(cg_t* cg, node_t* node) {
  while_stmt_t* stmt = (while_stmt_t*)node->data;
  int stmt_lbl = cg->label_cnt++;
  int end_lbl = cg->label_cnt++;
  fprintf(cg->out, "\tjmp .L%d\n", end_lbl);
  fprintf(cg->out, ".L%d:\n", stmt_lbl);
  cg_new_scope(cg);
  cg_stmt(cg, stmt->body);
  cg_back_scope(cg);
  fprintf(cg->out, ".L%d:\n", end_lbl);
  char* cond = cg_cond(cg, node->lhs, false);
  fprintf(cg->out, "\t%s .L%d\n", cond, stmt_lbl);
  cg->reg_bitmap = 0;
}

void cg_stmt(cg_t* cg, node_t* node) {
  switch (node->type) {
    case NODE_COMPOUND: {
      list_t* ast = (list_t*)node->data;
      for (list_item_t* item = ast->head->next; item != ast->head; item = item->next) {
        node_t* node = (node_t*)item->data;
        cg_stmt(cg, node);
      }
      break;
    }
    case NODE_FN_DEF:
      cg_fn_def(cg, node);
      break;
    case NODE_VAR_DEF:
      cg_var_def(cg, node);
      break;
    case NODE_RET:
      cg_ret(cg, node);
      break;
    case NODE_FN_CALL:
      cg_fn_call(cg, node, false);
      break;
    case NODE_ASSIGN...NODE_ASXOR:
      cg_assign(cg, node);
      break;
    case NODE_FOR:
      cg_for_loop(cg, node);
      break;
    case NODE_IF:
      cg_if_stmt(cg, node);
      break;
    case NODE_WHILE:
      cg_while_loop(cg, node);
      break;
  }
}

void cg_gen(cg_t* cg) {
  fprintf(cg->out, "[default rel]\n[section .text]\n");
  for (list_item_t* item = cg->ast->head->next; item != cg->ast->head; item = item->next) {
    node_t* node = (node_t*)item->data;
    cg_stmt(cg, node);
  }
  int i = 0;
  fprintf(cg->out, "[section .data]\n");
  for (list_item_t* item = cg->glb_vars->head->next; item != cg->glb_vars->head; item = item->next) {
    fprintf(cg->out, "%s", (char*)item->data);
  }
  for (list_item_t* item = cg->strings->head->next; item != cg->strings->head; item = item->next) {
    fprintf(cg->out, "LC%d: db `%s`,0\n", i, (char*)item->data);
    i++;
  }
  fclose(cg->out);
}
