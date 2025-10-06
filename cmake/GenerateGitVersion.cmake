# cmake/GenerateGitVersion.cmake
execute_process(
    COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(GIT_HEADER_PATH "${CMAKE_BINARY_DIR}/GitVersion.h" CACHE INTERNAL "")

file(WRITE "${GIT_HEADER_PATH}" "#pragma once\n")
file(APPEND "${GIT_HEADER_PATH}" "#define GIT_COMMIT_HASH \"${GIT_HASH}\"\n")
