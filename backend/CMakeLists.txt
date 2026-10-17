cmake_minimum_required(VERSION 3.12...3.31)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(authService)
add_subdirectory(gameService)
add_subdirectory(gateway)
add_subdirectory(lobbyService)
add_subdirectory(matchmakingService)

