cmake_minimum_required(VERSION 3.28)
project(limit_order_book LANGUAGES CXX)

add_library(lob
  src/order_book.cpp
)

target_include_directories(lob PUBLIC include)
target_compile_features(lob PUBLIC cxx_std_20)