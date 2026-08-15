set(ZLIB_INCLUDE_DIR "C:/msys64/mingw64/include")
set(ZLIB_LIBRARY "C:/msys64/mingw64/lib")
find_package(ZLIB REQUIRED)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(gen-tool)
target_sources(gen-tool
PRIVATE
        tool.hpp
        tool.cpp)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

message(${llvm_libs})

# Link against LLVM libraries

# ${llvm_libs} clangAST clangFrontend

SET(CMAKE_CXX_FLAGS "-fuse-ld=lld -march=native")

target_link_libraries(gen-tool clangTooling pthread)
#        clangFrontendTool clangDriver clangSerialization clangCodeGen clangParse clangSema clangStaticAnalyzerFrontend clangStaticAnalyzerCheckers clangStaticAnalyzerCore clangAnalysis clangRewrite clangRewriteFrontend clangEdit clangAST)