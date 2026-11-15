#include "cvc/VerseEmitter.hpp"
#include <cctype>

namespace cvc {
std::string VerseEmitter::capitalize(const std::string& s) {
  if (s.empty()) return s; std::string res = s; res[0] = toupper(res[0]); return res;
}
std::string VerseEmitter::snakeCase(const std::string& s) {
  if (s.empty()) return s; std::string res; res += tolower(s[0]);
  for (size_t i = 1; i < s.size(); ++i) {
    if (isupper(s[i])) { res += "_"; res += tolower(s[i]); } else { res += s[i]; }
  }
  return res;
}
std::string VerseEmitter::verseType(const std::string& t) {
  if (t == "int" || t == "float" || t == "string" || t == "void") return t;
  if (t == "Player") return "player";
  if (t == "Wallet") return "wallet";
  if (t.find("WeakMap<") == 0) {
    size_t comma = t.find(","); std::string K = t.substr(8, comma - 8); std::string V = t.substr(comma + 2, t.size() - comma - 3);
    return "weak_map(" + verseType(K) + ", " + verseType(V) + ")";
  }
  if (t == "auto") return "auto";
  return t;
}
void VerseEmitter::indent() { for (int i = 0; i < indentLevel_; ++i) *out_ << "  "; }

void VerseEmitter::emitExpr(const std::shared_ptr<Expr>& expr) {
  if (!expr) return;
  switch (expr->kind) {
    case ExprKind::Identifier: *out_ << capitalize(expr->value); break;
    case ExprKind::Literal: *out_ << (expr->value == "null" ? "false" : expr->value); break;
    case ExprKind::New: *out_ << snakeCase(expr->value) << "{}"; break;
    case ExprKind::Unary: *out_ << expr->op.lexeme; emitExpr(expr->right); break;
    case ExprKind::Binary: {
      emitExpr(expr->left);
      std::string op = std::string(expr->op.lexeme);
      if (op == "||") op = "or"; else if (op == "&&") op = "and"; else if (op == "==") op = "="; else if (op == "+=" || op == "-=") op = std::string(1, op[0]);
      if (expr->op.kind == TokenKind::MinusEqual || expr->op.kind == TokenKind::PlusEqual || expr->op.kind == TokenKind::Equal) *out_ << " " << expr->op.lexeme << " ";
      else *out_ << " " << op << " ";
      emitExpr(expr->right); break;
    }
    case ExprKind::Call:
    case ExprKind::FailableCall: {
      emitExpr(expr->left); *out_ << (expr->kind == ExprKind::FailableCall ? "[" : "(");
      for (size_t i = 0; i < expr->args.size(); ++i) { if (i > 0) *out_ << ", "; emitExpr(expr->args[i]); }
      *out_ << (expr->kind == ExprKind::FailableCall ? "]" : ")"); break;
    }
    case ExprKind::Subscript: emitExpr(expr->left); *out_ << "["; emitExpr(expr->right); *out_ << "]"; break;
    case ExprKind::MemberAccess: emitExpr(expr->left); *out_ << expr->op.lexeme << capitalize(expr->value); break;
  }
}

void VerseEmitter::emitStmt(const std::shared_ptr<Stmt>& stmt) {
  if (!stmt) return;
  if (stmt->kind == StmtKind::Block) { for (auto& s : stmt->body) emitStmt(s); }
  else if (stmt->kind == StmtKind::Expr) {
    indent();
    if (stmt->expr->kind == ExprKind::Binary && (stmt->expr->op.kind == TokenKind::Equal || stmt->expr->op.kind == TokenKind::MinusEqual || stmt->expr->op.kind == TokenKind::PlusEqual)) *out_ << "set ";
    emitExpr(stmt->expr); *out_ << "\n";
  }
  else if (stmt->kind == StmtKind::If) {
    indent(); *out_ << "if (";
    if (stmt->varDeclCond) { *out_ << capitalize(stmt->varDeclCond->varName) << " := "; emitExpr(stmt->varDeclCond->varInit); }
    else { emitExpr(stmt->expr); }
    *out_ << "):\n"; indentLevel_++;
    bool hasBody = false;
    for (auto& s : stmt->body) {
      if (s->kind == StmtKind::Fail) { indent(); *out_ << "false?\n"; hasBody = true; }
      else if (s->kind == StmtKind::Block) { for (auto& bs : s->body) { emitStmt(bs); hasBody = true; } }
      else { emitStmt(s); hasBody = true; }
    }
    if (!hasBody) { indent(); *out_ << "{}\n"; }
    indentLevel_--;
  }
  else if (stmt->kind == StmtKind::Return) { indent(); *out_ << "return "; if (stmt->expr) emitExpr(stmt->expr); *out_ << "\n"; }
  else if (stmt->kind == StmtKind::VarDecl) {
    indent();
    if (!stmt->isConst) {
      *out_ << "var " << capitalize(stmt->varName) << " : " << (stmt->varType == "auto" ? "any" : verseType(stmt->varType)) << " = ";
      if (stmt->varInit) emitExpr(stmt->varInit); else *out_ << "false";
    } else {
      *out_ << capitalize(stmt->varName) << " := ";
      if (stmt->varInit) emitExpr(stmt->varInit);
    }
    *out_ << "\n";
  }
}

void VerseEmitter::emit(const CompilationUnit& cu, std::ostream& out) {
  out_ = &out;
  for (const auto& ns : cu.nameSpace) { indent(); *out_ << capitalize(ns) << "<public> := module:\n"; indentLevel_++; }
  for (const auto& cls : cu.classes) {
    indent(); *out_ << snakeCase(cls.name) << "<public> := class:\n"; indentLevel_++;
    for (const auto& f : cls.fields) {
      indent(); if (!f.isConst) *out_ << "var";
      *out_ << "<" << f.access << "> " << capitalize(f.name) << " : " << verseType(f.type) << " = ";
      if (f.init) emitExpr(f.init); else *out_ << "false";
      *out_ << "\n";
    }
    if (!cls.fields.empty() && !cls.methods.empty()) *out_ << "\n";
    for (size_t i=0; i<cls.methods.size(); ++i) {
      const auto& m = cls.methods[i];
      indent(); *out_ << capitalize(m.name) << "<" << m.access << ">(";
      for (size_t p=0; p<m.params.size(); ++p) {
        if (p > 0) *out_ << ", ";
        *out_ << capitalize(m.params[p].second) << " : " << verseType(m.params[p].first);
      }
      *out_ << ")";
      if (m.isFailable) *out_ << "<decides><transacts>";
      else if (m.isConst) *out_ << "<reads>";
      else if (m.name.find("GetOrCreate") != std::string::npos) *out_ << "<transacts>";
      *out_ << " : " << verseType(m.returnType) << " =\n";
      indentLevel_++; for (auto& s : m.body) emitStmt(s); indentLevel_--;
      if (i < cls.methods.size() - 1) *out_ << "\n";
    }
    indentLevel_--; *out_ << "\n";
  }
  for (const auto& mv : cu.moduleVars) {
    indent();
    if (mv.type.find("WeakMap") != std::string::npos) *out_ << "var<" << mv.access << "> " << capitalize(mv.name) << " : " << verseType(mv.type) << " = map{}\n\n";
  }
  for (const auto& func : cu.funcs) {
    indent(); *out_ << capitalize(func.name) << "<" << func.access << ">(";
    for (size_t p=0; p<func.params.size(); ++p) {
      if (p > 0) *out_ << ", ";
      *out_ << capitalize(func.params[p].second) << " : " << verseType(func.params[p].first);
    }
    *out_ << ")";
    if (func.isFailable) *out_ << "<decides><transacts>"; else *out_ << "<transacts>";
    *out_ << " : " << verseType(func.returnType) << " =\n";
    indentLevel_++; for (auto& s : func.body) emitStmt(s); indentLevel_--;
  }
}
}
