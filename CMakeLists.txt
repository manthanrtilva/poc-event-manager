cmake_minimum_required(VERSION 3.11)
project(poc-event-manager)
set(CMAKE_CXX_STANDARD 20)

find_package(fmt CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)

add_executable(poc-event-manager main.cpp)
target_link_libraries(poc-event-manager fmt::fmt prometheus-cpp::pull)
# target_include_directories(poc-event-manager)