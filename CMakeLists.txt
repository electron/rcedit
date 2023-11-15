cmake_minimum_required(VERSION 3.15)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# /Ox, full optimization
# /Os, favour small code
add_compile_options(/Ox /Os)

project(rcedit)

add_executable(rcedit src/main.cc src/rescle.cc src/rcedit.rc)
target_link_libraries(rcedit version.lib)
