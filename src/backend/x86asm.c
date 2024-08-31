#include "x86asm.h"
#include "../frontend/type_checker.h"

const char* regs64[] = {"rdi", "rsi", "rdx", "rcx","r8", "r9", "r10",
  "r11", "r12", "r13", "r14", "r15"};

const char* regs32[] = {"edi", "esi", "edx", "ecx","r8d", "r9d", "r10d",
  "r11d", "r12d", "r13d", "r14d", "r15d"};

const char* regs16[] = {"di", "si", "dx", "cx","r8w", "r9w", "r10w",
  "r11w", "r12w", "r13w", "r14w", "r15w"};

const char* regs8[] = {"dil", "sil", "dl", "cl","r8b", "r9b", "r10b",
  "r11b", "r12b", "r13b", "r14b", "r15b"};

const char* dsizes[] = {
  [1] = "db",
  [2] = "dw",
  [4] = "dd",
  [8] = "dq"
};

const char* word_sizes[] = {
  [1] = "byte",
  [2] = "word",
  [4] = "dword",
  [8] = "qword"
};

int x86_get_size(type_t* type) {
  if (type->is_pointer) return 8;
  return type->type->size;
}

char* x86_get_reg(cg_t* cg, int reg) {
  switch (x86_get_size(cg->current_type)) {
    case 1:
      return regs8[reg];
      break;
    case 2:
      return regs16[reg];
      break;
    case 4:
      return regs32[reg];
      break;
    case 8:
      return regs64[reg];
      break;
  }
}

char* x86_get_reg_by_type(type_t* type, int reg) {
  switch (x86_get_size(type)) {
    case 1:
      return regs8[reg];
      break;
    case 2:
      return regs16[reg];
      break;
    case 4:
      return regs32[reg];
      break;
    case 8:
      return regs64[reg];
      break;
  }
}

char* x86_get_ax(type_t* type) {
  char* ax;
  switch (x86_get_size(type)) {
    case 1:
      ax = "al";
      break;
    case 2:
      ax = "ax";
      break;
    case 4:
      ax = "eax";
      break;
    case 8:
      ax = "rax";
      break;
  }
  return ax;
}

char* x86_get_mov64(type_t* type) {
  char* mov;
  switch (x86_get_size(type)) {
    case 1:
      mov = "movzx";
      break;
    case 2:
      mov = "movzx";
      break;
    case 4:
      mov = "movsxd";
      break;
    case 8:
      mov = "mov";
      break;
  }
  return mov;
}

int x86_new_local(cg_t* cg, int reg, type_t* type, bool initialised) {
  int offset = cg_new_offset(cg, type);
  fprintf(cg->out, "\tmov %s [rbp%d], %s\n", word_sizes[x86_get_size(type)],
    offset, (initialised ? x86_get_reg(cg, reg) : "0"));
  return offset;
}

int x86_load_int(cg_t* cg, int64_t val) {
  int reg = cg_reg_alloc(cg);
  if (cg->current_type == NULL) {
    cg->current_type = ty_u64;
  }
  fprintf(cg->out, "\tmov %s, %ld\n", x86_get_reg(cg, reg), val);
  return reg;
}

int x86_load_id(cg_t* cg, char* name, node_t* node) {
  int reg = cg_reg_alloc(cg);
  cg_obj_t* obj = (cg_obj_t*)cg_find_object(cg, name);
  if (cg->current_type == NULL) {
    cg->current_type = obj->type;
  }
  if (obj->glb) {
    fprintf(cg->out, "\tmov %s, [%s]\n", x86_get_reg(cg, reg), name);
  } else if (obj->type->is_arr) {
    char* output = cg_get_obj(cg, node->tok, node);
    fprintf(cg->out, "\tlea %s, %s\n", regs64[reg], output);
    free(output);
  } else {
    if (x86_get_size(obj->type) < x86_get_size(cg->current_type)) {
      fprintf(cg->out, "\t%s %s, %s [rbp%d]\n", x86_get_mov64(obj->type),
        x86_get_reg(cg, reg), word_sizes[x86_get_size(obj->type)], obj->offset);
    } else {
      fprintf(cg->out, "\tmov %s, %s [rbp%d]\n",
        x86_get_reg_by_type(obj->type, reg), word_sizes[x86_get_size(obj->type)], obj->offset);
      if (x86_get_size(obj->type) < x86_get_size(cg->current_type)) {
        fprintf(cg->out, "\tcdqe\n");
      }
    }
  }
  return reg;
}

