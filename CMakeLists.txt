cmake_minimum_required(VERSION 3.20)
project(Yggdrasil LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Currently, P2996 requires specific compiler flags on the custom forks.
# For example, on Bloomberg's clang fork or GCC 16: -freflection
add_compile_options(-freflection)


include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_library(yggdrasil INTERFACE)
target_include_directories(yggdrasil INTERFACE include)

enable_testing()
add_executable(test_order test/test_order.cpp)
target_link_libraries(test_order PRIVATE yggdrasil gtest_main)
include(GoogleTest)
gtest_discover_tests(test_order)
