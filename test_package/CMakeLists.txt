cmake_minimum_required(VERSION 3.14)
project(test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find cordoba package
find_package(cordoba REQUIRED)

# Simple test app
add_executable(test_app test_app.cpp)
target_link_libraries(test_app PRIVATE cordoba::gd)

# Copy gd/expected.h to include path for systems without C++23 <expected>
target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gd/inc)
