# Collect all source files in the lib directory
file(GLOB LIB_SOURCES "*.cpp")

# Create the library (Static library)
add_library(redis_lib STATIC ${LIB_SOURCES})

# Helper to allow other targets to see headers in 'lib' and 'msg'
# This allows includes like #include "TcpServer.h" (if we added it to path)
# But since your code uses "../lib/TcpServer.h", this acts mainly as dependency linkage
target_include_directories(redis_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../msg
)