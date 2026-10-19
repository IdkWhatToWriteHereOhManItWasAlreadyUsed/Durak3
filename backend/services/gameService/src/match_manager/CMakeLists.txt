project(match_manager)

add_subdirectory(match)

add_library(${PROJECT_NAME} match_manager.cpp)

target_link_libraries(${PROJECT_NAME} durak_match)