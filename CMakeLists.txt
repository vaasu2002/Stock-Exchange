cmake_minimum_required(VERSION 3.10)
project(ProcessDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include common directory
include_directories(${CMAKE_SOURCE_DIR}/common)

# Common IPC source files
set(IPC_SOURCES
    common/ipc/SharedMemory.cpp
    common/ipc/SharedMemory_Producer.cpp
    common/ipc/SharedMemory_Consumer.cpp
)

# Process 1 executable (Gateway)
add_executable(Gateway Gateway/main.cpp ${IPC_SOURCES})
target_include_directories(Gateway PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(Gateway PRIVATE ${CMAKE_SOURCE_DIR}/common/ipc)
target_include_directories(Gateway PRIVATE ${CMAKE_SOURCE_DIR}/common/Logger)
target_link_libraries(Gateway pthread)

# Process 2 executable
add_executable(process2 process2/main.cpp ${IPC_SOURCES})
target_include_directories(process2 PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(process2 PRIVATE ${CMAKE_SOURCE_DIR}/common/ipc)
target_include_directories(process2 PRIVATE ${CMAKE_SOURCE_DIR}/common/Logger)

# Test executable - IPC Queue Connection and Crash Recovery Test
add_executable(test_ipc_crash tests/test_ipc_crash.cpp ${IPC_SOURCES})
target_include_directories(test_ipc_crash PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(test_ipc_crash PRIVATE ${CMAKE_SOURCE_DIR}/common/ipc)

# Test executable - Gateway Network and FIX Protocol Test
add_executable(test_gateway tests/test_gateway.cpp)
target_include_directories(test_gateway PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_include_directories(test_gateway PRIVATE ${CMAKE_SOURCE_DIR}/common/Logger)

# Enable testing
enable_testing()

# Add tests to CTest
add_test(NAME IPC_Crash_Recovery COMMAND test_ipc_crash)
add_test(NAME Gateway_Network_Tests COMMAND test_gateway)

# Set test properties
set_tests_properties(IPC_Crash_Recovery PROPERTIES TIMEOUT 30)
set_tests_properties(Gateway_Network_Tests PROPERTIES TIMEOUT 60)