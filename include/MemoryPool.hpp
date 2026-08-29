#pragma once

//MemoryPool is a fixed capacity single threaded object pool
//
// Design
// 

#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace itch {

template <typename T>

class MemoryPool {
    static_assert(
        sizeof(T) >= sizeof(void*),
        "MemoryPool<T> requires sizeof(T) >= sizeof(void*)"
    );

};

}