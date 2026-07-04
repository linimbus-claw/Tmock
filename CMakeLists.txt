cmake_minimum_required(VERSION 3.10)
project(Tmock VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 头文件路径 ----
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- 测试_targets ----
add_executable(test_basic  tests/test_basic.cpp)
add_executable(test_async  tests/test_async.cpp)
add_executable(test_sequence tests/test_sequence.cpp)

enable_testing()
add_test(NAME BasicTest    COMMAND test_basic)
add_test(NAME AsyncTest    COMMAND test_async)
add_test(NAME SequenceTest COMMAND test_sequence)

# ---- 示例（可选）----
# add_executable(example_usage examples/example_usage.cpp)
# target_link_libraries(example_usage pthread)
