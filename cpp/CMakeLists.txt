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

add_executable(reference_book_test
  tests/reference_book.cpp
  tests/reference_book_test.cpp
)

add_test(
  NAME reference_book_test
  COMMAND reference_book_test
)

target_include_directories(lob PUBLIC include)
target_compile_features(lob PUBLIC cxx_std_20)
target_link_libraries(types_test PRIVATE lob)
target_link_libraries(order_book_test PRIVATE lob)
target_include_directories(reference_book_test PRIVATE include)
target_compile_features(reference_book_test PRIVATE cxx_std_20)

add_executable(agree_test
  tests/reference_book.cpp
  tests/snapshot.cpp
  tests/agree_test.cpp
)

add_test(
  NAME agree_test
  COMMAND agree_test
)

target_include_directories(agree_test PRIVATE include tests)
target_compile_features(agree_test PRIVATE cxx_std_20)
target_link_libraries(agree_test PRIVATE lob)

add_executable(differential_test
  tests/reference_book.cpp
  tests/snapshot.cpp
  tests/differential_test.cpp
)

add_test(
  NAME differential_test
  COMMAND differential_test
)

target_include_directories(differential_test PRIVATE include tests)
target_compile_features(differential_test PRIVATE cxx_std_20)
target_link_libraries(differential_test PRIVATE lob)
