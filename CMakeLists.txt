cmake_minimum_required( VERSION 3.12 )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wformat -Wformat-security -pedantic -pedantic-errors -Werror -g)
add_compile_options(-fPIC -pie -fstack-protector)
add_compile_options(-pthread)
project( vulkan_xcb LANGUAGES CXX )

add_subdirectory( src )
