#include "cvc/Parser.hpp"

namespace cvc {
Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& diag)
  : tokens_(std::move(tokens)), diag_(diag) {}

bool Parser::isAtEnd() { return peek().kind == TokenKind::EndOfFile; }
Token Parser::peek() { return tokens_[current_]; }
Token Parser::peekNext() { return (current_ + 1 >= tokens_.size()) ? tokens_.back() : tokens_[current_ + 1]; }
Token Parser::previous() { return tokens_[current_ - 1]; }
Token Parser::advance() { if (!isAtEnd()) current_++; return previous(); }
bool Parser::check(TokenKind kind) { return isAtEnd() ? false : peek().kind == kind; }
bool Parser::match(TokenKind kind) { if (check(kind)) { advance(); return true; } return false; }
void Parser::consume(TokenKind kind, const std::string& message) { if (check(kind)) { advance(); return; } diag_.report("", peek().loc, Severity::Error, "CV2002", message); }

std::string Parser::parseType() {
  std::string t = "";
  if (match(TokenKind::Identifier) || match(TokenKind::KwInt) || match(TokenKind::KwFloat) || match(TokenKind::KwString) || match(TokenKind::KwVoid) || match(TokenKind::KwAuto) || match(TokenKind::KwWeakMap)) {
    t = std::string(previous().lexeme);
    if (match(TokenKind::Less)) {
      t += "<" + parseType();
      while (match(TokenKind::Comma)) t += ", " + parseType();
      consume(TokenKind::Greater, "Expected '>'"); t += ">";
    }
    if (match(TokenKind::Question)) t += "?";
  }
  return t;
}

