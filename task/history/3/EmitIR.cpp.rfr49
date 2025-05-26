#include "EmitIR.hpp"
#include <stack>
#include <iostream>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/IR/ValueSymbolTable.h>

#define self (*this)

using namespace asg;

EmitIR::EmitIR(Obj::Mgr& mgr, llvm::LLVMContext& ctx, llvm::StringRef mid)
  : mMgr(mgr)
  , mMod(mid, ctx)
  , mCtx(ctx)
  , mIntTy(llvm::Type::getInt32Ty(ctx))
  , mCurIrb(std::make_unique<llvm::IRBuilder<>>(ctx))
  , mCtorTy(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false))
  , mCurFunc(nullptr)
  /*
  在EmitIR类中，有以下成员变量：

  mMgr: Manager, 这是一个Obj::Mgr类型的引用，可能是用来管理一些对象或资源的管理器。

  mMod: Module, 这是一个llvm::Module对象，它在LLVM中表示一个编译单元，包含了函数和全局变量的定义。

  mCtx: Context, 这是一个llvm::LLVMContext引用，它在LLVM中表示一个独立的全局上下文。所有在同一个LLVMContext中创建的值都可以相互引用。

  mIntTy: Integer Type, 这是一个llvm::Type指针，它表示一个32位整型。在LLVM中，llvm::Type用来表示类型。

  mCurIrb: Current IR Builder, 这是一个llvm::IRBuilder的智能指针。llvm::IRBuilder是一个辅助类，用于生成LLVM指令。

  mCtorTy: Constructor Type, 这是一个llvm::FunctionType指针，它表示一个没有参数且返回类型为void的函数类型。

  这些成员变量在EmitIR的构造函数中被初始化。它们在后续的成员函数中被用来生成LLVM的中间表示（IR）。

  */
{
}

llvm::Module&
EmitIR::operator()(asg::TranslationUnit* tu)
{
  for (auto&& i : tu->decls)
    self(i);
  return mMod;
}

//==============================================================================
// 类型
//==============================================================================

llvm::Type*
EmitIR::operator()(const Type* type)
{
  if (type->texp == nullptr) {
    switch (type->spec) {
      case Type::Spec::kInt:
        return llvm::Type::getInt32Ty(mCtx);
      // TODO: 在此添加对更多基础类型的处理
      case Type::Spec::kVoid:
        return llvm::Type::getVoidTy(mCtx);
      default:
        ABORT();
    }
  }

  Type subt;
  subt.spec = type->spec;
  subt.qual = type->qual;
  subt.texp = type->texp->sub;

  // TODO: 在此添加对指针类型、数组类型和函数类型的处理

  if (auto p = type->texp->dcst<PointerType>()) {
    return self(&subt)->getPointerTo();
  }

  if (auto p = type->texp->dcst<ArrayType>()) {
    if (p->len == ArrayType::kUnLen)
      return self(&subt)->getPointerTo();
    return llvm::ArrayType::get(self(&subt), p->len);
  }

  if (auto p = type->texp->dcst<FunctionType>()) {
    std::vector<llvm::Type*> pty;
    // TODO: 在此添加对函数参数类型的处理
    for (auto &param : p->params)
      pty.push_back(self(param));
    return llvm::FunctionType::get(self(&subt), std::move(pty), false);
  }

  ABORT();
}

//==============================================================================
// 表达式
//==============================================================================

llvm::Value*
EmitIR::operator()(Expr* obj)
{
  // TODO: 在此添加对更多表达式处理的跳转
  if (auto p = obj->dcst<IntegerLiteral>())
    return self(p);

  if (auto p = obj->dcst<BinaryExpr>())
    return self(p);
  
  if (auto p = obj->dcst<ImplicitCastExpr>())
    return self(p);
  
  if (auto p = obj->dcst<DeclRefExpr>())
    return self(p);
  
  if (auto p = obj->dcst<UnaryExpr>())
    return self(p);

  if (auto p = obj->dcst<ParenExpr>())
    return self(p);

  if (auto p = obj->dcst<CallExpr>())
    return self(p);

  ABORT();
}

