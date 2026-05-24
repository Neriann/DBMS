cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(DBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.0 REQUIRED)
find_package(Boost REQUIRED COMPONENTS container date_time)

set(DBMS_TREE_BSP 4)
set(INDEX_TREE_LIB dbms_b_star_plus_tree)

add_subdirectory(libs/json)

set(CROW_USE_BOOST ON)
add_subdirectory(libs/crow)

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
        src/storage/pager.cpp
        src/storage/serializer.cpp
        src/storage/storage_manager.cpp
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
        ${INDEX_TREE_LIB}
        Boost::container
        nlohmann_json::nlohmann_json)

set(CORE_COMPILE_DEFS
        DBMS_TREE_BSP=${DBMS_TREE_BSP}
        DBMS_INDEX_TREE=${DBMS_TREE_BSP})

add_library(dbms_core ${CORE_SOURCES})
target_include_directories(dbms_core
        PUBLIC
        ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms_core
        PUBLIC
        ${CORE_LIBS})
target_compile_definitions(dbms_core PUBLIC ${CORE_COMPILE_DEFS})

# CLI
add_executable(dbms src/main.cpp)
target_link_libraries(dbms PRIVATE dbms_core)

# HTTP server
add_executable(dbms_server
        src/server.cpp
        src/server/access_logger.cpp
        src/server/telemetry.cpp)
target_link_libraries(dbms_server PRIVATE dbms_core Crow::Crow)

# HTTP client
add_executable(dbms_client src/client.cpp)

# Tests
enable_testing()
add_subdirectory(tests)
