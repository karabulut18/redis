cmake_minimum_required(VERSION 3.10)
project(RedisLib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files in the lib directory
file(GLOB LIB_SOURCES 
    "common/*.cpp"
    "redis/*.cpp"
)

# Create the library (Static library)
add_library(redis_lib STATIC ${LIB_SOURCES})

# Helper to allow other targets to see headers in 'lib' and 'msg'
target_include_directories(redis_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/redis
    ${CMAKE_CURRENT_SOURCE_DIR}/../msg
)