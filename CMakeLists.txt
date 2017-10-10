cmake_minimum_required(VERSION 3.1)
project(PHPInterp)

set(CMAKE_C_COMPILER  "/usr/bin/clang")
set(CMAKE_C_STANDARD 11)

if (NOT MSVC)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer")
endif()

file(GLOB SRC_C *.c crossplatform/*.c)
file(GLOB SRC_H *.h crossplatform/*.h) 

set(SOURCE_FILES ${SRC_C} ${SRC_H})
add_executable(PHPInterp ${SOURCE_FILES})
