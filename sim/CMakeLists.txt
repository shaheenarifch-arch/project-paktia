cmake_minimum_required(VERSION 3.16)
project(havoc_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(havoc_sim STATIC
  src/Clock.cpp
  src/World.cpp
  src/StateHash.cpp
  src/Replay.cpp)
target_include_directories(havoc_sim PUBLIC include)

add_executable(havoc_sim_tests tests/main.cpp)
target_link_libraries(havoc_sim_tests PRIVATE havoc_sim)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(t havoc_sim havoc_sim_tests)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Werror)
  endforeach()
endif()

enable_testing()
add_test(NAME havoc_sim_tests COMMAND havoc_sim_tests)
