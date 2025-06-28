cmake_minimum_required(VERSION 3.0.2)
project(ekf_fast_lio2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")

## Dependencies
find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  nav_msgs
  geometry_msgs
  sensor_msgs
  tf
)

catkin_package(
  INCLUDE_DIRS include
  CATKIN_DEPENDS roscpp std_msgs nav_msgs geometry_msgs sensor_msgs tf
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

## Node executable
add_executable(EKFAdaptiveFilter
  src/EKFAdaptiveFilter.cpp
)

target_link_libraries(EKFAdaptiveFilter
  ${catkin_LIBRARIES}
)