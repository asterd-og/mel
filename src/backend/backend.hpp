#pragma once

#include <map>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
extern "C" {
#include "../frontend/parser.h"
}

typedef struct backend_scope_t {
  struct backend_scope_t* parent;
  std::map<std::string, llvm::Value*> var_map;
  std::map<std::string, type_t*> var_types;
} backend_scope_t;