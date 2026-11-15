#pragma once
#include "Diagnostics.hpp"
#include "Token.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace cvc {
class Lexer {
public:
  Lexer(std::string filePath, std::string_view source, DiagnosticEngine& diag);
  std::vector<Token> tokenize();
private:
  char peek(size_t offset = 0) const;
  char advance();
  void skipWhitespaceAndComments();
  Token scanIdentifierOrKeyword();
  Token scanNumber();
  Token scanString();
  std::string filePath_;
  std::string_view src_;
  DiagnosticEngine& diag_;
  size_t pos_ = 0;
  uint32_t line_ = 1;
  uint32_t col_ = 1;
};
}