llvm::Constant*
EmitIR::operator()(IntegerLiteral* obj)
{
  return llvm::ConstantInt::get(self(obj->type), obj->val);
}

// TODO: 在此添加对更多表达式类型的处理

llvm::Value* EmitIR::operator()(ParenExpr* obj) {
  return self(obj->sub);
}

llvm::Value* EmitIR::operator()(UnaryExpr* obj) {
  auto sub = self(obj->sub);

  auto &irb = *mCurIrb;

  switch (obj->op) {
    case UnaryExpr::Op::kPos:
      return sub;
    case UnaryExpr::Op::kNeg:
      return irb.CreateNeg(sub);
    case UnaryExpr::Op::kNot: {
      llvm::Value* toBool = sub->getType()->isIntegerTy(1) ? sub : irb.CreateICmpNE(sub, llvm::Constant::getNullValue(sub->getType()));
      return irb.CreateNot(toBool);
    }
    default:
      ABORT();
  }

}

llvm::Value* EmitIR::operator()(BinaryExpr* obj) {
  auto& irb = *mCurIrb;

  auto lft = self(obj->lft);

  switch (obj->op) {
    case BinaryExpr::Op::kOr: {

      auto rht = self(obj->rht);
      return irb.CreateOr(lft, rht);

    }

    case BinaryExpr::Op::kAnd: {
      auto rht = self(obj->rht);
      return irb.CreateAnd(lft, rht);
    }    
  }
  auto rht = self(obj->rht);
  switch (obj->op)
  {
    case BinaryExpr::Op::kAdd:
      return irb.CreateAdd(lft, rht);
    case BinaryExpr::Op::kSub:
      return irb.CreateSub(lft, rht);
    case BinaryExpr::Op::kMul:
      return irb.CreateMul(lft, rht);
    case BinaryExpr::Op::kDiv:
      return irb.CreateSDiv(lft, rht);
    case BinaryExpr::Op::kMod:
      return irb.CreateSRem(lft, rht);
    case BinaryExpr::Op::kEq:
      return irb.CreateICmpEQ(lft, rht);
    case BinaryExpr::Op::kNe:
      return irb.CreateICmpNE(lft, rht);
    case BinaryExpr::Op::kLt:
      return irb.CreateICmpSLT(lft, rht);
    case BinaryExpr::Op::kLe:
      return irb.CreateICmpSLE(lft, rht);
    case BinaryExpr::Op::kGt:
      return irb.CreateICmpSGT(lft, rht);
    case BinaryExpr::Op::kGe:
      return irb.CreateICmpSGE(lft, rht);
    case BinaryExpr::Op::kComma:
      return rht;
    case BinaryExpr::Op::kAssign:
      return irb.CreateStore(rht, lft);
    case BinaryExpr::Op::kIndex:{
      auto type = self(obj->type);
      return irb.CreateInBoundsGEP(type, lft, rht);
    }
    default:
      ABORT();
    }
}

llvm::Value* EmitIR::operator()(ImplicitCastExpr* obj) {
  auto sub = self(obj->sub);

  auto &irb = *mCurIrb;

  switch (obj->kind) {
    case ImplicitCastExpr::kLValueToRValue:{
      auto type = self(obj->sub->type);
      return irb.CreateLoad(type, sub);
    }
    case ImplicitCastExpr::kIntegralCast:{
      return irb.CreateIntCast(sub, self(obj->sub->type), true);
    }
    case ImplicitCastExpr::kArrayToPointerDecay:{
      auto type = self(obj->sub->type);
      return irb.CreateInBoundsGEP(type, sub, {irb.getInt64(0)});
    }
    case ImplicitCastExpr::kFunctionToPointerDecay:
      return sub;
    default:
      ABORT();
  }
}

llvm::Value* EmitIR::operator()(DeclRefExpr* obj) {
  // return reinterpret_cast<llvm::Value*>(obj->decl->any);

  auto name = obj->decl->name;
  auto type = self(obj->decl->type);
  // 先查看局部遮掩的符号
  if (auto p = mCurFunc->getValueSymbolTable()->lookup(name)) {
    return p;
  }

  if (auto p = mMod.getGlobalVariable(name)) {
    return p;
  }

  if (auto p = mMod.getFunction(name)) {
    return p;
  }
}

