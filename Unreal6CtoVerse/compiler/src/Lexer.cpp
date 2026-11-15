#include "cvc/Lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace cvc {
namespace {
const std::unordered_map<std::string_view, TokenKind> kKeywords = {
  {"namespace", TokenKind::KwNamespace}, {"using", TokenKind::KwUsing},
  {"public", TokenKind::KwPublic}, {"private", TokenKind::KwPrivate}, {"protected", TokenKind::KwProtected},
  {"internal", TokenKind::KwInternal}, {"class", TokenKind::KwClass}, {"struct", TokenKind::KwStruct},
  {"interface", TokenKind::KwInterface}, {"enum", TokenKind::KwEnum}, {"const", TokenKind::KwConst},
  {"void", TokenKind::KwVoid}, {"bool", TokenKind::KwBool}, {"int", TokenKind::KwInt},
  {"float", TokenKind::KwFloat}, {"string", TokenKind::KwString}, {"auto", TokenKind::KwAuto},
  {"failable", TokenKind::KwFailable}, {"async", TokenKind::KwAsync}, {"await", TokenKind::KwAwait},
  {"spawn", TokenKind::KwSpawn}, {"concurrent", TokenKind::KwConcurrent}, {"race", TokenKind::KwRace},
  {"rush", TokenKind::KwRush}, {"loop", TokenKind::KwLoop}, {"if", TokenKind::KwIf},
  {"else", TokenKind::KwElse}, {"for", TokenKind::KwFor}, {"return", TokenKind::KwReturn},
  {"fail", TokenKind::KwFail}, {"break", TokenKind::KwBreak}, {"continue", TokenKind::KwContinue},
  {"new", TokenKind::KwNew}, {"null", TokenKind::KwNull}, {"true", TokenKind::KwTrue},
  {"false", TokenKind::KwFalse}, {"Map", TokenKind::KwMap}, {"WeakMap", TokenKind::KwWeakMap}
};
}

Lexer::Lexer(std::string filePath, std::string_view source, DiagnosticEngine& diag)
  : filePath_(std::move(filePath)), src_(source), diag_(diag) {
  if (src_.size() >= 3 && src_[0] == '\xEF' && src_[1] == '\xBB' && src_[2] == '\xBF') pos_ = 3;
}

char Lexer::peek(size_t offset) const { return (pos_ + offset < src_.size()) ? src_[pos_ + offset] : '\0'; }
char Lexer::advance() {
  char c = src_[pos_++];
  if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
  return c;
}

void Lexer::skipWhitespaceAndComments() {
  while (pos_ < src_.size()) {
    char c = peek();
    if (std::isspace(static_cast<unsigned char>(c))) { advance(); }
    else if (c == '/' && peek(1) == '/') { while (pos_ < src_.size() && peek() != '\n') advance(); }
    else if (c == '/' && peek(1) == '*') {
      advance(); advance();
      while (pos_ < src_.size() && !(peek() == '*' && peek(1) == '/')) advance();
      if (pos_ < src_.size()) { advance(); advance(); }
    } else { break; }
  }
}

Token Lexer::scanIdentifierOrKeyword() {
  SourceLocation loc{line_, col_};
  size_t start = pos_;
  while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
  std::string_view text = src_.substr(start, pos_ - start);
  auto it = kKeywords.find(text);
  return Token{it != kKeywords.end() ? it->second : TokenKind::Identifier, text, loc};
}

Token Lexer::scanNumber() {
  SourceLocation loc{line_, col_};
  size_t start = pos_;
  bool isFloat = false;
  while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
    isFloat = true; advance();
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
  }
  return Token{isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, src_.substr(start, pos_ - start), loc};
}

