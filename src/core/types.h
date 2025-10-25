#pragma once

#include <cstdint>
#include <cassert>

// Custom integer type aliases
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Floating point type aliases
using f32 = float;
using f64 = double;

// Utility macros
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

// Align a value up to the nearest multiple of alignment
#define ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

// Debug assertion
#ifdef NDEBUG
    #define ENGINE_ASSERT(expr) ((void)0)
#else
    #define ENGINE_ASSERT(expr) assert(expr)
#endif
