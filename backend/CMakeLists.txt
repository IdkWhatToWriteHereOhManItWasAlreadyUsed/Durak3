cmake_minimum_required(VERSION 3.12...3.31)

project(Durak3Backend)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_subdirectory(gateways)
add_subdirectory(services)
