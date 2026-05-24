cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(storage_unit_test storage/unit.cpp)
target_link_libraries(storage_unit_test PRIVATE dbms_core gtest_main)

add_executable(binary_io_test storage/binary_io.cpp)
target_link_libraries(binary_io_test PRIVATE dbms_core gtest_main)

add_executable(serializer_test storage/serializer.cpp)
target_link_libraries(serializer_test PRIVATE dbms_core gtest_main)

add_executable(pager_test storage/pager.cpp)
target_link_libraries(pager_test PRIVATE dbms_core gtest_main)

add_executable(b_star_plus_tree_test core/b_star_plus_tree.cpp)
target_link_libraries(b_star_plus_tree_test PRIVATE dbms_b_star_plus_tree gtest_main)

add_executable(access_logger_test server/unit.cpp ../src/server/access_logger.cpp ../src/server/telemetry.cpp)
target_include_directories(access_logger_test PRIVATE ../include)
target_link_libraries(access_logger_test PRIVATE Crow::Crow nlohmann_json::nlohmann_json gtest_main)

add_executable(query_tests query/tests.cpp)
target_link_libraries(query_tests PRIVATE dbms_core gtest gtest_main)
target_include_directories(query_tests PRIVATE ${gtest_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(storage_unit_test)
gtest_discover_tests(binary_io_test)
gtest_discover_tests(serializer_test)
gtest_discover_tests(pager_test)
gtest_discover_tests(b_star_plus_tree_test)
gtest_discover_tests(access_logger_test)
gtest_discover_tests(query_tests)
