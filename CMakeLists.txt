cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(DBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(Boost REQUIRED COMPONENTS container date_time)

add_subdirectory(libs/json)

set(CROW_USE_BOOST ON)
add_subdirectory(libs/crow)

add_subdirectory(libs/sysprog/common)
add_subdirectory(libs/sysprog/allocator/allocator)
add_subdirectory(libs/sysprog/associative_container)

# Bison & Flex
FLEX_TARGET(Lexer src/query/lexer.ll ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
BISON_TARGET(Parser src/query/parser.yy ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

set(CORE_SOURCES
        src/core/Value.cpp
        src/core/Schema.cpp
        src/core/Table.cpp
        src/core/Database.cpp
        src/core/DBMS.cpp
        src/query/AST.cpp
        src/query/Executor.cpp
        src/storage/StorageManager.cpp
        ${FLEX_Lexer_OUTPUTS}
        ${BISON_Parser_OUTPUTS}
)

set(CORE_INCLUDE_DIRS
        include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${FLEX_INCLUDE_DIRS})

set(CORE_LIBS
        sys_prog_assctv_cntnr_indxng_tr_b_str_pls_tr
        sys_prog_cmmn
        Boost::container
        nlohmann_json::nlohmann_json)

# CLI
add_executable(dbms src/main.cpp ${CORE_SOURCES})
target_include_directories(dbms PRIVATE ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms PRIVATE ${CORE_LIBS})

# HTTP server
add_executable(dbms_server src/server.cpp ${CORE_SOURCES})
target_include_directories(dbms_server PRIVATE ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms_server PRIVATE ${CORE_LIBS} Crow::Crow)

# HTTP client
add_executable(dbms_client src/client.cpp)
target_include_directories(dbms_client PRIVATE include)
target_link_libraries(dbms_client PRIVATE Boost::container)
