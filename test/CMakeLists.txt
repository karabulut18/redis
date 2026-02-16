cmake_minimum_required(VERSION 3.10)

# Test executables
add_executable(test_ringbuffer test_ringbuffer.cpp)
target_link_libraries(test_ringbuffer redis_lib)

add_executable(test_hashmap test_hashmap.cpp)
target_link_libraries(test_hashmap redis_lib)

add_executable(test_avltree test_avltree.cpp)
target_link_libraries(test_avltree redis_lib)

add_executable(test_database test_database.cpp)
target_link_libraries(test_database redis_lib)

add_executable(test_resp_parser test_resp_parser.cpp)
target_link_libraries(test_resp_parser redis_lib)
