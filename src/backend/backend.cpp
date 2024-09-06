#include <iostream>
#include <fstream>
#include <map>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
extern "C" {
#include "backend.h"
#include "../frontend/parser.h"
}

using namespace llvm;

LLVMContext context;
Module module("module", context);
IRBuilder<> builder(context);

Function* current_fn = nullptr;
bool fn_ret = false;
bool inside_scope = false;
std::map<std::string, Function*> fn_map;
std::map<std::string, Value*> var_map;
std::map<std::string, Type*> type_map; // structs, etc...

void backend_gen_stmt(node_t* node);
Value* backend_gen_fn_call(node_t* node);
Type* backend_get_llvm_type(type_t* ty);
Value* backend_gen_expr(Type* ty, node_t* node);

Type* backend_gen_struct(type_t* ty) {
  std::vector<Type *> fields;
  for (list_item_t* item = ty->type->members->head->next; item != ty->type->members->head; item = item->next) {
    var_t* var = (var_t*)item->data;
    fields.push_back(backend_get_llvm_type(var->type));
  }
  auto* type = StructType::create(context, "struct." + std::string(ty->type->name));
  type->setBody(fields);
  type_map[ty->type->name] = type;
  return type;
}

Type* backend_get_llvm_type(type_t* ty) {
  Type* bt = nullptr;
  if (ty->type->_struct) {
    auto query = type_map.find(std::string(ty->type->name));
    if (query == type_map.end()) {
      bt = backend_gen_struct(ty);
    } else {
      bt = query->second;
    }
  } else {
    switch (ty->type->size) {
      case 0:
        bt = Type::getVoidTy(context);
        break;
      case 1:
        bt = Type::getInt8Ty(context);
        break;
      case 2:
        bt = Type::getInt16Ty(context);
        break;
      case 4:
        bt = Type::getInt32Ty(context);
        break;
      case 8:
        bt = Type::getInt64Ty(context);
        break;
    }
  }
  if (ty->is_pointer) {
    Type* temp_ty = bt;
    for (int i = 0; i < ty->ptr_cnt; i++) {
      temp_ty = temp_ty->getPointerTo();
    }
    bt = temp_ty;
  }
  if (ty->is_arr) {
    type_t* temp = ty;
    Type* temp_ty = bt;
    while (temp->is_arr) {
      temp = temp->pointer;
      temp_ty = ArrayType::get(temp_ty, temp->arr_size->value);
    }
    bt = temp_ty;
  }
  return bt;
}

Value* backend_load_int(Type* ty, int64_t val) {
  return ConstantInt::get(ty, val);
}

std::string unescape_string(const std::string& input) {
  std::string output;
  output.reserve(input.size()); // Reserve enough space to avoid multiple allocations
  
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == '\\' && i + 1 < input.size()) {
      char nextChar = input[i + 1];
      switch (nextChar) {
        case 'n': output += '\n'; ++i; break;
        case 't': output += '\t'; ++i; break;
        case 'r': output += '\r'; ++i; break;
        case '\\': output += '\\'; ++i; break;
        case '"': output += '"'; ++i; break;
        default: output += c; break;
      }
    } else {
      output += c;
    }
  }
  
  return output;
}

Value* backend_load_str(Type* ty, char* str, int len) {
  std::vector<llvm::Constant *> chars(len);
  std::string treated_str = unescape_string(str);
  for(unsigned int i = 0; i < len; i++) {
    chars[i] = ConstantInt::get(builder.getInt8Ty(), treated_str[i]);
  }
  auto init = ConstantArray::get(ArrayType::get(builder.getInt8Ty(), chars.size()),
                                 chars);
  GlobalVariable * v = new GlobalVariable(module, init->getType(), true, GlobalVariable::ExternalLinkage,
    init,
    str);
  return ConstantExpr::getBitCast(v, ty);
}

Type* backend_get_var_type(Value* var) {
  if (isa<AllocaInst>(var)) {
    return ((AllocaInst*)var)->getAllocatedType();
  } else {
    return ((Argument*)var)->getType();
  }
}

Value* backend_load_id(Type* ty, char* name) {
  Value* var = var_map.find(std::string(name))->second;
  if (ty == nullptr) {
    ty = backend_get_var_type(var);
  }
  if (backend_get_var_type(var)->isArrayTy()) {
    var = builder.CreateGEP(backend_get_var_type(var), var, { builder.getInt64(0), builder.getInt64(0) });
  } else if (isa<AllocaInst>(var)) {
    var = builder.CreateLoad(ty, var);
  }
  return var;
}