llvm::Value* EmitIR::operator()(CallExpr* obj) {
  auto &irb = *mCurIrb;

  auto func = reinterpret_cast<llvm::Function*>(self(obj->head));
  std::vector<llvm::Value*> params;
  for (int iter = 0; iter < obj->args.size(); iter++) {
    params.push_back(self(obj->args[iter]));
  }
  return irb.CreateCall(func,params);

}

//==============================================================================
// 语句
//==============================================================================

void
EmitIR::operator()(Stmt* obj)
{
  // TODO: 在此添加对更多Stmt类型的处理的跳转
  auto& irb = *mCurIrb;

  if (auto p = obj->dcst<CompoundStmt>()) {
    auto sp = irb.CreateIntrinsic(llvm::Intrinsic::stacksave, {}, {}, nullptr, "sp");
    return self(p);
    irb.CreateIntrinsic(llvm::Intrinsic::stackrestore, {}, {sp});
  }

  if (auto p = obj->dcst<ReturnStmt>())
    return self(p);

  if (auto p = obj->dcst<DeclStmt>())
    return self(p);

  if (auto p = obj->dcst<ExprStmt>())
    return self(p);
  
  if (auto p = obj->dcst<IfStmt>()) 
    return self(p);

  if (auto p = obj->dcst<WhileStmt>())
    return self(p);
  
  if (auto p = obj->dcst<DoStmt>())
    return self(p);
  
  if (auto p = obj->dcst<BreakStmt>())
    return self(p);
  
  if (auto p = obj->dcst<ContinueStmt>())
    return self(p);
  
  if (auto p = obj->dcst<ReturnStmt>())
    return self(p);

  if (auto p = obj->dcst<NullStmt>())
    return self(p);

  ABORT();
}

// TODO: 在此添加对更多Stmt类型的处理

void EmitIR::operator()(NullStmt* obj){
  return;
}

void EmitIR::operator()(ContinueStmt* obj) {
  auto &irb = *mCurIrb;
  auto jump = reinterpret_cast<llvm::BasicBlock*>(obj->loop->any)->getPrevNode();
  irb.CreateBr(jump);
}

void EmitIR::operator()(BreakStmt* obj) {
  auto &irb = *mCurIrb;
  auto jump = reinterpret_cast<llvm::BasicBlock*>(obj->loop->any)->getNextNode();
  irb.CreateBr(jump);
}

void EmitIR::operator()(WhileStmt* obj) {
  auto &irb = *mCurIrb;

  auto condBb = llvm::BasicBlock::Create(mCtx, "cond", mCurFunc);
  auto bodyBb = llvm::BasicBlock::Create(mCtx, "body", mCurFunc);
  auto exitBb = llvm::BasicBlock::Create(mCtx, "exit", mCurFunc);
  obj->any = bodyBb;
  irb.CreateBr(condBb);

  mCurIrb = std::make_unique<llvm::IRBuilder<>>(condBb);
  auto cond = self(obj->cond);
  if (mCurIrb->GetInsertBlock()->getTerminator() == nullptr)
    mCurIrb->CreateCondBr(cond, bodyBb, exitBb);

  mCurIrb = std::make_unique<llvm::IRBuilder<>>(bodyBb);
  self(obj->body);
  if (mCurIrb->GetInsertBlock()->getTerminator() == nullptr)
    mCurIrb->CreateBr(condBb);

  mCurIrb = std::make_unique<llvm::IRBuilder<>>(exitBb);

}

