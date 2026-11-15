#include "cvc/SourceMap.hpp"
#include <sstream>

namespace cvc {

void SourceMap::add(uint32_t genLine, uint32_t genCol, uint32_t srcLine, uint32_t srcCol, std::string symbol) {
    entries_.push_back(SourceMappingEntry{genLine, genCol, srcLine, srcCol, std::move(symbol)});
}

std::string SourceMap::toJson(const std::string& sourceFile, const std::string& generatedFile) const {
    std::ostringstream out;
    out << "{\n"
        << "  \"version\": 1,\n"
        << "  \"sourceFile\": \"" << sourceFile << "\",\n"
        << "  \"generatedFile\": \"" << generatedFile << "\",\n"
        << "  \"mappings\": [\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        out << "    { \"genLine\": " << e.genLine
            << ", \"genCol\": " << e.genCol
            << ", \"srcLine\": " << e.srcLine
            << ", \"srcCol\": " << e.srcCol;
        if (!e.symbol.empty()) {
            out << ", \"symbol\": \"" << e.symbol << "\"";
        }
        out << " }" << (i + 1 < entries_.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace cvc
