cmake_minimum_required(VERSION 3.16)
project(bossrally_port C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_library(brcore STATIC src/br_pod.c)
target_include_directories(brcore PUBLIC include)

enable_testing()
add_executable(test_pod tests/test_pod.c)
target_link_libraries(test_pod PRIVATE brcore)
add_test(NAME pod COMMAND test_pod ${CMAKE_SOURCE_DIR}/../testdata/BossRally.pod)
