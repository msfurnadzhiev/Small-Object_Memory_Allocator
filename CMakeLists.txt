cmake_minimum_required(VERSION 3.17)
project(MemoryAllocator)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(lib/googletest-master)
include_directories(lib/googletest-master/googletest/include)

add_executable(MemoryAllocator main.cpp MemoryAllocator.cpp UnitTests/SmallObjectAllocatorTest.cpp)
target_link_libraries(MemoryAllocator gtest gtest_main)