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
int inside_scope = 0;
type_t* current_ty; // parser ty, handles signess
BasicBlock* current_block;
bool should_br = true; // should put a br instruction at the end of conditional statement blocks?
std::map<std::string, Function*> fn_map;
std::map<std::string, Value*> var_map;
std::map<std::string, type_t*> var_types;
std::map<std::string, Type*> type_map; // structs, etc...

void backend_gen_stmt(node_t* node);
Value* backend_gen_fn_call(node_t* node);
Type* backend_get_llvm_type(type_t* ty);
Value* backend_gen_expr(Type* ty, node_t* node);
std::tuple<Type*,Value*> backend_gen_struct_acc(node_t* node);
std::tuple<Type*,Value*> backend_gen_deref(node_t* node);
std::tuple<Type*,Value*> backend_gen_ref(node_t* node);

Type* backend_get_ptr_arr_mod(type_t* ty, Type* bt) {
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

Type* backend_gen_struct(type_t* ty) {
  std::vector<Type *> fields;
  auto* type = StructType::create(context, "struct." + std::string(ty->type->name));
  for (list_item_t* item = ty->type->members->head->next; item != ty->type->members->head; item = item->next) {
    var_t* var = (var_t*)item->data;
    if (var->type->type == ty->type) {
      fields.push_back(backend_get_ptr_arr_mod(var->type, type));
    } else {
      fields.push_back(backend_get_llvm_type(var->type));
    }
  }
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
  bt = backend_get_ptr_arr_mod(ty, bt);
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
  } else if (isa<GlobalVariable>(var)) {
    return ((Type*)((GlobalVariable*)var)->getType())->getPointerElementType();
  } else {
    return var->getType();
  }
}

Value* backend_gen_dif(Value* val, Type* val_type, Type* expected_type, bool sign) {
  if (expected_type == nullptr || val_type == expected_type) {
    return val;
  }
  if (!expected_type->isIntegerTy() || !val_type->isIntegerTy()) {
    return val;
  }
  if (sign) {
    return builder.CreateSExtOrTrunc(val, expected_type);
  } else {
    return builder.CreateZExtOrTrunc(val, expected_type);
  }
}

Value* backend_gen_const_arr(Type* ty, list_t* items) {
  std::vector<llvm::Constant *> values(ty->getArrayNumElements());
  int i = 0;
  for (list_item_t* item = items->head->next; item != items->head; item = item->next) {
    node_t* node = (node_t*)item->data;
    values[i] = ConstantInt::get(ty->getArrayElementType(), node->value);
    i++;
  }
  auto init = ConstantArray::get(ArrayType::get(ty->getArrayElementType(), values.size()),
                                 values);
  return init;
}

