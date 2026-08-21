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

include_directories(include)

# Gather source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(vectordb main.cpp ${SOURCES})

# On MinGW, static-link the runtime so vectordb.exe doesn't depend on
# libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll being on PATH.
if(MINGW)
    target_link_options(vectordb PRIVATE -static -static-libgcc -static-libstdc++)
endif()