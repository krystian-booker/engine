#pragma once

#include "types.h"

#ifndef ECS_ENABLE_SIGNATURES
#define ECS_ENABLE_SIGNATURES 0
#endif

#ifndef ECS_SIGNATURE_BITS
#define ECS_SIGNATURE_BITS 64
#endif

#ifndef ECS_USE_SMALL_VECTOR
#define ECS_USE_SMALL_VECTOR 1
#endif

#ifndef ECS_SMALL_VECTOR_INLINE_CAPACITY
#define ECS_SMALL_VECTOR_INLINE_CAPACITY 2
#endif

static_assert(ECS_SIGNATURE_BITS == 64, "Only 64-bit signatures are supported currently");

inline constexpr bool kEcsSignaturesEnabled = ECS_ENABLE_SIGNATURES != 0;

#if ECS_ENABLE_SIGNATURES
using EntitySignature = u64;
#endif
