project(durak_match)

add_subdirectory(state)

add_library(${PROJECT_NAME} STATIC
    match.cpp
)

target_link_libraries(${PROJECT_NAME} durak_state)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
