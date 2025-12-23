# Collect source files for the server
file(GLOB SERVER_SOURCES "*.cpp")

# Create the server executable
add_executable(redis_server ${SERVER_SOURCES})

# Link the server against our library
target_link_libraries(redis_server PRIVATE redis_lib)

# Ensure the server can find headers if needed (though relative paths are currently used)
target_include_directories(redis_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