int x86_load_str(cg_t* cg, token_t* tok) {
  char* key = parse_str(tok);
  int idx = 0;
  bool found = false;
  for (list_item_t* item = cg->strings->head->next; item != cg->strings->head; item = item->next) {
    if (hashmap_hash((char*)item->data) == hashmap_hash(key)) {
      found = true;
      break;
    }
    idx++;
  }
  if (!found) {
    idx = cg->strings->size;
    list_add(cg->strings, key);
  }
  int reg = cg_reg_alloc(cg);
  fprintf(cg->out, "\tlea %s, [LC%d]\n", regs64[reg], idx);
  return reg;
}

int x86_idx_arr(cg_t* cg, int reg, char* name, bool assign) {
  cg_obj_t* obj = (cg_obj_t*)cg_find_object(cg, name);
  int res = cg_reg_alloc(cg);
  // TODO: What if object is global?
  if (obj->type->is_arr) {
    if (obj->glb) {
      printf("TODO!\n");
      exit(1);
    }
    fprintf(cg->out, "\tmov %s, %s [rbp%d+%s]\n", x86_get_reg(cg, res), word_sizes[x86_get_size(obj->type)],
      obj->offset, x86_get_reg(cg, reg));
  } else {
    // its a pointer
    int ptr = cg_reg_alloc(cg);
    int scale = cg_reg_alloc(cg);
    fprintf(cg->out, "\tmov %s, %d\n", x86_get_reg(cg, scale), obj->type->type->size);
    int mul = x86_mul(cg, reg, scale);
    if (x86_get_size(cg->current_type) < 8) {
      fprintf(cg->out, "\tmovsx %s, %s\n", regs64[mul], x86_get_reg_by_type(cg->current_type, mul));
    }
    if (mul != reg) {
      cg_reg_free(cg, reg);
      reg = mul;
    }
    cg_reg_free(cg, scale);
    fprintf(cg->out, "\tmov %s, qword [rbp%d]\n", regs64[ptr], obj->offset);
    fprintf(cg->out, "\tadd %s, %s\n", regs64[ptr], regs64[reg]);
    if (!assign) {
      fprintf(cg->out, "\t%s %s, %s [%s]\n", x86_get_mov64(obj->type->pointer), regs64[res],
        word_sizes[x86_get_size(obj->type->pointer)], regs64[ptr]);
      cg_reg_free(cg, ptr);
    } else {
      cg_reg_free(cg, res);
      res = ptr;
    }
  }
  return res;
}

int x86_fn_call(cg_t* cg, char* name, bool alloc) {
  cg_obj_t* obj = (cg_obj_t*)cg_find_object(cg, name);
  fprintf(cg->out, "\tcall %s\n", name);
  if (x86_get_size(obj->type) == 0) return -1;
  int reg = cg_reg_alloc(cg);
  fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg_by_type(obj->type, reg), x86_get_ax(obj->type));
  return reg;
}

