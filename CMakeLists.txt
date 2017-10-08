cmake_minimum_required(VERSION 3.5)
project(PHPInterp)

set(CMAKE_C_COMPILER  "/usr/bin/clang")

set(CMAKE_C_STANDARD 11)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer")

set(SOURCE_FILES main.c lex.c lex.h run.h run.c parse.c parse.h scope.c scope.h compile.c compile.h array-util.h enum-util.h op_util.h macros.h util.h util.c)
add_executable(PHPInterp ${SOURCE_FILES})