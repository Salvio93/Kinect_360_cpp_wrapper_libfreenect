cmake_minimum_required(VERSION 3.10)
project(KinectService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREENECT REQUIRED libfreenect)

include_directories(${FREENECT_INCLUDE_DIRS})
link_directories(${FREENECT_LIBRARY_DIRS})

add_executable(kinect_display kinect_display.cpp)
target_link_libraries(kinect_display ${FREENECT_LIBRARIES} pthread)

install(TARGETS kinect_display DESTINATION /app)
