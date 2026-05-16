cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(storage_unit_test storage/unit.cpp)
target_link_libraries(storage_unit_test PRIVATE dbms_core gtest_main)

include(GoogleTest)
gtest_discover_tests(storage_unit_test)
