cmake_minimum_required(VERSION 3.20)
project(hypernova-chess VERSION 1.0.0 LANGUAGES CXX)

# C++20 for best optimization + modern features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect architecture for optimal flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    # AVX-512, AVX2, BMI2, POPCNT instruction sets
    set(ARCH_FLAGS "-march=native -mtune=native")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(ARCH_FLAGS "-march=armv8.2-a+dotprod+fp16")
endif()

# Aggressive optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${ARCH_FLAGS} -O3 -flto=auto -fipa-cp-clone -fdevirtualize-at-ltrans -funroll-loops -ftree-vectorize -fomit-frame-pointer -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ARCH_FLAGS} -O2 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG")

# Link-time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Profiling support (optional)
option(USE_PERF "Use perf profiling" OFF)
if(USE_PERF)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer -g")
endif()

# Aggressive warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wstrict-aliasing -Wcast-align -Wno-unused-parameter -Wno-parentheses -Wno-sign-conversion -Wno-conversion")

# PGO support (Profile Guided Optimization)
option(USE_PGO "Use Profile Guided Optimization" OFF)
if(USE_PGO)
    set(PGO_DIR "${CMAKE_BINARY_DIR}/pgo")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate=${PGO_DIR}")
endif()

# Memory sanitizer for debug builds
option(USE_ASAN "Use AddressSanitizer" ON)
if(USE_ASAN AND CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined,leak")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined,leak")
endif()

# Find required packages
find_package(Threads REQUIRED)

# If we can find OpenBLAS or Eigen, use them for optional optimizations
# But the engine works fully without them (pure SIMD)
find_package(OpenBLAS QUIET)
find_package(Eigen3 QUIET)

# Sources - each file with 1000+ lines
set(SOURCES
    src/bitboard.cpp
    src/board.cpp
    src/movegen.cpp
    src/main.cpp
)

# Headers
set(HEADERS
    src/types.h
    src/bitboard.h
    src/board.h
    src/movegen.h
)

# Executable
add_executable(hypernova ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(hypernova PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(OpenBLAS_FOUND)
    target_link_libraries(hypernova PRIVATE OpenBLAS::OpenBLAS)
    target_compile_definitions(hypernova PRIVATE USE_OPENBLAS)
endif()
if(Eigen3_FOUND)
    target_compile_definitions(hypernova PRIVATE USE_EIGEN)
endif()

# Set compile definitions for optimization
target_compile_definitions(hypernova PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    HYPERNOVA_VERSION="${PROJECT_VERSION}"
    DEFAULT_THREADS=2
)

# Include directories
target_include_directories(hypernova PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Install target
install(TARGETS hypernova DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/hypernova.ini DESTINATION etc/hypernova)

# Add test targets
enable_testing()
add_test(NAME perft_test COMMAND hypernova --perft 6)
add_test(NAME selfplay_test COMMAND hypernova --selfplay 16 10)

message(STATUS "===== HyperNova NNUE Chess Engine =====")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture flags: ${ARCH_FLAGS}")
message(STATUS "Threads: available ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "OpenBLAS: ${OpenBLAS_FOUND}")
message(STATUS "Eigen3: ${Eigen3_FOUND}")
message(STATUS "========================================")