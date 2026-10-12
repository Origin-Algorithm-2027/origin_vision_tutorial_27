cmake_minimum_required(VERSION 3.16.3)

project(fire_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
message(STATUS "--------------------CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}--------------------")

find_package(fmt REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(yaml-cpp REQUIRED)

include_directories(${EIGEN3_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(tasks/auto_aim)

add_executable(fire_test src/fire_test.cpp)
target_link_libraries(fire_test fmt::fmt yaml-cpp auto_aim)   
