#pragma once

#include <stdint.h>
#include <stddef.h>
#include "cg.h"

#define MAX_REGS 12

extern const char* regs8[];
extern const char* regs16[];
extern const char* regs32[];
extern const char* regs64[];
extern const char* dsizes[];
extern const char* word_sizes[];

int x86_get_size(type_t* type);
char* x86_get_reg(cg_t* cg, int reg);
char* x86_get_ax(type_t* type);
int x86_new_local(cg_t* cg, int reg, type_t* type, bool initialised);
int x86_load_int(cg_t* cg, int64_t val);
int x86_load_id(cg_t* cg, char* name, node_t* node);
int x86_load_str(cg_t* cg, token_t* tok);
int x86_idx_arr(cg_t* cg, int reg, char* name, bool assign);
int x86_fn_call(cg_t* cg, char* name, bool alloc);
int x86_ref(cg_t* cg, node_t* expr, bool lea);
int x86_add(cg_t* cg, int reg_1, int reg_2);
int x86_sub(cg_t* cg, int reg_1, int reg_2);
int x86_mul(cg_t* cg, int reg_1, int reg_2);
int x86_div(cg_t* cg, int reg_1, int reg_2, bool mod);
int x86_and(cg_t* cg, int reg_1, int reg_2);
int x86_or(cg_t* cg, int reg_1, int reg_2);
int x86_xor(cg_t* cg, int reg_1, int reg_2);
int x86_excl(cg_t* cg, int reg_1);
int x86_not(cg_t* cg, int reg_1);
int x86_sh(cg_t* cg, int reg_1, int reg_2, bool shl);
void x86_ret(cg_t* cg, int reg);
void x86_save_used_regs(cg_t* cg);
void x86_restore_used_regs(cg_t* cg);
int x86_new_param(cg_t* cg, int param, type_t* type);
void x86_setup_stack(cg_t* cg);
void x86_end_stack(cg_t* cg);