Value* backend_arr_idx(Type* ty, node_t* node, Value* var, bool store, Value* store_val) {
  Value* alloca = var;
  list_t* expr_list = (list_t*)node->data;
  Value* idx;
  Type* arr_type = backend_get_var_type(alloca);
  Value* pointer = nullptr;
  bool is_pointer = false;
  if (arr_type->isPointerTy()) {
    is_pointer = true;
    pointer = builder.CreateLoad(arr_type, alloca);
    arr_type = arr_type->getPointerElementType();
  }
  Value* element_ptr;
  for (list_item_t* expr = expr_list->head->next; expr != expr_list->head; expr = expr->next) {
    idx = backend_gen_expr(nullptr, (node_t*)expr->data);
    if (is_pointer) {
      element_ptr = builder.CreateGEP(arr_type, pointer, idx);
    } else {
      element_ptr = builder.CreateGEP(arr_type, alloca, { builder.getInt64(0), idx });
    }
    alloca = element_ptr;
    if (arr_type->isArrayTy()) {
      arr_type = arr_type->getArrayElementType();
    } else {
      if (expr->next != expr_list->head) {
        arr_type = arr_type->getPointerElementType();
        pointer = builder.CreateLoad(arr_type, alloca); // TODO: Test this later.
      }
    }
  }
  Value* val;
  if (!store) {
    val = builder.CreateLoad(arr_type, element_ptr);
  } else {
    val = builder.CreateStore(store_val, element_ptr);
  }
  return val;
}

Value* backend_gen_expr(Type* ty, node_t* node) {
  Value *lhs, *rhs;
  Value* val;
  if (node->lhs) lhs = backend_gen_expr(ty, node->lhs);
  if (node->rhs) rhs = backend_gen_expr(ty, node->rhs);
  switch (node->type) {
    case NODE_ADD:
      val = builder.CreateAdd(lhs, rhs);
      break;
    case NODE_SUB:
      val = builder.CreateSub(lhs, rhs);
      break;
    case NODE_MUL:
      val = builder.CreateMul(lhs, rhs);
      break;
    case NODE_DIV:
      val = builder.CreateUDiv(lhs, rhs);
      break;
    case NODE_INT:
      if (ty == nullptr) ty = Type::getInt32Ty(context);
      val = backend_load_int(ty, node->value);
      break;
    case NODE_STR: {
      char* str = parse_str(node->tok);
      if (ty == nullptr)
        ty = PointerType::getInt8PtrTy(context);
      val = backend_load_str(ty, str, node->tok->text_len + 1);
      free(str);
      break;
    }
    case NODE_ID: {
      char* str = parse_str(node->tok);
      val = backend_load_id(ty, str);
      free(str);
      break;
    }
    case NODE_FN_CALL:
      val = backend_gen_fn_call(node);
      if (ty == nullptr)
        ty = val->getType();
      break;
    case NODE_ARR_IDX: {
      char* str = parse_str(node->tok);
      Value* var = var_map.find(std::string(str))->second;
      val = backend_arr_idx(ty, node, var, false, nullptr);
      free(str);
      break;
    }
  }
  return val;
}

void backend_gen_arr(Value* alloca, Type* arr_ty, node_t* node) {
  arr_t* arr = (arr_t*)node->data;
  int i = 0;
  for (list_item_t* item = arr->expr_list->head->next; item != arr->expr_list->head; item = item->next) {
    node_t* expr = (node_t*)item->data;
    Value* val;
    if (expr->type == NODE_ARRAY) {
      Value* element_ptr = builder.CreateGEP(arr_ty, alloca, { builder.getInt64(0), builder.getInt64(i) } );
      backend_gen_arr(element_ptr, arr_ty->getArrayElementType(), expr);
    } else {
      val = backend_gen_expr(arr_ty->getArrayElementType(), expr);
      Value* element_ptr = builder.CreateGEP(arr_ty, alloca, { builder.getInt64(0), builder.getInt64(i) } );
      builder.CreateStore(val, element_ptr);
    }
    i++;
  }
}