// lea/mov reg, [base + idx * scale]
int x86_ref(cg_t* cg, node_t* expr, bool lea) {
  int reg = cg_reg_alloc(cg);
  if (expr->lhs->type == NODE_ID) {
    char* name = parse_str(expr->lhs->tok);
    cg_obj_t* obj = (cg_obj_t*)cg_find_object(cg, name);
    char* output = cg_get_obj(cg, expr->lhs->tok, expr->lhs);
    fprintf(cg->out, "\t%s %s, %s\n", (lea ? "lea" : "mov"), regs64[reg], output);
    free(name);
    return reg;
  } else {
    // It's an addition or sub fs
    int rhs = cg_expr(cg, expr->lhs->rhs);
    char* name = parse_str(expr->lhs->lhs->tok);
    cg_obj_t* obj = (cg_obj_t*)cg_find_object(cg, name);
    char* output = cg_get_obj(cg, expr->lhs->lhs->tok, expr->lhs->lhs);
    fprintf(cg->out, "\t%s %s, %s\n", (lea ? "lea" : "mov"), regs64[reg], output);
    fprintf(cg->out, "\timul %s, %d\n", regs64[rhs], obj->type->type->size);
    if (expr->lhs->type == NODE_SUB) {
      fprintf(cg->out, "\tsub %s, %s\n", regs64[reg], regs64[rhs]);
    } else {
      fprintf(cg->out, "\tadd %s, %s\n", regs64[reg], regs64[rhs]);
    }
    cg_reg_free(cg, rhs);
    return reg;
  }
}

int x86_add(cg_t* cg, int reg_1, int reg_2) {
  fprintf(cg->out, "\tadd %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_sub(cg_t* cg, int reg_1, int reg_2) {
  fprintf(cg->out, "\tsub %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_mul(cg_t* cg, int reg_1, int reg_2) {
  if (cg->current_type->type->size == 1) {
    // AX:= AL ∗ r/m byte.
    int c1 = cg_reg_alloc(cg);
    int c2 = cg_reg_alloc(cg);
    fprintf(cg->out, "\tmovzx %s, %s\n", regs64[c1], regs8[reg_1]);
    fprintf(cg->out, "\tmovzx %s, %s\n", regs64[c2], regs8[reg_2]);
    fprintf(cg->out, "\timul %s, %s\n", regs64[c1], regs64[c2]);
    cg_reg_free(cg, reg_1);
    cg_reg_free(cg, c2);
    reg_1 = c1;
  } else {
    fprintf(cg->out, "\timul %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  }
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_div(cg_t* cg, int reg_1, int reg_2, bool mod) {
  int temp = cg_reg_alloc(cg); int div_reg = (reg_2 == 2 ? temp : reg_2);
  fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, temp), x86_get_reg(cg, 2));
  fprintf(cg->out, "\tmov %s, %s\n", x86_get_ax(cg->current_type), x86_get_reg(cg, reg_1));
  fprintf(cg->out, "\tcqo\n");
  fprintf(cg->out, "\txor %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, 2));
  fprintf(cg->out, "\tidiv %s\n", x86_get_reg(cg, div_reg));
  if (!mod) {
    fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, temp));
    fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, reg_2), x86_get_ax(cg->current_type));
  } else {
    fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, reg_2), x86_get_reg(cg, 2));
    fprintf(cg->out, "\tmov %s, %s\n", x86_get_reg(cg, 2), x86_get_reg(cg, temp));
  }
  cg_reg_free(cg, reg_1);
  cg_reg_free(cg, temp);
  return reg_2;
}

