#pragma once

#include <cstdint>
#include <string_view>

namespace cvc {

enum class TokenKind : uint16_t {
    EndOfFile,
    Invalid,

    // Identifiers & Literals
    Identifier,
    IntLiteral,
    FloatLiteral,
    StringLiteral,

    // Keywords
    KwNamespace,
    KwUsing,
    KwPublic,
    KwPrivate,
    KwProtected,
    KwInternal,
    KwClass,
    KwStruct,
    KwInterface,
    KwEnum,
    KwMut,
    KwConst,
    KwVoid,
    KwBool,
    KwInt,
    KwFloat,
    KwString,
    KwAuto,
    KwFailable,
    KwAsync,
    KwAwait,
    KwSpawn,
    KwConcurrent,
    KwRace,
    KwRush,
    KwLoop,
    KwIf,
    KwElse,
    KwFor,
    KwReturn,
    KwFail,
    KwBreak,
    KwContinue,
    KwNew,
    KwNull,
    KwTrue,
    KwFalse,
    KwMap,
    KwWeakMap,

    // Punctuation & Operators
    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,
    DoubleLBracket, DoubleRBracket,
    Semicolon,
    Colon,
    DoubleColon,
    Comma,
    Dot,
    Question,
    DoubleQuestion,
    QuestionDot,
    Arrow,
    Equal,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    AmpAmp,
    PipePipe,
    Bang
};

struct SourceLocation {
    uint32_t line = 1;
    uint32_t column = 1;
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string_view lexeme;
    SourceLocation loc;
};

} // namespace cvc
