# PR-000: minimal repo skeleton + smoke test (lean layout)
cmake_minimum_required(VERSION 3.16)
project(PostcardDB C)

# Use modern, portable C (C11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Keep quality high (treat warnings as errors).
add_compile_options(-Wall -Wextra -Werror -pedantic -Wno-unused-parameter)

# Public headers live in include/
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Core library (for PR-000 we just provide a stub CRC32C)
add_library(pc STATIC
  src/pc_crc32c.c
  src/pc_log.c
  src/pc_ring.c 
  src/pc_parity.c
  src/pc_flash_sim.c
  src/pc_logseg.c
  src/pc_recover.c 
  src/pc_block_writer.c 
  src/pc_appender.c
  src/pc_alloc.c  
  src/pc_api.c    
)
target_include_directories(pc PUBLIC include)

# ----------------- Tests -----------------
enable_testing()

add_executable(test_smoke
  tests/test_smoke.c
)
target_link_libraries(test_smoke pc)
add_test(NAME smoke COMMAND test_smoke)

add_executable(test_ring tests/test_ring.c)   
target_link_libraries(test_ring pc)
add_test(NAME ring COMMAND test_ring)

find_package(Threads REQUIRED)

add_executable(test_ring_threads tests/test_ring_threads.c)
target_link_libraries(test_ring_threads pc Threads::Threads)
add_test(NAME ring_threads COMMAND test_ring_threads)

# new CRC32C/parity test
add_executable(test_crc32c tests/test_crc32c.c)
target_link_libraries(test_crc32c pc)
add_test(NAME crc32c COMMAND test_crc32c)

add_executable(test_flash_sim tests/test_flash_sim.c)
target_link_libraries(test_flash_sim pc)
add_test(NAME flash_sim COMMAND test_flash_sim)

# Tests add
add_executable(test_logseg tests/test_logseg.c)
target_link_libraries(test_logseg pc)
add_test(NAME logseg COMMAND test_logseg)


add_executable(test_recover tests/test_recover.c)
target_link_libraries(test_recover pc)
add_test(NAME recover COMMAND test_recover)

add_executable(test_block tests/test_block.c)
target_link_libraries(test_block pc)
add_test(NAME block COMMAND test_block)

# New test
add_executable(test_appender tests/test_appender.c)
target_link_libraries(test_appender pc)
add_test(NAME appender COMMAND test_appender)

# New API test
add_executable(test_api tests/test_api.c)
target_link_libraries(test_api pc)
add_test(NAME api COMMAND test_api)

# New test
add_executable(test_alloc tests/test_alloc.c)
target_link_libraries(test_alloc pc)
add_test(NAME alloc COMMAND test_alloc)