void EmitIR::operator()(IfStmt* obj) {
  auto &irb = *mCurIrb;

  auto cond = self(obj->cond);
  auto thenBb = llvm::BasicBlock::Create(mCtx, "then", mCurFunc);
  auto elseBb = llvm::BasicBlock::Create(mCtx, "else", mCurFunc);
  auto exitBb = llvm::BasicBlock::Create(mCtx, "exit", mCurFunc);

  irb.CreateCondBr(cond, thenBb, elseBb);

  mCurIrb = std::make_unique<llvm::IRBuilder<>>(thenBb);
  if (obj->then) {
    self(obj->then);
  }
  if (mCurIrb->GetInsertBlock()->getTerminator() == nullptr)
    mCurIrb->CreateBr(exitBb);


  mCurIrb = std::make_unique<llvm::IRBuilder<>>(elseBb);
  if (obj->else_) {
    self(obj->else_);
  }
  if (mCurIrb->GetInsertBlock()->getTerminator() == nullptr)
    mCurIrb->CreateBr(exitBb);

  mCurIrb = std::make_unique<llvm::IRBuilder<>>(exitBb);
}

void
EmitIR::operator()(ExprStmt* obj)
{
  self(obj->expr);
}

void
EmitIR::operator()(CompoundStmt* obj)
{
  auto& irb = *mCurIrb;
  // TODO: 可以在此添加对符号重名的处理
  
  for (auto&& stmt : obj->subs)
    self(stmt);
}

void EmitIR::operator()(DeclStmt* obj) {
  for (auto&& decl : obj->decls)
    self(decl);
}

void
EmitIR::operator()(ReturnStmt* obj)
{
  auto& irb = *mCurIrb;

  llvm::Value* retVal;
  if (!obj->expr)
    retVal = nullptr;
  else
    retVal = self(obj->expr);

  mCurIrb->CreateRet(retVal);

  auto exitBb = llvm::BasicBlock::Create(mCtx, "return_exit", mCurFunc);
  mCurIrb = std::make_unique<llvm::IRBuilder<>>(exitBb);
}

//==============================================================================
// 声明
//==============================================================================

struct element {
  InitListExpr* initList;
  llvm::Type* type;
  llvm::Value* dst;
};

void EmitIR::transInit(llvm::Value* dst, Expr* src, VarDecl* obj) {
  auto& irb = *mCurIrb;

  if (auto p = src->dcst<IntegerLiteral>()) {
    auto initVal = llvm::ConstantInt::get(self(p->type), p->val);
    irb.CreateStore(initVal, dst);
    return;
  }
  if (auto p = src->dcst<InitListExpr>()) {
    std::stack<element> stack;
    stack.push({p, self(obj->type), dst});

    while (!stack.empty()) {
      auto element = stack.top();
      stack.pop();
      auto type = element.type;
      auto initList = element.initList;
      auto dst = element.dst;

      for (int i = type->getArrayNumElements()-1; i>=0; i--) {
        int listsize = initList->list.size();
        if (i < initList->list.size()) {
          auto sub = initList->list[i];
          auto subDst = irb.CreateInBoundsGEP(type, dst, {irb.getInt64(0), irb.getInt64(i)});
          if (auto p = sub->dcst<InitListExpr>()) {
            stack.push({p,type->getArrayElementType() ,subDst});
          } else {
            transInit(subDst, sub, obj);
          }
        }
        else {
          auto initVal = llvm::Constant::getNullValue(type->getArrayElementType());
          auto subDst = irb.CreateInBoundsGEP(type, dst, {irb.getInt64(0), irb.getInt64(i)});
          irb.CreateStore(initVal, subDst);
        }
      }
    }
    return;
  }

  if (auto p = src->dcst<ImplicitInitExpr>()) {
    auto initVal = llvm::Constant::getNullValue(self(obj->type));
    irb.CreateStore(initVal, dst);
    return;
  }

  if (auto p = src->dcst<ImplicitCastExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, dst);
    return;
  }

  if (auto p = src->dcst<UnaryExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, dst);
    return;
  }

  if (auto p = src->dcst<ParenExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, dst);
    return;
  }

  if (auto p = src->dcst<BinaryExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, dst);
    return;
  }

  if (auto p = src->dcst<DeclRefExpr>()) {
      auto initVal = self(p);
      irb.CreateStore(initVal, dst);
      return;
  }

  if (auto p = src->dcst<CallExpr>()) {
    auto initVal = self(p);
    irb.CreateStore(initVal, dst);
    return;
  }

  ABORT();
}

