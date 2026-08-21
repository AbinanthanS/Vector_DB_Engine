cmake_minimum_required(VERSION 3.20)
project(VectorDBCpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AVX2 SIMD flags based on compiler
if(MSVC)
    add_compile_options(/arch:AVX2 /O2)
else()
    add_compile_options(-mavx2 -mfma -O3 -Wall -Wextra)
endif()

# ------------------------------------------------------------
# Core engine library (shared by the benchmark binary and tests)
# ------------------------------------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(vectordb_core STATIC ${SOURCES})
target_include_directories(vectordb_core PUBLIC include)

# ------------------------------------------------------------
# Benchmark executable
# ------------------------------------------------------------
add_executable(vectordb main.cpp)
target_link_libraries(vectordb PRIVATE vectordb_core)

# On MinGW, static-link the runtime so vectordb.exe doesn't depend on
# libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll being on PATH.
if(MINGW)
    target_link_options(vectordb PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ------------------------------------------------------------
# Unit tests (GoogleTest, fetched at configure time)
# ------------------------------------------------------------
option(BUILD_TESTS "Build the GoogleTest unit test suite" ON)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Keep gtest's runtime settings consistent with this project on MSVC
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    file(GLOB TEST_SOURCES "tests/*.cpp")
    add_executable(vectordb_tests ${TEST_SOURCES})
    target_link_libraries(vectordb_tests PRIVATE vectordb_core gtest_main)

    if(MINGW)
        target_link_options(vectordb_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    include(GoogleTest)
    gtest_discover_tests(vectordb_tests)
endif()