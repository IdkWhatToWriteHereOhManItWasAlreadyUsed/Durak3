cmake_minimum_required(VERSION 3.12...3.31)
project(authService CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)
set(USERVER_FEATURE_PATCH_LIBPQ OFF CACHE BOOL "" FORCE)
include(DownloadUserver)

find_package(
        userver
        COMPONENTS core #
        postgresql
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
        src/handlers/auth/google_auth.cpp
        src/repositories/user_repository.cpp
        src/services/auth_service.cpp
        src/handlers/auth/logout.cpp
        src/handlers/users/users.cpp
        src/handlers/auth/me.cpp
)
target_link_libraries(
        ${PROJECT_NAME}_objs
        PUBLIC userver::core
        userver::postgresql
)

# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)

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