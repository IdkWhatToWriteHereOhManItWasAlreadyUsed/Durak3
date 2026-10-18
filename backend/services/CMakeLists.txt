cmake_minimum_required(VERSION 3.12...3.31)

project(Durak3BackendServices)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_subdirectory(authService)
add_subdirectory(gameService)
add_subdirectory(lobbyService)
add_subdirectory(replayService)
add_subdirectory(matchmakingService)
