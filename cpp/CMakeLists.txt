cmake_minimum_required(VERSION 3.28)
project(limit_order_book LANGUAGES CXX)

enable_testing()

add_library(lob
  src/order_book.cpp
)

add_executable(types_test
  tests/types_test.cpp
)

add_test(
  NAME types_test
  COMMAND types_test
)

target_include_directories(lob PUBLIC include)
target_compile_features(lob PUBLIC cxx_std_20)
target_link_libraries(types_test PRIVATE lob)
