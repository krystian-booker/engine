#pragma once
#include "core/types.h"
#include <functional>

// Entity is a 64-bit handle: 32-bit index + 32-bit generation
struct Entity {
    u32 index;      // Index into entity array
    u32 generation; // Generation counter for validity checking

    // Invalid entity constant
    static const Entity Invalid;

    bool operator==(const Entity& other) const {
        return index == other.index && generation == other.generation;
    }

    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }

    bool IsValid() const {
        return *this != Invalid;
    }
};

// Hash function for using Entity in unordered_map
namespace std {
    template<>
    struct hash<Entity> {
        size_t operator()(const Entity& e) const {
            return ((u64)e.generation << 32) | e.index;
        }
    };
}