int x86_and(cg_t* cg, int reg_1, int reg_2) {
  fprintf(cg->out, "\tand %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_or(cg_t* cg, int reg_1, int reg_2) {
  fprintf(cg->out, "\tor %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_xor(cg_t* cg, int reg_1, int reg_2) {
  fprintf(cg->out, "\txor %s, %s\n", x86_get_reg(cg, reg_1), x86_get_reg(cg, reg_2));
  cg_reg_free(cg, reg_2);
  return reg_1;
}

int x86_excl(cg_t* cg, int reg_1) {
  fprintf(cg->out, "\tcmp %s, 0\n", x86_get_reg(cg, reg_1));
  fprintf(cg->out, "\tsete al\n");
  fprintf(cg->out, "\tmovzx %s, al\n", regs64[reg_1]);
  return reg_1;
}

int x86_not(cg_t* cg, int reg_1) {
  fprintf(cg->out, "\tnot %s\n", x86_get_reg(cg, reg_1));
  return reg_1;
}

int x86_sh(cg_t* cg, int reg_1, int reg_2, bool shl) {
  int temp = 0;
  bool rstor = false;
  if (reg_2 != 3) {
    if ((cg->reg_bitmap & (1 << 3)) > 0) {
      // RCX is being used
      temp = cg_reg_alloc(cg);
      fprintf(cg->out, "\tmov %s, rcx\n", regs64[temp]);
      rstor = true;
    }
    fprintf(cg->out, "\tmov rcx, %s\n", regs64[reg_2]);
  }
  fprintf(cg->out, "\t%s %s, cl\n", (shl ? "shl" : "shr"), x86_get_reg(cg, reg_1));
  cg_reg_free(cg, reg_2);
  if (rstor) {
    fprintf(cg->out, "\tmov rcx, %s\n", regs64[temp]);
    cg_reg_free(cg, rstor);
  }
  return reg_1;
}

void x86_ret(cg_t* cg, int reg) {
  fprintf(cg->out, "\tmov %s, %s\n", x86_get_ax(cg->current_func_type), x86_get_reg(cg, reg));
}

void x86_save_used_regs(cg_t* cg) {
  int i = 0;
  int count = 0;
  for (; i < MAX_REGS; i++) {
    if ((cg->reg_bitmap & (1 << i)) != 0) {
      stack_push(cg->reg_stack, i);
      count++;
      fprintf(cg->out, "\tpush %s\n", regs64[i]);
    }
  }
  stack_push(cg->reg_stack, count);
  cg->reg_bitmap = 0;
}

void x86_restore_used_regs(cg_t* cg) {
  int count = stack_pop(cg->reg_stack);
  for (; count != 0; count--) {
    int reg = stack_pop(cg->reg_stack);
    fprintf(cg->out, "\tpop %s\n", regs64[reg]);
    cg->reg_bitmap |= (1 << reg);
  }
}

int x86_new_param(cg_t* cg, int param, type_t* type) {
  int offset = cg_new_offset(cg, type);
  if (param > 5) {
    int temp = cg_reg_alloc(cg);
    fprintf(cg->out, "\tmov %s, %s [rbp+%d]\n", x86_get_reg(cg, temp), word_sizes[x86_get_size(type)],
      ((param - 6) * 8) + 16);
    param = temp;
  }
  fprintf(cg->out, "\tmov %s [rbp%d], %s\n", word_sizes[x86_get_size(type)], offset,
    x86_get_reg(cg, param));
  return offset;
}

void x86_setup_stack(cg_t* cg) {
  fprintf(cg->out, "\tpush rbp\n");
  fprintf(cg->out, "\tmov rbp, rsp\n");
  int size = 0;
  for (list_item_t* item = cg->current_func->objs->head->next; item != cg->current_func->objs->head;
    item = item->next) {
    object_t* obj = (object_t*)item->data;
    if (obj->type->is_arr) {
      type_t* temp = obj->type;
      int final_size = temp->pointer->arr_size->value;
      int old_size = 0;
      temp = temp->pointer;
      while (temp->pointer) {
        temp = temp->pointer;
        final_size *= temp->arr_size->value;
      }
      size += (final_size * obj->type->type->size);
      continue;
    }
    size += x86_get_size(obj->type);
  }
  if (size > 0) fprintf(cg->out, "\tsub rsp, %d\n", (size + 15) & ~15 /* align to nearest multiple of 16 */);
}

void x86_end_stack(cg_t* cg) {
  fprintf(cg->out, "\tmov rsp, rbp\n");
  fprintf(cg->out, "\tpop rbp\n");
}