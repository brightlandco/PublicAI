#include "cvc/Parser.hpp"

namespace cvc {

Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& diag)
  : tokens_(std::move(tokens)), diag_(diag) {}

bool Parser::isAtEnd() { return peek().kind == TokenKind::EndOfFile; }
Token Parser::peek() { return tokens_[current_]; }
Token Parser::peekNext() {
  if (current_ + 1 >= tokens_.size()) return tokens_.back();
  return tokens_[current_ + 1];
}
Token Parser::previous() { return tokens_[current_ - 1]; }
Token Parser::advance() {
  if (!isAtEnd()) current_++;
  return previous();
}
bool Parser::check(TokenKind kind) {
  if (isAtEnd()) return false;
  return peek().kind == kind;
}
bool Parser::match(TokenKind kind) {
  if (check(kind)) { advance(); return true; }
  return false;
}
void Parser::consume(TokenKind kind, const std::string& message) {
  if (check(kind)) { advance(); return; }
  diag_.report("", peek().loc, Severity::Error, "CV2002", message);
  if (!isAtEnd()) advance(); // Always advance to prevent infinite loops
}

std::string Parser::parseType() {
  std::string t = "";
  if (match(TokenKind::Identifier) || match(TokenKind::KwInt) || match(TokenKind::KwFloat) ||
      match(TokenKind::KwString) || match(TokenKind::KwVoid) || match(TokenKind::KwBool) ||
      match(TokenKind::KwAuto) || match(TokenKind::KwMap) || match(TokenKind::KwWeakMap)) {
    t = std::string(previous().lexeme);
    if (match(TokenKind::Less)) {
      t += "<";
      t += parseType();
      while (match(TokenKind::Comma)) {
        t += ", ";
        t += parseType();
      }
      consume(TokenKind::Greater, "Expected '>' in generic type.");
      t += ">";
    }
    if (match(TokenKind::Question)) {
      t += "?";
    }
  }
  return t;
}

std::shared_ptr<Expr> Parser::parseExpr() { return parseAssignment(); }

