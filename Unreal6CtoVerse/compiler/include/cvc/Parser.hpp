#pragma once
#include "AST.hpp"
#include "Lexer.hpp"
#include "Diagnostics.hpp"

namespace cvc {
class Parser {
public:
  Parser(std::vector<Token> tokens, DiagnosticEngine& diag);
  CompilationUnit parse();
private:
  std::vector<Token> tokens_;
  DiagnosticEngine& diag_;
  size_t current_ = 0;

  bool isAtEnd(); Token peek(); Token peekNext(); Token previous(); Token advance();
  bool check(TokenKind kind); bool match(TokenKind kind); void consume(TokenKind kind, const std::string& message);

  std::string parseType();
  std::shared_ptr<Expr> parseExpr(); std::shared_ptr<Expr> parseAssignment(); std::shared_ptr<Expr> parseLogicalOr();
  std::shared_ptr<Expr> parseLogicalAnd(); std::shared_ptr<Expr> parseEquality(); std::shared_ptr<Expr> parseComparison();
  std::shared_ptr<Expr> parseTerm(); std::shared_ptr<Expr> parseFactor(); std::shared_ptr<Expr> parseUnary();
  std::shared_ptr<Expr> parseCall(); std::shared_ptr<Expr> parsePrimary();

  std::shared_ptr<Stmt> parseStmt(); std::shared_ptr<Stmt> parseIfStmt(); std::shared_ptr<Stmt> parseBlock();
  std::shared_ptr<Stmt> parseVarDecl(bool expectSemicolon = true);

  void parseClass(CompilationUnit& cu, const std::string& access);
  void parseFunctionOrVar(CompilationUnit& cu, const std::string& access);
};
}
