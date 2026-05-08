cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(DBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.0 REQUIRED)
find_package(Boost REQUIRED COMPONENTS container date_time)

set(DBMS_TREE_B 1)
set(DBMS_TREE_BP 2)
set(DBMS_TREE_BS 3)
set(DBMS_TREE_BSP 4)

set(DBMS_ALLOC_GLOBAL_HEAP 1)
set(DBMS_ALLOC_BOUNDARY_TAGS 2)
set(DBMS_ALLOC_SORTED_LIST 3)
set(DBMS_ALLOC_BUDDIES 4)
set(DBMS_ALLOC_RB_TREE 5)

set(DBMS_INDEX_TREE "BSP_tree" CACHE STRING "Index tree for the DBMS databases container")
set_property(CACHE DBMS_INDEX_TREE PROPERTY STRINGS "B_tree" "BP_tree" "BS_tree" "BSP_tree")
if (DBMS_INDEX_TREE STREQUAL "B_tree")
    set(INDEX_TREE_LIB sys_prog_assctv_cntnr_indxng_tr_b_tr)
    set(INDEX_TREE_NUM ${DBMS_TREE_B})
elseif (DBMS_INDEX_TREE STREQUAL "BP_tree")
    set(INDEX_TREE_LIB sys_prog_assctv_cntnr_indxng_tr_b_pls_tr)
    set(INDEX_TREE_NUM ${DBMS_TREE_BP})
elseif (DBMS_INDEX_TREE STREQUAL "BS_tree")
    set(INDEX_TREE_LIB sys_prog_assctv_cntnr_indxng_tr_b_str_tr)
    set(INDEX_TREE_NUM ${DBMS_TREE_BS})
elseif (DBMS_INDEX_TREE STREQUAL "BSP_tree")
    set(INDEX_TREE_LIB sys_prog_assctv_cntnr_indxng_tr_b_str_pls_tr)
    set(INDEX_TREE_NUM ${DBMS_TREE_BSP})
else ()
    message(FATAL_ERROR "DBMS_INDEX_TREE must be B_tree, BP_tree, BS_tree or BSP_tree, got '${DBMS_INDEX_TREE}'")
endif ()
message(STATUS "DBMS index tree: ${DBMS_INDEX_TREE}")

set(DBMS_ALLOCATOR "red_black_tree" CACHE STRING "Allocator for the DBMS index tree")
set_property(CACHE DBMS_ALLOCATOR PROPERTY STRINGS "global_heap" "boundary_tags" "sorted_list" "buddies_system" "red_black_tree")
if (DBMS_ALLOCATOR STREQUAL "global_heap")
    set(ALLOCATOR_LIB sys_prog_allctr_allctr_glbl_hp)
    set(ALLOCATOR_NUM ${DBMS_ALLOC_GLOBAL_HEAP})
elseif (DBMS_ALLOCATOR STREQUAL "boundary_tags")
    set(ALLOCATOR_LIB sys_prog_allctr_allctr_bndr_tgs)
    set(ALLOCATOR_NUM ${DBMS_ALLOC_BOUNDARY_TAGS})
elseif (DBMS_ALLOCATOR STREQUAL "sorted_list")
    set(ALLOCATOR_LIB sys_prog_allctr_allctr_srtd_lst)
    set(ALLOCATOR_NUM ${DBMS_ALLOC_SORTED_LIST})
elseif (DBMS_ALLOCATOR STREQUAL "buddies_system")
    set(ALLOCATOR_LIB sys_prog_allctr_allctr_bdds_sstm)
    set(ALLOCATOR_NUM ${DBMS_ALLOC_BUDDIES})
elseif (DBMS_ALLOCATOR STREQUAL "red_black_tree")
    set(ALLOCATOR_LIB sys_prog_allctr_allctr_rb_tr)
    set(ALLOCATOR_NUM ${DBMS_ALLOC_RB_TREE})
else ()
    message(FATAL_ERROR "DBMS_ALLOCATOR must be global_heap, boundary_tags, sorted_list, buddies_system or red_black_tree, got '${DBMS_ALLOCATOR}'")
endif ()
message(STATUS "DBMS allocator: ${DBMS_ALLOCATOR}")

add_subdirectory(libs/json)

set(CROW_USE_BOOST ON)
add_subdirectory(libs/crow)

add_subdirectory(libs/sysprog/common)
add_subdirectory(libs/sysprog/allocator)
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
        ${INDEX_TREE_LIB}
        ${ALLOCATOR_LIB}
        sys_prog_cmmn
        Boost::container
        nlohmann_json::nlohmann_json)

set(CORE_COMPILE_DEFS
        DBMS_TREE_B=${DBMS_TREE_B}
        DBMS_TREE_BP=${DBMS_TREE_BP}
        DBMS_TREE_BS=${DBMS_TREE_BS}
        DBMS_TREE_BSP=${DBMS_TREE_BSP}
        DBMS_ALLOC_GLOBAL_HEAP=${DBMS_ALLOC_GLOBAL_HEAP}
        DBMS_ALLOC_BOUNDARY_TAGS=${DBMS_ALLOC_BOUNDARY_TAGS}
        DBMS_ALLOC_SORTED_LIST=${DBMS_ALLOC_SORTED_LIST}
        DBMS_ALLOC_BUDDIES=${DBMS_ALLOC_BUDDIES}
        DBMS_ALLOC_RB_TREE=${DBMS_ALLOC_RB_TREE}
        DBMS_INDEX_TREE=${INDEX_TREE_NUM}
        DBMS_ALLOCATOR=${ALLOCATOR_NUM})

# CLI
add_executable(dbms src/main.cpp ${CORE_SOURCES})
target_include_directories(dbms PRIVATE ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms PRIVATE ${CORE_LIBS})
target_compile_definitions(dbms PRIVATE ${CORE_COMPILE_DEFS})

# HTTP server
add_executable(dbms_server src/server.cpp ${CORE_SOURCES})
target_include_directories(dbms_server PRIVATE ${CORE_INCLUDE_DIRS})
target_link_libraries(dbms_server PRIVATE ${CORE_LIBS} Crow::Crow)
target_compile_definitions(dbms_server PRIVATE ${CORE_COMPILE_DEFS})

# HTTP client
add_executable(dbms_client src/client.cpp)
target_include_directories(dbms_client PRIVATE include)
target_link_libraries(dbms_client PRIVATE Boost::container)