std::tuple<Type*,Value*> backend_arr_gep(Type* ty, node_t* node, Value* var) {
  Value* alloca = var;
  list_t* expr_list = (list_t*)node->data;
  Value* idx;
  Type* arr_type = ty;
  Value* pointer = nullptr;
  bool is_pointer = false;
  if (ty->isPointerTy()) {
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
  return {arr_type, element_ptr};
}

std::tuple<Type*,Value*> backend_arr_idx(Type* ty, node_t* node, Value* var, bool store, Value* store_val) {
  auto gep = backend_arr_gep(backend_get_var_type(var), node, var);
  Value* element_ptr = std::get<1>(gep);
  Value* val;
  if (!store) {
    if (ty == nullptr) ty = std::get<0>(gep);
    val = builder.CreateLoad(std::get<0>(gep), element_ptr);
    val = backend_gen_dif(val, std::get<0>(gep), ty, (current_ty == nullptr ? false : current_ty->_signed));
  } else {
    val = builder.CreateStore(store_val, element_ptr);
  }
  return {std::get<0>(gep), val};
}

Value* backend_gen_aop(int node_type, bool _signed, Value* lhs, Value* expr) {
  Value* ret;
  switch (node_type) {
    case NODE_ASADD:
    case NODE_ADD:
      if (lhs->getType()->isPointerTy()) {
        ret = builder.CreateGEP(lhs->getType()->getPointerElementType(), lhs, expr);
      } else {
        ret = builder.CreateAdd(lhs, expr);
      }
      break;
    case NODE_ASSUB:
    case NODE_SUB: {
      if (lhs->getType()->isPointerTy()) {
        Value* sub = builder.CreateSub(ConstantInt::get(backend_get_var_type(expr), 0), expr);
        ret = builder.CreateGEP(lhs->getType()->getPointerElementType(), lhs, sub);
      } else {
        ret = builder.CreateSub(lhs, expr);
      }
      break;
    }
    case NODE_ASMUL:
    case NODE_MUL:
      ret = builder.CreateMul(lhs, expr);
      break;
    case NODE_ASDIV:
    case NODE_DIV:
      if (_signed) {
        ret = builder.CreateSDiv(lhs, expr);
      } else {
        ret = builder.CreateUDiv(lhs, expr);
      }
      break;
    case NODE_ASMOD:
    case NODE_MOD:
      if (_signed) {
        ret = builder.CreateSRem(lhs, expr);
      } else {
        ret = builder.CreateURem(lhs, expr);
      }
      break;
    case NODE_ASSHL:
    case NODE_SHL:
      ret = builder.CreateShl(lhs, expr);
      break;
    case NODE_ASSHR:
    case NODE_SHR:
      if (_signed) {
        ret = builder.CreateAShr(lhs, expr);
      } else {
        ret = builder.CreateLShr(lhs, expr);
      }
      break;
    case NODE_ASAND:
    case NODE_AND:
      ret = builder.CreateAnd(lhs, expr);
      break;
    case NODE_ASOR:
    case NODE_OR:
      ret = builder.CreateOr(lhs, expr);
      break;
    case NODE_ASXOR:
    case NODE_XOR:
      ret = builder.CreateXor(lhs, expr);
      break;
    case NODE_NOT:
      ret = builder.CreateXor(lhs, -1);
      break;
  }
  return ret;
}

std::tuple<Type*,Value*> backend_gen_iexpr(Type* ty, node_t* node) {
  Value *lhs, *rhs;
  Value* val;
  if (node->lhs && node->type != NODE_FN_CALL && node->type != NODE_STRUCT_ACC && node->type != NODE_AT && node->type != NODE_REF) {
    auto expr = backend_gen_iexpr(ty, node->lhs);
    lhs = std::get<1>(expr);
    if (ty == nullptr) {
      ty = std::get<0>(expr);
    }
  }
  if (node->rhs) {
    auto expr = backend_gen_iexpr(ty, node->rhs);
    rhs = std::get<1>(expr);
    if (ty == nullptr) {
      ty = std::get<0>(expr);
    }
  }
  switch (node->type) {
    case NODE_INT:
      if (ty == nullptr) ty = Type::getInt32Ty(context);
      if (ty->isPointerTy()) ty = ty->getPointerElementType(); // TODO: Check if this will suffice.
      val = backend_load_int(ty, node->value);
      break;
    case NODE_STR: {
      char* str = parse_str(node->tok);
      ty = PointerType::getInt8PtrTy(context);
      val = backend_load_str(ty, str, node->tok->text_len + 1);
      free(str);
      break;
    }
    case NODE_EXCL: {
      if (ty == nullptr) ty = Type::getInt32Ty(context);
      Value *zero = backend_load_int(ty, 0);
      val = builder.CreateICmpEQ(lhs, zero);
      ty = builder.getIntNTy(1);
      break;
    }
    case NODE_ID: {
      char* str = parse_str(node->tok);
      Value* var = var_map.find(std::string(str))->second;
      Type* first_ty = ty;
      ty = backend_get_var_type(var);
      if (current_ty == nullptr) {
        current_ty = var_types[std::string(str)];
      }
      free(str);
      if (backend_get_var_type(var)->isArrayTy()) {
        var = builder.CreateGEP(backend_get_var_type(var), var, { builder.getInt64(0), builder.getInt64(0) });
      } else {
        var = builder.CreateLoad(ty, var);
      }
      val = backend_gen_dif(var, ty, first_ty, current_ty->_signed);
      break;
    }
    case NODE_FN_CALL:
      val = backend_gen_fn_call(node);
      ty = val->getType();
      break;
    case NODE_ARR_IDX: {
      char* str = parse_str(node->tok);
      Value* var = var_map.find(std::string(str))->second;
      current_ty = var_types[std::string(str)];
      auto arr = backend_arr_idx(ty, node, var, false, nullptr);
      val = std::get<1>(arr);
      ty = std::get<0>(arr);
      free(str);
      break;
    }
    case NODE_STRUCT_ACC: {
      auto struct_acc = backend_gen_struct_acc(node);
      val = std::get<1>(struct_acc);
      Type* member_ty = std::get<0>(struct_acc);
      val = builder.CreateLoad(member_ty, val);
      val = backend_gen_dif(val, member_ty, ty, (current_ty ? current_ty->_signed : false));
      ty = member_ty;
      break;
    }
    case NODE_AT: {
      auto deref = backend_gen_deref(node);
      val = std::get<1>(deref);
      ty = std::get<0>(deref);
      val = builder.CreateLoad(ty, val);
      break;
    }
    case NODE_REF: {
      auto ref = backend_gen_ref(node);
      val = std::get<1>(ref);
      ty = std::get<0>(ref);
      break;
    }
    default:
      val = backend_gen_aop(node->type, (current_ty ? current_ty->_signed : false), lhs, rhs);
      break;
  }
  return {ty, val};
}

Value* backend_gen_expr(Type* ty, node_t* node) {
  return std::get<1>(backend_gen_iexpr(ty, node));
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
  Function* fn;
  char* name = parse_str(fn_node->name);
  if (fn_map.find(name) == fn_map.end()) {
    for (list_item_t* param = fn_node->params->head->next; param != fn_node->params->head; param = param->next) {
      node_t* node = (node_t*)param->data;
      if (node->type == NODE_3DOT) {
        vararg = true;
        break;
      }
      params.push_back(backend_get_llvm_type(((var_t*)node->data)->type));
    }
    FunctionType* fn_type = FunctionType::get(backend_get_llvm_type(fn_node->type), params, vararg);
    fn = Function::Create(fn_type, Function::ExternalLinkage, name, &module);
    fn_map[std::string(name)] = fn;
  } else {
    fn = fn_map.find(name)->second;
  }
  free(name);
  if (!fn_node->initialised) return;
  fn_ret = false;
  current_fn = fn;
  BasicBlock* entry = BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  current_block = entry;
  auto param_iterator = fn->arg_begin();
  for (list_item_t* param = fn_node->params->head->next; param != fn_node->params->head; param = param->next) {
    node_t* node = (node_t*)param->data;
    Value* val = &*param_iterator++;
    var_t* var = (var_t*)node->data;
    char* pname = parse_str(var->name);
    val->setName(pname);

    AllocaInst* addr = builder.CreateAlloca(backend_get_llvm_type(var->type), nullptr, std::string(pname) + ".addr");
    builder.CreateStore(val, addr);
    val = addr;

    var_map[std::string(pname)] = val;
    var_types[std::string(pname)] = var->type;
    free(pname);
  }
  for (list_item_t* item = fn_node->body->head->next; item != fn_node->body->head; item = item->next) {
    backend_gen_stmt((node_t*)item->data);
  }
  if (!fn_ret) {
    if (fn->getReturnType() == Type::getVoidTy(context)) {
      builder.CreateRetVoid();
    } else {
      Value* ret;
      if (fn->getReturnType()->isPointerTy()) {
        ret = builder.CreateAlloca(fn->getReturnType());
        ret = builder.CreateLoad(fn->getReturnType(), ret);
      } else {
        ret = backend_load_int(fn->getReturnType(), 0);
      }
      builder.CreateRet(ret);
    }
  }
  current_fn = nullptr;
}

void backend_gen_var_def(node_t* node) {
  var_t* var_node = (var_t*)node->data;
  Type* type = backend_get_llvm_type(var_node->type);
  char* name = parse_str(var_node->name);
  Value* var;
  if (current_fn != nullptr) {
    var = builder.CreateAlloca(type, nullptr, name);
  } else {
    auto glob = (&module)->getOrInsertGlobal(name, type);
    var = glob;
    ((GlobalVariable*)var)->setDSOLocal(true);
    ((GlobalVariable*)var)->setInitializer((ConstantInt*)backend_load_int(type, 0));
  }
  var_map[std::string(name)] = var;
  var_types[std::string(name)] = var_node->type;
  current_ty = var_node->type;
  free(name);
  if (!var_node->initialised) {
    if (isa<GlobalVariable>(var)) {
      ((GlobalVariable*)var)->setInitializer(ConstantAggregateZero::get(type));
    }
    return;
  }
  if (isa<GlobalVariable>(var)) {
    // TODO: Work with strings.
    Value* val;
    if (node->lhs->type == NODE_ARRAY) {
      arr_t* arr = (arr_t*)node->lhs->data;
      val = backend_gen_const_arr(type, arr->expr_list);
    } else {
      val = backend_load_int(type, node->lhs->value);
    }
    ((GlobalVariable*)var)->setInitializer((Constant*)val);
    return;
  }
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
  auto param_iterator = fn->arg_begin();
  int size = 0;
  if (fn_call_node->arguments->size > 0) {
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
  }
  free(name);
  auto* call = builder.CreateCall(fn, args);
  auto *result = call;
  return result;
}

std::tuple<var_t*, int> backend_get_struct_member_idx(type_t* struc, token_t* member) {
  int i = 0;
  var_t* var;
  for (list_item_t* item = struc->type->members->head->next; item != struc->type->members->head; item = item->next) {
    var = (var_t*)item->data;
    if (!strncmp(var->name->text, member->text, member->text_len)) {
      current_ty = var->type;
      break;
    }
    i++;
  }
  return {var, i};
}

std::tuple<Type*,Value*> backend_gen_struct_acc(node_t* node) {
  char* name = parse_str(node->tok);
  Value* val = var_map[std::string(name)];
  current_ty = var_types[std::string(name)];
  free(name);
  node_t* temp = node;
  Type* ty = backend_get_var_type(val);
  if (ty->isPointerTy()) {
    val = builder.CreateLoad(ty, val);
    ty = ty->getPointerElementType();
  }
  while (temp->lhs) {
    temp = temp->lhs;
    auto member = backend_get_struct_member_idx(current_ty, temp->tok);
    int access = std::get<1>(member);
    val = builder.CreateGEP(ty, val, { builder.getInt32(0), builder.getInt32(access) });
    current_ty = std::get<0>(member)->type;
    ty = backend_get_llvm_type(std::get<0>(member)->type);
    if (temp->type == NODE_ARR_IDX) {
      auto gep = backend_arr_gep(ty, temp, val);
      val = std::get<1>(gep);
      ty = std::get<0>(gep);
    } else {
      if (ty->isPointerTy() && temp->lhs) {
        val = builder.CreateLoad(ty, val);
        ty = ty->getPointerElementType();
      }
    }
  }
  return {ty, val};
}

std::tuple<Type*,Value*> backend_gen_deref(node_t* node) {
  std::tuple<Type*,Value*> expr;
  Value* val;
  Type* ty;
  // TODO: Probably check if ty is a pointer... this should be done in parser, no?
  if (node->lhs->type == NODE_STRUCT_ACC) {
    auto struct_acc = backend_gen_struct_acc(node->lhs);
    val = std::get<1>(struct_acc);
    ty = std::get<0>(struct_acc);
    ty = ty->getPointerElementType();
  } else if (node->lhs->type == NODE_ID) {
    expr = backend_gen_iexpr(nullptr, node->lhs);
    val = std::get<1>(expr);
    ty = std::get<0>(expr);
    ty = ty->getPointerElementType();
  } else if (node->lhs->type == NODE_ARR_IDX) {
    char* name = parse_str(node->lhs->tok);
    Value* var = var_map[name];
    auto arr_idx = backend_arr_gep(backend_get_var_type(var), node->lhs, var);
    val = std::get<1>(arr_idx);
    ty = std::get<0>(arr_idx);
    ty = ty->getPointerElementType();
  } else {
    node_t* var_node = node->lhs->lhs;
    char* name = parse_str(var_node->tok);
    Value* var = var_map[name];
    ty = backend_get_llvm_type(var_types[name]);

    var = builder.CreateLoad(ty, var);
    ty = ty->getPointerElementType();

    expr = backend_gen_iexpr(nullptr, node->lhs->rhs);
    val = std::get<1>(expr);
    val = builder.CreateGEP(ty, var, val);
  }
  return {ty, val};
}

std::tuple<Type*,Value*> backend_gen_ref(node_t* node) {
  // lhs is the expression
  std::tuple<Type*,Value*> expr;
  Value* val;
  Type* ty;
  Value* var;
  std::vector<Value*> idx;
  if (node->lhs->type == NODE_STRUCT_ACC) {
    auto struct_acc = backend_gen_struct_acc(node->lhs);
    val = std::get<1>(struct_acc);
    ty = std::get<0>(struct_acc);
  } else if (node->lhs->type == NODE_ID) {
    char* name = parse_str(node->lhs->tok);
    var = var_map[name];
    free(name);
    val = var;
    ty = var->getType();
  } else if (node->lhs->type == NODE_ARR_IDX) {
    char* name = parse_str(node->lhs->tok);
    var = var_map[name];
    free(name);
    auto arr_idx = backend_arr_gep(backend_get_var_type(var), node->lhs, var);
    val = std::get<1>(arr_idx);
    ty = std::get<0>(arr_idx);
  } else {
    node_t* var_node = node->lhs->lhs;
    char* name = parse_str(var_node->tok);
    Value* var = var_map[name];
    ty = backend_get_llvm_type(var_types[name]);

    var = builder.CreateLoad(ty, var);
    ty = ty->getPointerElementType();

    expr = backend_gen_iexpr(nullptr, node->lhs->rhs);
    val = std::get<1>(expr);
    val = builder.CreateGEP(ty, var, val);
  }
  return {ty, val};
}

std::tuple<Type*,Value*> backend_gen_lvalue(node_t* lvalue) {
  switch (lvalue->type) {
    case NODE_ID: {
      char* pname = parse_str(lvalue->tok);
      Value* val = var_map[std::string(pname)];
      current_ty = var_types[std::string(pname)];
      free(pname);
      return {backend_get_llvm_type(current_ty), val};
      break;
    }
    case NODE_ARR_IDX: {
      char* str = parse_str(lvalue->tok);
      Value* var = var_map.find(std::string(str))->second;
      current_ty = var_types[std::string(str)];
      auto val = backend_arr_gep(backend_get_var_type(var), lvalue, var);
      free(str);
      return val;
      break;
    }
    case NODE_STRUCT_ACC:
      return backend_gen_struct_acc(lvalue);
      break;
    case NODE_AT:
      return backend_gen_deref(lvalue);
      break;
    case NODE_REF:
      return backend_gen_ref(lvalue);
      break;
    default:
      printf("Unhandled lvalue.\n");
      break;
  }
  return {};
}

void backend_gen_assignment(node_t* node) {
  auto ty_lval = backend_gen_lvalue(node->lhs);
  Value* lvalue = std::get<1>(ty_lval);
  Value* expr = backend_gen_expr(std::get<0>(ty_lval), node->rhs);
  Value* store = expr;

  if (node->type != NODE_ASSIGN) {
    Value* lhs;
    lhs = builder.CreateLoad(std::get<0>(ty_lval), lvalue);
    store = backend_gen_aop(node->type, (current_ty ? current_ty->_signed : false), lhs, expr);
  }

  builder.CreateStore(store, lvalue);
}

void backend_gen_ret(node_t* node) {
  if (current_fn->getReturnType() == Type::getVoidTy(context)) {
    builder.CreateRetVoid();
  } else {
    builder.CreateRet(backend_gen_expr(current_fn->getReturnType(), node->lhs));
  }
  if (!inside_scope) fn_ret = true;
  if (inside_scope) should_br = false;
}

void backend_gen_cond(node_t* cond, BasicBlock* true_block, BasicBlock* false_block) {
  if (cond->type >= NODE_EQEQ && cond->type <= NODE_NOTEQ && cond->type != NODE_NOT) {
    auto first_expr = backend_gen_iexpr(nullptr, cond->lhs);
    type_t* ty = current_ty;
    auto second_expr = backend_gen_iexpr(std::get<0>(first_expr), cond->rhs);
    Value* lhs = std::get<1>(first_expr);
    Value* rhs = std::get<1>(second_expr);
    Value* cmp;
    switch (cond->type) {
      case NODE_EQEQ:
        cmp = builder.CreateICmpEQ(lhs, rhs, "cmp");
        break;
      case NODE_GT:
        if (current_ty->_signed) {
          cmp = builder.CreateICmpSGT(lhs, rhs, "cmp");
        } else {
          cmp = builder.CreateICmpUGT(lhs, rhs, "cmp");
        }
        break;
      case NODE_GTEQ:
        if (current_ty->_signed) {
          cmp = builder.CreateICmpSGE(lhs, rhs, "cmp");
        } else {
          cmp = builder.CreateICmpUGE(lhs, rhs, "cmp");
        }
        break;
      case NODE_LT:
        if (current_ty->_signed) {
          cmp = builder.CreateICmpSLT(lhs, rhs, "cmp");
        } else {
          cmp = builder.CreateICmpULT(lhs, rhs, "cmp");
        }
        break;
      case NODE_LTEQ:
        if (current_ty->_signed) {
          cmp = builder.CreateICmpSLE(lhs, rhs, "cmp");
        } else {
          cmp = builder.CreateICmpULE(lhs, rhs, "cmp");
        }
        break;
      case NODE_NOTEQ:
        cmp = builder.CreateICmpNE(lhs, rhs, "cmp");
        break;
    }
    builder.CreateCondBr(cmp, true_block, false_block);
  } else {
    Value* cmp;
    BasicBlock* temp_block = current_block;
    if (cond->type == NODE_DBAND) {
      BasicBlock* land_lhs_true = BasicBlock::Create(context, "land.lhs.true", current_fn);
      backend_gen_cond(cond->lhs, land_lhs_true, false_block);
      builder.SetInsertPoint(land_lhs_true);
      backend_gen_cond(cond->rhs, true_block, false_block);
      builder.SetInsertPoint(temp_block);
    } else if (cond->type == NODE_DBOR) {
      BasicBlock* lor_lhs_false = BasicBlock::Create(context, "lor.lhs.false", current_fn);
      backend_gen_cond(cond->lhs, true_block, lor_lhs_false);
      builder.SetInsertPoint(lor_lhs_false);
      backend_gen_cond(cond->rhs, true_block, false_block);
      builder.SetInsertPoint(temp_block);
    } else {
      auto first_expr = backend_gen_iexpr(nullptr, cond);
      Value* val = std::get<1>(first_expr);
      Value* zero = backend_load_int(std::get<0>(first_expr), 0);
      cmp = builder.CreateICmpUGT(val, zero, "cmp");
      builder.CreateCondBr(cmp, true_block, false_block);
    }
  }
}

void backend_gen_for_loop(node_t* node) {
  for_stmt_t* stmt = (for_stmt_t*)node->data;

  if (stmt->primary_stmt->type == NODE_VAR_DEF)
    backend_gen_var_def(stmt->primary_stmt);
  else 
    backend_gen_assignment(stmt->primary_stmt);

  BasicBlock* entry_block = current_block;

  BasicBlock* cond = BasicBlock::Create(context, "for.cond", current_fn);
  builder.SetInsertPoint(cond);
  current_block = cond;

  BasicBlock* step = BasicBlock::Create(context, "for.step", current_fn);
  builder.SetInsertPoint(step);
  current_block = step;
  backend_gen_assignment(stmt->step);
  builder.CreateBr(cond);

  inside_scope++;
  BasicBlock* body = BasicBlock::Create(context, "for.body", current_fn);
  builder.SetInsertPoint(body);
  current_block = body;
  backend_gen_stmt(stmt->body);
  if (should_br) builder.CreateBr(step);
  else should_br = true;

  inside_scope--;

  BasicBlock* end = BasicBlock::Create(context, "for.end", current_fn);
  current_block = end;

  builder.SetInsertPoint(entry_block);
  builder.CreateBr(cond);

  builder.SetInsertPoint(cond);
  backend_gen_cond(node->lhs, body, end);

  builder.SetInsertPoint(end);
}

void backend_gen_if_stmt(node_t* node) {
  if_stmt_t* stmt = (if_stmt_t*)node->data;
  BasicBlock* entry_block = current_block;

  BasicBlock* true_body = BasicBlock::Create(context, "if.then", current_fn);
  builder.SetInsertPoint(true_body);
  current_block = true_body;
  inside_scope++;
  backend_gen_stmt(stmt->true_stmt);
  bool true_should_br = should_br;
  should_br = true;
  bool false_should_br = true;
  BasicBlock* end_true_body = current_block;
  BasicBlock* else_body;
  BasicBlock* end_else_body;

  if (stmt->false_stmt != NULL) {
    inside_scope++;
    else_body = BasicBlock::Create(context, "if.else", current_fn);
    builder.SetInsertPoint(else_body);
    current_block = else_body;
    backend_gen_stmt(stmt->false_stmt);
    end_else_body = current_block;
    inside_scope--;
    false_should_br = should_br;
    should_br = true;
  }

  BasicBlock* end_body = BasicBlock::Create(context, "if.end", current_fn);
  builder.SetInsertPoint(end_true_body);
  if (true_should_br) builder.CreateBr(end_body);
  inside_scope--;

  if (stmt->false_stmt != NULL && false_should_br) {
    if (end_else_body != else_body) {
      builder.SetInsertPoint(end_else_body);
      builder.CreateBr(end_body);
    } else {
      builder.SetInsertPoint(else_body);
      builder.CreateBr(end_body);
    }
  }

  builder.SetInsertPoint(entry_block);
  backend_gen_cond(node->lhs, true_body, (stmt->false_stmt ? else_body : end_body));

  builder.SetInsertPoint(end_body);
  current_block = end_body;
}

void backend_gen_while_loop(node_t* node) {
  while_stmt_t* stmt = (while_stmt_t*)node->data;

  BasicBlock* entry_block = current_block;

  BasicBlock* cond = BasicBlock::Create(context, "while.cond", current_fn);
  builder.SetInsertPoint(cond);
  current_block = cond;

  inside_scope++;
  BasicBlock* body = BasicBlock::Create(context, "while.body", current_fn);
  builder.SetInsertPoint(body);
  current_block = body;
  backend_gen_stmt(stmt->body);
  if (should_br) builder.CreateBr(cond);
  else should_br = true;
  inside_scope--;

  BasicBlock* end = BasicBlock::Create(context, "while.end", current_fn);
  current_block = end;

  builder.SetInsertPoint(entry_block);
  builder.CreateBr(cond);

  builder.SetInsertPoint(cond);
  backend_gen_cond(node->lhs, body, end);

  builder.SetInsertPoint(end);
}

void backend_gen_stmt(node_t* node) {
  switch (node->type) {
    case NODE_COMPOUND: {
      list_t* ast = (list_t*)node->data;
      for (list_item_t* item = ast->head->next; item != ast->head; item = item->next) {
        node_t* node = (node_t*)item->data;
        backend_gen_stmt(node);
      }
      break;
    }
    case NODE_FN_DEF:
      backend_gen_fn_def(node);
      break;
    case NODE_VAR_DEF:
      backend_gen_var_def(node);
      break;
    case NODE_FN_CALL:
      backend_gen_fn_call(node);
      break;
    case NODE_ASSIGN...NODE_ASXOR:
      backend_gen_assignment(node);
      break;
    case NODE_FOR:
      backend_gen_for_loop(node);
      break;
    case NODE_IF:
      backend_gen_if_stmt(node);
      break;
    case NODE_WHILE:
      backend_gen_while_loop(node);
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

  std::ofstream ll(out);
  ll << stream.str() << std::endl;
  ll.close();
}