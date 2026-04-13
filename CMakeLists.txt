cmake_minimum_required(VERSION 4.1.2)
project(toylanguage)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM 22 REQUIRED CONFIG)

# 2. Include the LLVM headers
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})

# 3. Define which LLVM components you need (Core is usually enough to start)
llvm_map_components_to_libnames(llvm_libs core support native target passes)

message(STATUS "LLVM found in: ${LLVM_DIR}")
message(STATUS "LLVM includes: ${LLVM_INCLUDE_DIRS}")

add_executable(toylanguage main.cpp
        Parsing/Tokenizer.cpp
        Parsing/Tokenizer.h
        Parsing/Parser.cpp
        Parsing/Parser.h
        Compilation/LLVMEmitter.cpp
        Compilation/LLVMEmitter.h
        Compilation/SSABlock.cpp
        Compilation/SSABlock.h)

target_include_directories(toylanguage SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})

# 5. Link libraries
target_link_libraries(toylanguage PRIVATE ${llvm_libs})

# 6. Disable RTTI if LLVM doesn't use it (prevents common linker crashes)
if(NOT LLVM_ENABLE_RTTI)
    if(MSVC)
        target_compile_options(toylanguage PUBLIC "/GR-")
    else()
        target_compile_options(toylanguage PUBLIC "-fno-rtti")
    endif()
endif()