Token Lexer::scanString() {
  SourceLocation loc{line_, col_};
  size_t start = pos_;
  advance();
  while (pos_ < src_.size() && peek() != '"') {
    if (peek() == '\\' && pos_ + 1 < src_.size()) advance();
    advance();
  }
  if (pos_ < src_.size()) advance();
  return Token{TokenKind::StringLiteral, src_.substr(start, pos_ - start), loc};
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (true) {
    skipWhitespaceAndComments();
    if (pos_ >= src_.size()) { tokens.push_back(Token{TokenKind::EndOfFile, "", {line_, col_}}); break; }
    char c = peek();
    SourceLocation loc{line_, col_};
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') { tokens.push_back(scanIdentifierOrKeyword()); continue; }
    if (std::isdigit(static_cast<unsigned char>(c))) { tokens.push_back(scanNumber()); continue; }
    if (c == '"') { tokens.push_back(scanString()); continue; }
    advance();
    switch (c) {
      case '(': tokens.push_back({TokenKind::LParen, "(", loc}); break;
      case ')': tokens.push_back({TokenKind::RParen, ")", loc}); break;
      case '{': tokens.push_back({TokenKind::LBrace, "{", loc}); break;
      case '}': tokens.push_back({TokenKind::RBrace, "}", loc}); break;
      case '[': if (peek() == '[') { advance(); tokens.push_back({TokenKind::DoubleLBracket, "[[", loc}); } else tokens.push_back({TokenKind::LBracket, "[", loc}); break;
      case ']': if (peek() == ']') { advance(); tokens.push_back({TokenKind::DoubleRBracket, "]]", loc}); } else tokens.push_back({TokenKind::RBracket, "]", loc}); break;
      case ';': tokens.push_back({TokenKind::Semicolon, ";", loc}); break;
      case ',': tokens.push_back({TokenKind::Comma, ",", loc}); break;
      case '.': tokens.push_back({TokenKind::Dot, ".", loc}); break;
      case ':': if (peek() == ':') { advance(); tokens.push_back({TokenKind::DoubleColon, "::", loc}); } else tokens.push_back({TokenKind::Colon, ":", loc}); break;
      case '?': if (peek() == '?') { advance(); tokens.push_back({TokenKind::DoubleQuestion, "??", loc}); } else if (peek() == '.') { advance(); tokens.push_back({TokenKind::QuestionDot, "?.", loc}); } else tokens.push_back({TokenKind::Question, "?", loc}); break;
      case '=': if (peek() == '=') { advance(); tokens.push_back({TokenKind::EqualEqual, "==", loc}); } else tokens.push_back({TokenKind::Equal, "=", loc}); break;
      case '!': if (peek() == '=') { advance(); tokens.push_back({TokenKind::BangEqual, "!=", loc}); } else tokens.push_back({TokenKind::Bang, "!", loc}); break;
      case '+': if (peek() == '=') { advance(); tokens.push_back({TokenKind::PlusEqual, "+=", loc}); } else tokens.push_back({TokenKind::Plus, "+", loc}); break;
      case '-': if (peek() == '=') { advance(); tokens.push_back({TokenKind::MinusEqual, "-=", loc}); } else if (peek() == '>') { advance(); tokens.push_back({TokenKind::Arrow, "->", loc}); } else tokens.push_back({TokenKind::Minus, "-", loc}); break;
      case '*': if (peek() == '=') { advance(); tokens.push_back({TokenKind::StarEqual, "*=", loc}); } else tokens.push_back({TokenKind::Star, "*", loc}); break;
      case '/': if (peek() == '=') { advance(); tokens.push_back({TokenKind::SlashEqual, "/=", loc}); } else tokens.push_back({TokenKind::Slash, "/", loc}); break;
      case '<': if (peek() == '=') { advance(); tokens.push_back({TokenKind::LessEqual, "<=", loc}); } else tokens.push_back({TokenKind::Less, "<", loc}); break;
      case '>': if (peek() == '=') { advance(); tokens.push_back({TokenKind::GreaterEqual, ">=", loc}); } else tokens.push_back({TokenKind::Greater, ">", loc}); break;
      case '&': if (peek() == '&') { advance(); tokens.push_back({TokenKind::AmpAmp, "&&", loc}); } else { diag_.report(filePath_, loc, Severity::Error, "CV1002", "Address-of '&' unsupported"); } break;
      case '|': if (peek() == '|') { advance(); tokens.push_back({TokenKind::PipePipe, "||", loc}); } break;
      default: { char buf[128]; snprintf(buf, sizeof(buf), "Unexpected character '%c' (0x%02X) in source.", std::isprint(static_cast<unsigned char>(c)) ? c : '?', static_cast<unsigned char>(c)); diag_.report(filePath_, loc, Severity::Error, "CV1001", buf); break; }
    }
  }
  return tokens;
}
}
