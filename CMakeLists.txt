cmake_minimum_required( VERSION 3.20 )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wformat -Wformat-security -pedantic -pedantic-errors -Werror)
add_compile_options(-fPIC -pie -fstack-protector)
add_compile_options(-pthread)
add_compile_options(-fstack-protector-strong)
set(CMAKE_CXX_FLAGS_RELEASE -O2)
set(CMAKE_CXX_FLAGS_DEBUG -g)

project( vulkan_xcb LANGUAGES CXX )

add_subdirectory( src )
