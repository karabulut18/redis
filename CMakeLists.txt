cmake_minimum_required(VERSION 3.10)

project(RedisCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clangd/VSCode intellisense

# Global compiler options
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Include subdirectories
add_subdirectory(lib)
add_subdirectory(server)
add_subdirectory(client)