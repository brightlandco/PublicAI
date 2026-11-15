cmake_minimum_required(VERSION 3.20)
project(cvc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cvc
    compiler/src/main.cpp
    compiler/src/Lexer.cpp
    compiler/src/Parser.cpp
    compiler/src/EffectAnalyzer.cpp
    compiler/src/VerseEmitter.cpp
    compiler/src/SourceMap.cpp
)

target_include_directories(cvc PRIVATE compiler/include)

if(MSVC)
    target_compile_options(cvc PRIVATE /W4 /permissive-)
else()
    target_compile_options(cvc PRIVATE -Wall -Wextra -Wpedantic)
endif()
