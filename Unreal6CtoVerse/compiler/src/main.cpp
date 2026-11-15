#include "cvc/Diagnostics.hpp"
#include "cvc/Lexer.hpp"
#include "cvc/Parser.hpp"
#include "cvc/VerseEmitter.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <system_error>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: cvc <input.cv> [-o <output.verse>] [--sourcemap]\n";
    return 1;
  }

  std::string inputPath = argv[1];
  std::string outputPath;
  bool sourcemap = false;
  (void)sourcemap;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
    else if (arg == "--sourcemap") sourcemap = true;
  }

  std::ifstream in(inputPath, std::ios::binary);
  if (!in) {
    std::cerr << "cvc: error: could not open input file '" << inputPath << "'\n";
    return 1;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  std::string source = buffer.str();

  std::cout << "[cvc] Lexing " << inputPath << "...\n";
  cvc::DiagnosticEngine diag;
  cvc::Lexer lexer(inputPath, source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) { diag.flush(); return 1; }
  std::cout << "[cvc] Lexer finished. Produced " << tokens.size() << " tokens.\n";

  std::cout << "[cvc] Parsing AST...\n";
  cvc::Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) { diag.flush(); return 1; }
  std::cout << "[cvc] Parser finished successfully.\n";

  if (!outputPath.empty()) {
    std::cout << "[cvc] Emitting Verse code to " << outputPath << "...\n";
    std::filesystem::path outPath(outputPath);
    if (outPath.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(outPath.parent_path(), ec);
      if (ec) {
        std::cerr << "cvc: warning: failed to create directories: " << ec.message() << "\n";
      }
    }
    std::ofstream out(outputPath);
    if (!out) { 
      std::cerr << "cvc: error: could not open output file '" << outputPath << "'\n"; 
      return 1; 
    }
    
    cvc::VerseEmitter emitter;
    emitter.emit(ast, out);
    std::cout << "cvc: generated " << outputPath << " successfully!\n";
  }
  return 0;
}