void backend_gen_fn_def(node_t* node) {
  fun_t* fn_node = (fun_t*)node->data;
  std::vector<Type*> params;
  bool vararg = false;
  for (list_item_t* param = fn_node->params->head->next; param != fn_node->params->head; param = param->next) {
    node_t* node = (node_t*)param->data;
    if (node->type == NODE_3DOT) {
      vararg = true;
      break;
    }
    params.push_back(backend_get_llvm_type(((var_t*)node->data)->type));
  }
  FunctionType* fn_type = FunctionType::get(backend_get_llvm_type(fn_node->type), params, vararg);
  char* name = parse_str(fn_node->name);
  Function* fn = Function::Create(fn_type, Function::ExternalLinkage, name, &module);
  fn_map[std::string(name)] = fn;
  free(name);
  if (!fn_node->initialised) return;
  fn_ret = false;
  current_fn = fn;
  BasicBlock* entry = BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  auto param_iterator = fn->arg_begin();
  for (list_item_t* param = fn_node->params->head->next; param != fn_node->params->head; param = param->next) {
    node_t* node = (node_t*)param->data;
    Value* val = &*param_iterator++;
    var_t* var = (var_t*)node->data;
    char* pname = parse_str(var->name);
    val->setName(pname);
    if (var->type->is_pointer) {
      AllocaInst* addr = builder.CreateAlloca(backend_get_llvm_type(var->type), nullptr, std::string(pname) + ".addr");
      builder.CreateStore(val, addr);
      val = addr;
    }
    var_map[std::string(pname)] = val;
    free(pname);
  }
  for (list_item_t* item = fn_node->body->head->next; item != fn_node->body->head; item = item->next) {
    backend_gen_stmt((node_t*)item->data);
  }
  if (!fn_ret) {
    if (fn->getReturnType() == Type::getVoidTy(context)) {
      builder.CreateRetVoid();
    } else {
      builder.CreateRet(ConstantInt::get(fn->getReturnType(), 0));
    }
  }
}

void backend_gen_var_def(node_t* node) {
  var_t* var_node = (var_t*)node->data;
  Type* type = backend_get_llvm_type(var_node->type);
  char* name = parse_str(var_node->name);
  AllocaInst* var = builder.CreateAlloca(type, nullptr, name);
  var_map[std::string(name)] = var;
  free(name);
  if (!var_node->initialised) return;
  if (var_node->type->is_arr) {
    if (var_node->initialised) {
      backend_gen_arr(var, type, node->lhs);
    }
  } else {
    Value* expr = backend_gen_expr(type, node->lhs);
    builder.CreateStore(expr, var);
  }
}

Value* backend_gen_fn_call(node_t* node) {
  funcall_t* fn_call_node = (funcall_t*)node->data;
  std::vector<Value*> args;
  char* name = parse_str(fn_call_node->name);
  Function* fn = fn_map[std::string(name)];
  free(name);
  auto param_iterator = fn->arg_begin();
  int size = 0;
  for (list_item_t* item = fn_call_node->arguments->head->next; item != fn_call_node->arguments->head;
    item = item->next) {
    Type* ty = nullptr;
    if (size < fn->arg_size()) {
      Value* val = &*param_iterator++;
      ty = val->getType();
    }
    size++;
    args.push_back(backend_gen_expr(ty, (node_t*)item->data));
  }
  auto* call = builder.CreateCall(fn, args);
  auto *result = call;
  return result;
}

void backend_gen_ret(node_t* node) {
  if (current_fn->getReturnType() == Type::getVoidTy(context)) {
    builder.CreateRetVoid();
  } else {
    builder.CreateRet(backend_gen_expr(current_fn->getReturnType(), node->lhs));
  }
  if (!inside_scope) fn_ret = true;
}

void backend_gen_stmt(node_t* node) {
  switch (node->type) {
    case NODE_FN_DEF:
      backend_gen_fn_def(node);
      break;
    case NODE_VAR_DEF:
      backend_gen_var_def(node);
      break;
    case NODE_FN_CALL:
      backend_gen_fn_call(node);
      break;
    case NODE_RET:
      backend_gen_ret(node);
      break;
  }
}

extern "C" void backend_gen(list_t* ast, char* out) {
  for (list_item_t* item = ast->head->next; item != ast->head; item = item->next) {
    backend_gen_stmt((node_t*)item->data);
  }

  std::string buffer;
  raw_string_ostream stream(buffer);
  module.print(stream, nullptr);

  std::ofstream ll("out.ll");
  ll << stream.str() << std::endl;
}