std::shared_ptr<Expr> Parser::parseExpr() { return parseAssignment(); }
std::shared_ptr<Expr> Parser::parseAssignment() {
  auto expr = parseLogicalOr();
  if (match(TokenKind::Equal) || match(TokenKind::PlusEqual) || match(TokenKind::MinusEqual)) {
    auto assignExpr = std::make_shared<Expr>();
    assignExpr->kind = ExprKind::Binary; assignExpr->op = previous(); assignExpr->left = expr; assignExpr->right = parseAssignment();
    return assignExpr;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseLogicalOr() {
  auto expr = parseLogicalAnd();
  while (match(TokenKind::PipePipe)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseLogicalAnd(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseLogicalAnd() {
  auto expr = parseEquality();
  while (match(TokenKind::AmpAmp)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseEquality(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseEquality() {
  auto expr = parseComparison();
  while (match(TokenKind::EqualEqual) || match(TokenKind::BangEqual)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseComparison(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseComparison() {
  auto expr = parseTerm();
  while (match(TokenKind::Less) || match(TokenKind::LessEqual) || match(TokenKind::Greater) || match(TokenKind::GreaterEqual)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseTerm(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseTerm() {
  auto expr = parseFactor();
  while (match(TokenKind::Plus) || match(TokenKind::Minus)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseFactor(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseFactor() {
  auto expr = parseUnary();
  while (match(TokenKind::Star) || match(TokenKind::Slash)) {
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = previous(); bin->left = expr; bin->right = parseUnary(); expr = bin;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parseUnary() {
  if (match(TokenKind::Bang) || match(TokenKind::Minus)) {
    auto un = std::make_shared<Expr>();
    un->kind = ExprKind::Unary; un->op = previous(); un->right = parseUnary(); return un;
  }
  return parseCall();
}
std::shared_ptr<Expr> Parser::parseCall() {
  auto expr = parsePrimary();
  while (true) {
    if (match(TokenKind::LParen)) {
      auto callExpr = std::make_shared<Expr>(); callExpr->kind = ExprKind::Call; callExpr->left = expr;
      if (!check(TokenKind::RParen)) do { callExpr->args.push_back(parseExpr()); } while (match(TokenKind::Comma));
      consume(TokenKind::RParen, "Expect ')'");
      if (match(TokenKind::Question)) callExpr->kind = ExprKind::FailableCall;
      expr = callExpr;
    } else if (match(TokenKind::LBracket)) {
      auto subExpr = std::make_shared<Expr>(); subExpr->kind = ExprKind::Subscript; subExpr->left = expr; subExpr->right = parseExpr();
      consume(TokenKind::RBracket, "Expect ']'"); expr = subExpr;
    } else if (match(TokenKind::Dot) || match(TokenKind::QuestionDot)) {
      auto memExpr = std::make_shared<Expr>(); memExpr->kind = ExprKind::MemberAccess; memExpr->left = expr; memExpr->op = previous();
      consume(TokenKind::Identifier, "Expect property"); memExpr->value = std::string(previous().lexeme); expr = memExpr;
    } else break;
  }
  return expr;
}
std::shared_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::Identifier)) { auto expr = std::make_shared<Expr>(); expr->kind = ExprKind::Identifier; expr->value = std::string(previous().lexeme); return expr; }
  if (match(TokenKind::IntLiteral) || match(TokenKind::FloatLiteral) || match(TokenKind::StringLiteral) || match(TokenKind::KwTrue) || match(TokenKind::KwFalse) || match(TokenKind::KwNull)) {
    auto expr = std::make_shared<Expr>(); expr->kind = ExprKind::Literal; expr->value = std::string(previous().lexeme); return expr;
  }
  if (match(TokenKind::KwNew)) {
    consume(TokenKind::Identifier, "Expect class");
    auto expr = std::make_shared<Expr>(); expr->kind = ExprKind::New; expr->value = std::string(previous().lexeme);
    consume(TokenKind::LParen, "Expect '('"); consume(TokenKind::RParen, "Expect ')'"); return expr;
  }
  if (match(TokenKind::LParen)) { auto expr = parseExpr(); consume(TokenKind::RParen, "Expect ')'"); return expr; }
  diag_.report("", peek().loc, Severity::Error, "CV2001", "Expected expression"); advance(); return nullptr;
}

std::shared_ptr<Stmt> Parser::parseVarDecl(bool expectSemicolon) {
  auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::VarDecl;
  stmt->isConst = match(TokenKind::KwConst);
  stmt->varType = match(TokenKind::KwAuto) ? "auto" : parseType();
  consume(TokenKind::Identifier, "Expect var name"); stmt->varName = std::string(previous().lexeme);
  if (match(TokenKind::Equal)) stmt->varInit = parseExpr();
  if (expectSemicolon) consume(TokenKind::Semicolon, "Expect ';'");
  return stmt;
}
std::shared_ptr<Stmt> Parser::parseIfStmt() {
  auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::If;
  consume(TokenKind::LParen, "Expect '('");
  if (peek().kind == TokenKind::KwAuto || (peek().kind == TokenKind::Identifier && peekNext().kind == TokenKind::Identifier)) stmt->varDeclCond = parseVarDecl(false);
  else stmt->expr = parseExpr();
  consume(TokenKind::RParen, "Expect ')'");
  stmt->body.push_back(parseStmt());
  if (match(TokenKind::KwElse)) stmt->elseBranch = parseStmt();
  return stmt;
}
std::shared_ptr<Stmt> Parser::parseBlock() {
  auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::Block;
  while (!check(TokenKind::RBrace) && !isAtEnd()) stmt->body.push_back(parseStmt());
  consume(TokenKind::RBrace, "Expect '}'"); return stmt;
}
std::shared_ptr<Stmt> Parser::parseStmt() {
  if (match(TokenKind::KwIf)) return parseIfStmt();
  if (match(TokenKind::LBrace)) return parseBlock();
  if (match(TokenKind::KwReturn)) {
    auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::Return;
    if (!check(TokenKind::Semicolon)) stmt->expr = parseExpr();
    consume(TokenKind::Semicolon, "Expect ';'"); return stmt;
  }
  if (match(TokenKind::KwFail)) {
    auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::Fail;
    consume(TokenKind::Semicolon, "Expect ';'"); return stmt;
  }
  if (peek().kind == TokenKind::KwConst || peek().kind == TokenKind::KwAuto || (peek().kind == TokenKind::Identifier && peekNext().kind == TokenKind::Identifier)) return parseVarDecl(true);
  auto stmt = std::make_shared<Stmt>(); stmt->kind = StmtKind::Expr; stmt->expr = parseExpr(); consume(TokenKind::Semicolon, "Expect ';'"); return stmt;
}

void Parser::parseClass(CompilationUnit& cu, const std::string& /*access*/) {
  ClassDecl cdecl; consume(TokenKind::Identifier, "Expect class"); cdecl.name = std::string(previous().lexeme);
  consume(TokenKind::LBrace, "Expect '{'");
  while (!check(TokenKind::RBrace) && !isAtEnd()) {
    std::string memAccess = match(TokenKind::KwPublic) ? "public" : match(TokenKind::KwProtected) ? "protected" : "private";
    if (match(TokenKind::KwPrivate)) {} // consume if present
    bool isConst = match(TokenKind::KwConst); bool isFailable = match(TokenKind::KwFailable);
    std::string type = parseType(); consume(TokenKind::Identifier, "Expect member"); std::string name = std::string(previous().lexeme);
    if (match(TokenKind::LParen)) {
      MethodDecl m; m.access = memAccess; m.returnType = type; m.name = name; m.isFailable = isFailable;
      if (!check(TokenKind::RParen)) do { std::string ptype = parseType(); consume(TokenKind::Identifier, "Expect param"); m.params.push_back({ptype, std::string(previous().lexeme)}); } while(match(TokenKind::Comma));
      consume(TokenKind::RParen, "Expect ')'"); if (match(TokenKind::KwConst)) m.isConst = true;
      consume(TokenKind::LBrace, "Expect '{'");
      while(!check(TokenKind::RBrace) && !isAtEnd()) m.body.push_back(parseStmt());
      consume(TokenKind::RBrace, "Expect '}'"); cdecl.methods.push_back(m);
    } else {
      FieldDecl f; f.access = memAccess; f.type = type; f.name = name; f.isConst = isConst;
      if (match(TokenKind::Equal)) f.init = parseExpr();
      consume(TokenKind::Semicolon, "Expect ';'"); cdecl.fields.push_back(f);
    }
  }
  consume(TokenKind::RBrace, "Expect '}'"); cu.classes.push_back(cdecl);
}

void Parser::parseFunctionOrVar(CompilationUnit& cu, const std::string& access) {
  bool isFailable = match(TokenKind::KwFailable); std::string type = parseType();
  consume(TokenKind::Identifier, "Expect name"); std::string name = std::string(previous().lexeme);
  if (match(TokenKind::LParen)) {
    GlobalFuncDecl func; func.access = access; func.isFailable = isFailable; func.returnType = type; func.name = name;
    if (!check(TokenKind::RParen)) do { std::string ptype = parseType(); consume(TokenKind::Identifier, "Expect param"); func.params.push_back({ptype, std::string(previous().lexeme)}); } while(match(TokenKind::Comma));
    consume(TokenKind::RParen, "Expect ')'"); consume(TokenKind::LBrace, "Expect '{'");
    while(!check(TokenKind::RBrace) && !isAtEnd()) func.body.push_back(parseStmt());
    consume(TokenKind::RBrace, "Expect '}'"); cu.funcs.push_back(func);
  } else {
    FieldDecl f; f.access = access; f.type = type; f.name = name;
    if (match(TokenKind::Equal)) f.init = parseExpr();
    consume(TokenKind::Semicolon, "Expect ';'"); cu.moduleVars.push_back(f);
  }
}

CompilationUnit Parser::parse() {
  CompilationUnit cu;
  while (!isAtEnd()) {
    if (match(TokenKind::KwNamespace)) {
      do { consume(TokenKind::Identifier, "Expect namespace"); cu.nameSpace.push_back(std::string(previous().lexeme)); } while(match(TokenKind::DoubleColon));
      consume(TokenKind::Semicolon, "Expect ';'"); continue;
    }
    std::string access = match(TokenKind::KwPublic) ? "public" : match(TokenKind::KwPrivate) ? "private" : "internal";
    if (match(TokenKind::KwClass)) parseClass(cu, access); else parseFunctionOrVar(cu, access);
  }
  return cu;
}
}
