project(durak_state)

add_library(${PROJECT_NAME} STATIC
    game_session.cpp
    replay.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
