#pragma once

#include <cstdint>
#include <cstddef>

// Platform-specific types
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// Essential macros
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))

// Assert macro
#ifdef _DEBUG
#define ENGINE_ASSERT(expr) \
if (!(expr)) { \
std::cerr << "Assertion failed: " << #expr << std::endl; \
std::abort(); \
}
#else
#define ENGINE_ASSERT(expr) ((void)0)
#endif