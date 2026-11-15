#pragma once
#include "Token.hpp"
#include <string>
#include <vector>
#include <memory>

namespace cvc {

enum class ExprKind { Identifier, Literal, Binary, Unary, Call, Subscript, MemberAccess, FailableCall, New };
struct Expr {
  ExprKind kind;
  Token op;
  std::string value;
  std::shared_ptr<Expr> left, right;
  std::vector<std::shared_ptr<Expr>> args;
};

enum class StmtKind { Block, If, Return, Fail, Expr, VarDecl };
struct Stmt {
  StmtKind kind;
  std::shared_ptr<Expr> expr;
  std::vector<std::shared_ptr<Stmt>> body;
  std::shared_ptr<Stmt> elseBranch;
  std::shared_ptr<Stmt> varDeclCond; // for if(auto x = ...)
  std::string varName;
  std::string varType;
  bool isMut = false;
  std::shared_ptr<Expr> varInit;
};

struct MethodDecl {
  std::string access = "public";
  std::string name;
  std::string returnType;
  bool isFailable = false;
  bool isConst = false;
  std::vector<std::pair<std::string, std::string>> params;
  std::vector<std::shared_ptr<Stmt>> body;
};

struct FieldDecl {
  std::string access = "private";
  std::string name;
  std::string type;
  bool isMut = false;
  std::shared_ptr<Expr> init;
};

struct ClassDecl {
  std::string name;
  std::vector<FieldDecl> fields;
  std::vector<MethodDecl> methods;
};

struct GlobalFuncDecl {
  std::string access = "public";
  std::string name;
  std::string returnType;
  bool isFailable = false;
  std::vector<std::pair<std::string, std::string>> params;
  std::vector<std::shared_ptr<Stmt>> body;
};

struct CompilationUnit {
  std::vector<std::string> nameSpace;
  std::vector<ClassDecl> classes;
  std::vector<GlobalFuncDecl> funcs;
  std::vector<FieldDecl> moduleVars;
};

} // namespace cvc
