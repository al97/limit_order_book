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

add_executable(order_book_test
  tests/order_book_test.cpp
)

add_test(
  NAME order_book_test
  COMMAND order_book_test
)

target_include_directories(lob PUBLIC include)
target_compile_features(lob PUBLIC cxx_std_20)
target_link_libraries(types_test PRIVATE lob)
target_link_libraries(order_book_test PRIVATE lob)
