cmake_minimum_required(VERSION 3.12...3.31)
project(gameService CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Googletest для юнит-тестов скачивается CPM, если в системе не найден.
set(USERVER_DOWNLOAD_PACKAGE_GTEST ON CACHE BOOL "" FORCE)
set(USERVER_DOWNLOAD_PACKAGE_GOOGLETEST ON CACHE BOOL "" FORCE)
include(DownloadUserver)


find_package(
    userver
    COMPONENTS
        core #
        chaotic
        chaotic-openapi
        redis
    #QUIET
)
if(NOT userver_FOUND)
    # Tries TRY_DIR first, falls back to downloading userver from GitHub using CPM.
    download_userver(TRY_DIR third_party/userver)
endif()

userver_setup_environment()

# The Service
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/components/match_manager_component.hpp
        src/components/match_manager_component.cpp
        src/components/redis_client_component.cpp
        src/components/redis_client_component.hpp
)



add_subdirectory(src/match_manager)

userver_target_generate_openapi_handlers(
        ${PROJECT_NAME}_handlers
        NAME ${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/handlers"
        SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src"
        SCHEMAS "${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml"
)

target_link_libraries(${PROJECT_NAME}_handlers userver::redis)

target_link_libraries(${PROJECT_NAME} PRIVATE
        userver::core
        userver::redis
        match_manager
        ${PROJECT_NAME}_handlers
)

userver_generate_config_yaml(
        ${PROJECT_NAME}
        BASE_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config.yaml"
)

# Engine unit tests
add_executable(${PROJECT_NAME}_engine_test src/match_manager/match/state/game_session_test.cpp)
target_link_libraries(${PROJECT_NAME}_engine_test PRIVATE durak_state userver::utest)

# Match unit tests (Match + MatchManager)
add_executable(${PROJECT_NAME}_games_test src/match_manager/match_test.cpp)
target_link_libraries(${PROJECT_NAME}_games_test PRIVATE match_manager userver::utest)

include(CTest)
add_test(NAME ${PROJECT_NAME}_engine_test COMMAND ${PROJECT_NAME}_engine_test)
add_test(NAME ${PROJECT_NAME}_games_test COMMAND ${PROJECT_NAME}_games_test)

# Unit Tests
#add_executable(${PROJECT_NAME}_unittest src/greeting_test.cpp)
#target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
#add_google_tests(${PROJECT_NAME}_unittest)

# Benchmarks
#add_executable(${PROJECT_NAME}_benchmark src/greeting_benchmark.cpp)
#target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
#add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional testing
userver_testsuite_add_simple()

# Install
include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
    message(STATUS "Set install prefix: $ENV{PREFIX}")
    file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
    set(CMAKE_INSTALL_PREFIX "${PREFIX_PATH}")
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${PROJECT_NAME}
)
install(
    FILES ${CONFIGS_FILES}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)