#pragma once

#include <cstdint>
#include <iostream>

namespace Engine::Ipc {

    // Configurations
    constexpr const char* SHM_BASE_PATH = "/dev/shm/"; 
    constexpr const char* LOCK_BASE_PATH = "/tmp/";
    constexpr size_t CACHE_LINE_SIZE = 64;

    // Total number of message slots in the buffer.
    constexpr uint32_t BUFFER_CAPACITY  = 1024;  
    
    // Maximum number of bytes a single message can contain.
    constexpr uint32_t MAX_MSG_SIZE   = 4096;
}