cmake_minimum_required(VERSION 3.12...3.31)
project(matchmakingService CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)
set(USERVER_FEATURE_REDIS ON CACHE BOOL "" FORCE)
set(USERVER_FEATURE_PATCH_LIBPQ OFF CACHE BOOL "" FORCE)
include(DownloadUserver)

find_package(
    userver
    COMPONENTS
        core #
        postgresql
        redis
    QUIET
)
if(NOT userver_FOUND)
    # Tries TRY_DIR first, falls back to downloading userver from GitHub using CPM.
    download_userver(TRY_DIR third_party/userver)
endif()

userver_setup_environment()

# Common sources
include_directories(src)

add_library(
        ${PROJECT_NAME}_objs OBJECT
        src/greeting.cpp #
        src/hello.cpp #
        src/hello_postgres.cpp
)

target_link_libraries(
        ${PROJECT_NAME}_objs
        PUBLIC
        userver::core #
        userver::postgresql
        userver::redis
)

userver_target_generate_openapi_handlers(
        ${PROJECT_NAME}_handlers
        NAME ${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/handlers"
        SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src"
        SCHEMAS "${CMAKE_CURRENT_SOURCE_DIR}/openapi.yaml"
)

#[[
userver_add_sql_library(
        ${PROJECT_NAME}_sql
        NAMESPACE ${PROJECT_NAME}
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
        DTO_DIALECT postgresql
        MIGRATIONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/postgresql/schemas
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries
        QUERIES_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/postgresql/queries
        DUMP_DIR       ${CMAKE_CURRENT_SOURCE_DIR}
        SQL_FILES *.sql
)
]]

target_link_libraries(${PROJECT_NAME}_handlers
        #${PROJECT_NAME}_sql
        userver::postgresql
        userver::redis
)

# The Service
add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${PROJECT_NAME}_objs
        ${PROJECT_NAME}_handlers
        #${PROJECT_NAME}_sql
)

userver_generate_config_yaml(
        ${PROJECT_NAME}
        BASE_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config.yaml"
)

# Unit Tests
#add_executable(${PROJECT_NAME}_unittest src/greeting_test.cpp)
#target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
#add_google_tests(${PROJECT_NAME}_unittest)

# Benchmarks
#add_executable(${PROJECT_NAME}_benchmark src/greeting_benchmark.cpp)
#target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
#add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional testing
#userver_testsuite_add_simple()

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