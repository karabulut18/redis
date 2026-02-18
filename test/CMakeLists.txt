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


add_executable(test_zrank test_zrank.cpp)
target_link_libraries(test_zrank redis_lib)

add_executable(test_network test_network.cpp)
target_link_libraries(test_network redis_lib)

add_executable(test_segmented_buffer test_segmented_buffer.cpp)
target_link_libraries(test_segmented_buffer redis_lib)

add_executable(test_persistence test_persistence.cpp)
target_link_libraries(test_persistence redis_lib)