std::shared_ptr<Expr> Parser::parseAssignment() {
  auto expr = parseLogicalOr();
  if (match(TokenKind::Equal) || match(TokenKind::PlusEqual) || match(TokenKind::MinusEqual) ||
      match(TokenKind::StarEqual) || match(TokenKind::SlashEqual)) {
    Token op = previous();
    auto value = parseAssignment();
    auto assignExpr = std::make_shared<Expr>();
    assignExpr->kind = ExprKind::Binary;
    assignExpr->op = op;
    assignExpr->left = expr;
    assignExpr->right = value;
    return assignExpr;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseLogicalOr() {
  auto expr = parseLogicalAnd();
  while (match(TokenKind::PipePipe) || match(TokenKind::DoubleQuestion)) {
    Token op = previous();
    auto right = parseLogicalAnd();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseLogicalAnd() {
  auto expr = parseEquality();
  while (match(TokenKind::AmpAmp)) {
    Token op = previous();
    auto right = parseEquality();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseEquality() {
  auto expr = parseComparison();
  while (match(TokenKind::EqualEqual) || match(TokenKind::BangEqual)) {
    Token op = previous();
    auto right = parseComparison();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseComparison() {
  auto expr = parseTerm();
  while (match(TokenKind::Less) || match(TokenKind::LessEqual) || match(TokenKind::Greater) || match(TokenKind::GreaterEqual)) {
    Token op = previous();
    auto right = parseTerm();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseTerm() {
  auto expr = parseFactor();
  while (match(TokenKind::Plus) || match(TokenKind::Minus)) {
    Token op = previous();
    auto right = parseFactor();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseFactor() {
  auto expr = parseUnary();
  while (match(TokenKind::Star) || match(TokenKind::Slash)) {
    Token op = previous();
    auto right = parseUnary();
    auto bin = std::make_shared<Expr>();
    bin->kind = ExprKind::Binary; bin->op = op; bin->left = expr; bin->right = right;
    expr = bin;
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parseUnary() {
  if (match(TokenKind::Bang) || match(TokenKind::Minus)) {
    Token op = previous();
    auto right = parseUnary();
    auto un = std::make_shared<Expr>();
    un->kind = ExprKind::Unary; un->op = op; un->right = right;
    return un;
  }
  return parseCall();
}

std::shared_ptr<Expr> Parser::parseCall() {
  auto expr = parsePrimary();
  while (true) {
    if (match(TokenKind::LParen)) {
      auto callExpr = std::make_shared<Expr>();
      callExpr->kind = ExprKind::Call;
      callExpr->left = expr;
      if (!check(TokenKind::RParen)) {
        do {
          callExpr->args.push_back(parseExpr());
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RParen, "Expect ')' after arguments.");
      if (match(TokenKind::Question)) callExpr->kind = ExprKind::FailableCall;
      expr = callExpr;
    } else if (match(TokenKind::LBracket)) {
      auto subExpr = std::make_shared<Expr>();
      subExpr->kind = ExprKind::Subscript;
      subExpr->left = expr;
      subExpr->right = parseExpr();
      consume(TokenKind::RBracket, "Expect ']' after index.");
      expr = subExpr;
    } else if (match(TokenKind::Dot) || match(TokenKind::QuestionDot)) {
      auto memExpr = std::make_shared<Expr>();
      memExpr->kind = ExprKind::MemberAccess;
      memExpr->left = expr;
      memExpr->op = previous();
      consume(TokenKind::Identifier, "Expect property name.");
      memExpr->value = std::string(previous().lexeme);
      expr = memExpr;
    } else {
      break;
    }
  }
  return expr;
}

std::shared_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::Identifier)) {
    std::string ident = std::string(previous().lexeme);
    // Support struct initializer syntax: ThrusterTelemetry{ ... }
    if (match(TokenKind::LBrace)) {
      auto expr = std::make_shared<Expr>();
      expr->kind = ExprKind::New;
      expr->value = ident;
      while (!check(TokenKind::RBrace) && !isAtEnd()) {
        expr->args.push_back(parseExpr());
        match(TokenKind::Comma);
      }
      consume(TokenKind::RBrace, "Expect '}' after struct initializer.");
      return expr;
    }
    auto expr = std::make_shared<Expr>();
    expr->kind = ExprKind::Identifier; expr->value = ident;
    return expr;
  }
  if (match(TokenKind::IntLiteral) || match(TokenKind::FloatLiteral) || match(TokenKind::StringLiteral) ||
      match(TokenKind::KwTrue) || match(TokenKind::KwFalse) || match(TokenKind::KwNull)) {
    auto expr = std::make_shared<Expr>();
    expr->kind = ExprKind::Literal; expr->value = std::string(previous().lexeme);
    return expr;
  }
  if (match(TokenKind::KwAwait)) { return parseExpr(); }
  if (match(TokenKind::KwNew)) {
    consume(TokenKind::Identifier, "Expect class name after new.");
    auto expr = std::make_shared<Expr>();
    expr->kind = ExprKind::New; expr->value = std::string(previous().lexeme);
    consume(TokenKind::LParen, "Expect '(' after class name.");
    consume(TokenKind::RParen, "Expect ')' after class name.");
    return expr;
  }
  if (match(TokenKind::LParen)) {
    auto expr = parseExpr();
    consume(TokenKind::RParen, "Expect ')' after expression.");
    return expr;
  }
  diag_.report("", peek().loc, Severity::Error, "CV2001", "Expected expression.");
  advance();
  return nullptr;
}

static bool isTypeToken(TokenKind k) {
  return k == TokenKind::KwInt || k == TokenKind::KwFloat || k == TokenKind::KwString ||
         k == TokenKind::KwBool || k == TokenKind::KwVoid || k == TokenKind::KwAuto ||
         k == TokenKind::Identifier;
}

std::shared_ptr<Stmt> Parser::parseVarDecl(bool expectSemicolon) {
  auto stmt = std::make_shared<Stmt>();
  stmt->kind = StmtKind::VarDecl;
  stmt->isConst = match(TokenKind::KwConst);
  if (match(TokenKind::KwAuto)) {
    stmt->varType = "auto";
  } else {
    stmt->varType = parseType();
  }
  consume(TokenKind::Identifier, "Expect variable name.");
  stmt->varName = std::string(previous().lexeme);
  if (match(TokenKind::Equal)) {
    stmt->varInit = parseExpr();
  }
  if (expectSemicolon) consume(TokenKind::Semicolon, "Expect ';' after variable declaration.");
  return stmt;
}

std::shared_ptr<Stmt> Parser::parseIfStmt() {
  auto stmt = std::make_shared<Stmt>();
  stmt->kind = StmtKind::If;
  consume(TokenKind::LParen, "Expect '(' after 'if'.");
  
  if (peek().kind == TokenKind::KwAuto || (isTypeToken(peek().kind) && peekNext().kind == TokenKind::Identifier)) {
    stmt->varDeclCond = parseVarDecl(false);
  } else {
    stmt->expr = parseExpr();
  }
  consume(TokenKind::RParen, "Expect ')' after if condition.");
  
  stmt->body.push_back(parseStmt());
  
  if (match(TokenKind::KwElse)) {
    stmt->elseBranch = parseStmt();
  }
  return stmt;
}

std::shared_ptr<Stmt> Parser::parseBlock() {
  auto stmt = std::make_shared<Stmt>();
  stmt->kind = StmtKind::Block;
  while (!check(TokenKind::RBrace) && !isAtEnd()) {
    stmt->body.push_back(parseStmt());
  }
  consume(TokenKind::RBrace, "Expect '}' after block.");
  return stmt;
}

std::shared_ptr<Stmt> Parser::parseStmt() {
  if (match(TokenKind::KwIf)) return parseIfStmt();
  if (match(TokenKind::LBrace)) return parseBlock();
  if (match(TokenKind::KwLoop) || match(TokenKind::KwRace) || match(TokenKind::KwConcurrent) || match(TokenKind::KwRush)) {
    consume(TokenKind::LBrace, "Expect '{' after structured concurrency keyword.");
    return parseBlock();
  }
  if (match(TokenKind::KwSpawn)) {
    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::Expr;
    stmt->expr = parseExpr();
    consume(TokenKind::Semicolon, "Expect ';' after spawn.");
    return stmt;
  }
  if (match(TokenKind::KwReturn)) {
    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::Return;
    if (!check(TokenKind::Semicolon)) stmt->expr = parseExpr();
    consume(TokenKind::Semicolon, "Expect ';' after return.");
    return stmt;
  }
  if (match(TokenKind::KwFail)) {
    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::Fail;
    consume(TokenKind::Semicolon, "Expect ';' after fail.");
    return stmt;
  }
  
  if (peek().kind == TokenKind::KwConst || peek().kind == TokenKind::KwAuto ||
      (isTypeToken(peek().kind) && peekNext().kind == TokenKind::Identifier)) {
    return parseVarDecl(true);
  }
  
  auto stmt = std::make_shared<Stmt>();
  stmt->kind = StmtKind::Expr;
  stmt->expr = parseExpr();
  consume(TokenKind::Semicolon, "Expect ';' after expression.");
  return stmt;
}

void Parser::parseClass(CompilationUnit& cu, const std::string& defaultAccess) {
  ClassDecl cdecl;
  consume(TokenKind::Identifier, "Expect class/struct name.");
  cdecl.name = std::string(previous().lexeme);
  if (match(TokenKind::Colon)) {
    consume(TokenKind::Identifier, "Expect base class name.");
  }
  consume(TokenKind::LBrace, "Expect '{' before class body.");
  
  std::string currentAccess = defaultAccess;

  while (!check(TokenKind::RBrace) && !isAtEnd()) {
    // 1. Check for C++ access specifier blocks (public:, private:, protected:, internal:)
    //    or optional inline per-member access modifiers
    if (check(TokenKind::KwPublic) || check(TokenKind::KwPrivate) ||
        check(TokenKind::KwProtected) || check(TokenKind::KwInternal)) {
      if (match(TokenKind::KwPublic)) currentAccess = "public";
      else if (match(TokenKind::KwPrivate)) currentAccess = "private";
      else if (match(TokenKind::KwProtected)) currentAccess = "protected";
      else if (match(TokenKind::KwInternal)) currentAccess = "internal";
      
      if (match(TokenKind::Colon)) {
        continue; // It was a `public:` or `private:` label!
      }
    }

    // 2. Consume optional attributes like [[editable]]
    while (match(TokenKind::DoubleLBracket)) {
      while (!check(TokenKind::DoubleRBracket) && !isAtEnd()) advance();
      consume(TokenKind::DoubleRBracket, "Expect ']]' after attribute.");
    }

    // 3. Optional member qualifiers
    bool isConst = match(TokenKind::KwConst);
    bool isFailable = match(TokenKind::KwFailable);
    bool isAsync = match(TokenKind::KwAsync);
    (void)isAsync;
    
    std::string type = parseType();
    if (type.empty()) {
      diag_.report("", peek().loc, Severity::Error, "CV2003", "Expected member type.");
      advance();
      continue;
    }
    consume(TokenKind::Identifier, "Expect member name.");
    std::string name = std::string(previous().lexeme);
    
    if (match(TokenKind::LParen)) {
      MethodDecl m;
      m.access = currentAccess;
      m.returnType = type;
      m.name = name;
      m.isFailable = isFailable;
      if (!check(TokenKind::RParen)) {
        do {
          std::string ptype = parseType();
          consume(TokenKind::Identifier, "Expect parameter name.");
          m.params.push_back({ptype, std::string(previous().lexeme)});
        } while(match(TokenKind::Comma));
      }
      consume(TokenKind::RParen, "Expect ')' after parameters.");
      if (match(TokenKind::KwConst)) m.isConst = true;
      consume(TokenKind::LBrace, "Expect '{' before method body.");
      while(!check(TokenKind::RBrace) && !isAtEnd()) {
        m.body.push_back(parseStmt());
      }
      consume(TokenKind::RBrace, "Expect '}' after method body.");
      cdecl.methods.push_back(m);
    } else {
      FieldDecl f;
      f.access = currentAccess;
      f.type = type;
      f.name = name;
      f.isConst = isConst;
      if (match(TokenKind::Equal)) {
        f.init = parseExpr();
      }
      consume(TokenKind::Semicolon, "Expect ';' after field declaration.");
      cdecl.fields.push_back(f);
    }
  }
  consume(TokenKind::RBrace, "Expect '}' after class body.");
  match(TokenKind::Semicolon); // Allow optional C++ trailing semicolon after class/struct `};`
  cu.classes.push_back(cdecl);
}

void Parser::parseFunctionOrVar(CompilationUnit& cu, const std::string& access) {
  bool isFailable = match(TokenKind::KwFailable);
  bool isAsync = match(TokenKind::KwAsync);
  (void)isAsync;
  std::string type = parseType();
  if (type.empty()) {
    diag_.report("", peek().loc, Severity::Error, "CV2004", "Expected top-level declaration.");
    advance();
    return;
  }
  consume(TokenKind::Identifier, "Expect name.");
  std::string name = std::string(previous().lexeme);
  
  if (match(TokenKind::LParen)) {
    GlobalFuncDecl func;
    func.access = access;
    func.isFailable = isFailable;
    func.returnType = type;
    func.name = name;
    if (!check(TokenKind::RParen)) {
      do {
        std::string ptype = parseType();
        consume(TokenKind::Identifier, "Expect parameter name.");
        func.params.push_back({ptype, std::string(previous().lexeme)});
      } while(match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "Expect ')' after parameters.");
    consume(TokenKind::LBrace, "Expect '{' before function body.");
    while(!check(TokenKind::RBrace) && !isAtEnd()) {
      func.body.push_back(parseStmt());
    }
    consume(TokenKind::RBrace, "Expect '}' after function body.");
    cu.funcs.push_back(func);
  } else {
    FieldDecl f;
    f.access = access;
    f.type = type;
    f.name = name;
    if (match(TokenKind::Equal)) f.init = parseExpr();
    consume(TokenKind::Semicolon, "Expect ';' after global variable.");
    cu.moduleVars.push_back(f);
  }
}

CompilationUnit Parser::parse() {
  CompilationUnit cu;
  
  while (!isAtEnd()) {
    if (match(TokenKind::KwNamespace)) {
      do {
        consume(TokenKind::Identifier, "Expect namespace part.");
        cu.nameSpace.push_back(std::string(previous().lexeme));
      } while(match(TokenKind::DoubleColon));
      consume(TokenKind::Semicolon, "Expect ';' after namespace.");
      continue;
    }
    
    std::string access = "public";
    if (match(TokenKind::KwPublic)) access = "public";
    else if (match(TokenKind::KwPrivate)) access = "private";
    else if (match(TokenKind::KwInternal)) access = "internal";
    
    if (match(TokenKind::KwClass)) {
      parseClass(cu, "private");
    } else if (match(TokenKind::KwStruct)) {
      parseClass(cu, "public");
    } else {
      parseFunctionOrVar(cu, access);
    }
  }
  
  return cu;
}

} // namespace cvc
