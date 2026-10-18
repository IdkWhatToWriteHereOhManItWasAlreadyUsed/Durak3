cmake_minimum_required(VERSION 3.15)
project(apiGateway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -DDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra -DDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
endif()

file(GLOB_RECURSE SOURCE_FILES
        "src/*.cpp"
        "src/*.cc"
)

find_package(Drogon REQUIRED)
find_package(Jsoncpp REQUIRED)

add_executable(${PROJECT_NAME}
        ${SOURCE_FILES}
        src/middleware/AuthCheckMiddleware.hpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${JSONCPP_INCLUDE_DIRS}"
        "${DROGON_INCLUDE_DIRS}"
)

target_link_libraries(${PROJECT_NAME} Drogon::Drogon)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
        COMMENT "Copying config directory to output directory"
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Executable directory: $<TARGET_FILE_DIR:${PROJECT_NAME}>")