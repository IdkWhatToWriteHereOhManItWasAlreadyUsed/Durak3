project(match_manager)

add_subdirectory(match)

add_library(${PROJECT_NAME} match_manager.cpp i_match_manager.hpp match_manager.hpp)

#target_include_directories(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} durak_match)