#pragma once

#include "Token.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cvc {

enum class AccessModifier : uint8_t {
    Default,
    Public,
    Private,
    Protected,
    Internal
};

enum EffectFlags : uint32_t {
    EFFECT_NONE      = 0,
    EFFECT_COMPUTES  = 1 << 0,
    EFFECT_READS     = 1 << 1,
    EFFECT_TRANSACTS = 1 << 2,
    EFFECT_DECIDES   = 1 << 3,
    EFFECT_SUSPENDS  = 1 << 4
};

struct TypeRef {
    std::string baseName;
    std::vector<TypeRef> typeArgs;
    bool isNullable = false;
    bool isArray = false;
};

struct ParameterDecl {
    TypeRef type;
    std::string name;
    SourceLocation loc;
};

struct Stmt {
    SourceLocation loc;
    virtual ~Stmt() = default;
};

struct RawVerseStmt : Stmt {
    std::string code;
};

struct VarDeclStmt : Stmt {
    AccessModifier access = AccessModifier::Default;
    bool isMutable = false;
    bool isEditable = false;
    TypeRef type;
    std::string name;
    std::string initializerExpr;
};

struct FunctionDecl {
    AccessModifier access = AccessModifier::Default;
    bool isFailable = false;
    bool isAsync = false;
    bool isConst = false;
    TypeRef returnType;
    std::string name;
    std::vector<ParameterDecl> params;
    std::vector<std::unique_ptr<Stmt>> body;
    uint32_t inferredEffects = EFFECT_NONE;
    SourceLocation loc;
};

struct TypeDecl {
    enum class Kind { Class, Struct, Interface } kind = Kind::Class;
    AccessModifier access = AccessModifier::Public;
    std::string name;
    std::vector<std::string> baseTypes;
    std::vector<VarDeclStmt> fields;
    std::vector<FunctionDecl> methods;
    SourceLocation loc;
};

struct CompilationUnit {
    std::string sourcePath;
    std::vector<std::string> namespaceParts;
    std::vector<TypeDecl> types;
    std::vector<VarDeclStmt> moduleVars;
    std::vector<FunctionDecl> moduleFunctions;
};

} // namespace cvc