void EmitIR::operator()(Decl* obj) {
  if (auto p = obj->dcst<FunctionDecl>())
    return self(p);
  
  if (auto p = obj->dcst<VarDecl>())
    return self(p);
  
  ABORT();
}

void EmitIR::operator()(VarDecl* obj) {

  // 通过判断变量声明是否在基本块中，来判断变量是全局变量还是局部变量
  if (mCurFunc) {
    auto type = self(obj->type);
    auto alloca = mCurIrb->CreateAlloca(type, nullptr, obj->name);
    obj->any = alloca;

    if (obj->init == nullptr)
      return;
    if (obj->init) 
      transInit(alloca, obj->init,obj);
    else
      mCurIrb->CreateStore(llvm::Constant::getNullValue(type), alloca);
    return;
  }

  // 生成全局变量

  auto preFunc = mCurFunc;

  auto type = llvm::Type::getInt64Ty(mCtx);

  auto gvar = new llvm::GlobalVariable(
    mMod, type, false, llvm::GlobalValue::ExternalLinkage, nullptr, obj->name);

  obj->any = gvar;

  gvar->setInitializer(llvm::ConstantInt::get(type, 0));

  if (obj->init == nullptr)
    return;
  
  // 生成构造函数
  // 生成构造函数的理由是：全局变量的初始化是在main函数之前的，而全局变量的初始化是在main函数之后的
  mCurFunc = llvm::Function::Create(
    mCtorTy, llvm::GlobalVariable::PrivateLinkage, obj->name + "ctor_" + obj->name, mMod);
  llvm::appendToGlobalCtors(mMod, mCurFunc, 65535);

  // 生成函数体  
  // 函数体的内容是：全局变量的初始化
  auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", mCurFunc);
  // 生成IRBuilder
  // 为什么要以entryBb为参数呢？
  // 因为entryBb是构造函数的入口
  // IRBuilder的参数是一个BasicBlock，表示在这个BasicBlock中生成IR
  mCurIrb = std::make_unique<llvm::IRBuilder<>>(entryBb);
  // transinit的作用是：将一个表达式的值赋给一个变量
  transInit(gvar, obj->init,obj);
  mCurIrb->CreateRet(nullptr);
  mCurFunc = preFunc;
  return;

}

void
EmitIR::operator()(FunctionDecl* obj)
{
  // 创建函数
  auto fty = llvm::dyn_cast<llvm::FunctionType>(self(obj->type));
  std::string testName = obj->name;
  // std::cout<<"test-----cout:"<<testName<<std::endl;
  auto func = llvm::Function::Create(fty, llvm::GlobalVariable::ExternalLinkage, obj->name, mMod);

  // std::cout<<"test-----cout:"<<func<<std::endl;
  obj->any = func;

  // if (fty) {
  //   fty->getReturnType()->print(llvm::errs());
  //   llvm::errs()<<'\n';
  //   for (auto& paramType:fty->params()) {
  //     paramType->print(llvm::errs());
  //   }
  // }

  if (obj->body == nullptr)
    return;
  auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", func);
  mCurIrb = std::make_unique<llvm::IRBuilder<>>(entryBb);
  auto& entryIrb = *mCurIrb;

  // TODO: 添加对函数参数的处理

  auto argIter = func->arg_begin();
  
  // std::cout<<"test-----cout:"<<argIter<<std::endl;
  for (int i = 0; i < obj->params.size(); i++) {
    auto param = obj->params[i];
    auto paramVar = mCurIrb->CreateAlloca(self(param->type), nullptr, param->name);
    argIter->setName(param->name);
    entryIrb.CreateStore(argIter,paramVar);
    argIter++;
  }

  // 翻译函数体
  mCurFunc = func;
  self(obj->body);
  auto& exitIrb = *mCurIrb;

  if (fty->getReturnType()->isVoidTy())
    exitIrb.CreateRetVoid();
  else
    exitIrb.CreateUnreachable();
}


