cmake_minimum_required(VERSION 3.12...3.31)

project(Durak3Backend)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_subdirectory(gateways/apiGateway)
add_subdirectory(gateways/realtimeGateway)

add_subdirectory(services/authService)
add_subdirectory(services/gameService)
add_subdirectory(services/lobbyService)
add_subdirectory(services/replayService)
add_subdirectory(services/matchmakingService)
