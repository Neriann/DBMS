cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(DBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.0 REQUIRED)
find_package(Boost REQUIRED COMPONENTS container date_time)
find_package(OpenSSL REQUIRED)

include(FetchContent)

FetchContent_Declare(
        jwt-cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG v0.7.0
)

set(CROW_USE_BOOST ON)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow
        GIT_TAG v1.3.2
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(jwt-cpp Crow)

add_library(dbms_b_star_plus_tree INTERFACE)
target_include_directories(dbms_b_star_plus_tree
        INTERFACE
        include
        include/core/trees)
target_link_libraries(dbms_b_star_plus_tree
        INTERFACE
        Boost::container)

# Bison & Flex
FLEX_TARGET(Lexer src/query/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
BISON_TARGET(Parser src/query/parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

set(CORE_SOURCES
        src/core/value.cpp
        src/core/schema.cpp
        src/core/table.cpp
        src/core/database.cpp
        src/core/dbms.cpp
        src/query/ast.cpp
        src/query/executor.cpp
        src/query/query_runner.cpp
        src/storage/binary_io.cpp
        src/storage/page_io.cpp
        src/storage/pager.cpp
        src/storage/serializer.cpp
        src/storage/storage_manager.cpp
        src/core/stringpool.cpp
        src/async/task_manager.cpp
        src/auth/auth_service.cpp
        src/auth/jwt_service.cpp
        src/auth/password_hasher.cpp
        src/rbac/access_manager.cpp
        src/rbac/permission_resolver.cpp
        ${FLEX_Lexer_OUTPUTS}
        ${BISON_Parser_OUTPUTS}
)

set(CORE_INCLUDE_DIRS
        include
        ${CMAKE_CURRENT_BINARY_DIR})

if (FLEX_INCLUDE_DIRS)
    list(APPEND CORE_INCLUDE_DIRS ${FLEX_INCLUDE_DIRS})
endif ()

set(CORE_LIBS
        dbms_b_star_plus_tree
        Boost::container
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        OpenSSL::Crypto)

add_library(dbms_core ${CORE_SOURCES})
target_include_directories(dbms_core
        PUBLIC
        ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms_core
        PUBLIC
        ${CORE_LIBS})

# CLI
add_executable(dbms src/main.cpp)
target_link_libraries(dbms PRIVATE dbms_core)

# HTTP server
add_executable(dbms_server
        src/server.cpp
        src/server/middleware/access_logger.cpp
        src/server/middleware/telemetry.cpp
        src/server/routes/auth_routes.cpp
        src/server/middleware/auth_middleware.cpp)
target_link_libraries(dbms_server PRIVATE dbms_core Crow::Crow)


add_executable(entrypointer
    src/entrypoint_main.cpp
    src/server/entrypoint.cpp
    src/server/heartbeat.cpp
)
target_link_libraries(entrypointer PRIVATE dbms_core Crow::Crow)

# HTTP client
add_executable(dbms_client src/client.cpp)

# Tests
enable_testing()
add_subdirectory(tests)
