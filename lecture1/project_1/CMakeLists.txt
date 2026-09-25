cmake_minimum_required(VERSION 3.16.3)

project(fire_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
message(STATUS "--------------------CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}--------------------")

#===== 在此补充 =====
# find_package([包名] REQUIRED) 表示寻找程序包
# include_directories([路径]) 表示添加头文件


#====================

add_subdirectory(tasks/auto_aim)

add_executable(fire_test src/fire_test.cpp)
target_link_libraries(fire_test fmt::fmt yaml-cpp auto_aim)
