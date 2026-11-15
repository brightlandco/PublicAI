#include "cvc/Diagnostics.hpp"
#include "cvc/Lexer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: cvc <input.cv> [-o <output.verse>] [--sourcemap]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath;
    bool sourcemap = false;

    // Parse flags safely
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--sourcemap") {
            sourcemap = true;
        }
    }

    std::ifstream in(inputPath, std::ios::binary); // Open as binary to avoid Windows LF mutation
    if (!in) {
        std::cerr << "cvc: error: could not open input file '" << inputPath << "'\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    // CRITICAL: We MUST store the buffer.str() in a local variable.
    // If we pass buffer.str() directly to Lexer, the std::string temporary is destroyed,
    // and the Lexer's std::string_view holds a dangling pointer to garbage memory.
    std::string source = buffer.str();

    cvc::DiagnosticEngine diag;
    cvc::Lexer lexer(inputPath, source, diag);
    auto tokens = lexer.tokenize();

    if (diag.hasErrors()) {
        diag.flush();
        return 1;
    }

    std::cout << "cvc: tokenized " << tokens.size() << " tokens from " << inputPath << "\n";
    if (!outputPath.empty()) {
        std::cout << "cvc: (Stub) ready to write AST to " << outputPath << "\n";
    }
    
    return 0;
}
