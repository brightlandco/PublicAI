#pragma once
#include "AST.hpp"
#include <ostream>

namespace cvc {
class VerseEmitter {
public:
  void emit(const CompilationUnit& cu, std::ostream& out);
private:
  int indentLevel_ = 0;
  std::ostream* out_ = nullptr;
  void indent();
  void emitStmt(const std::shared_ptr<Stmt>& stmt);
  void emitExpr(const std::shared_ptr<Expr>& expr);
  std::string capitalize(const std::string& s);
  std::string snakeCase(const std::string& s);
  std::string verseType(const std::string& t);
};
} // namespace cvc
