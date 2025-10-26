#pragma once
#include "core/types.h"
#include <functional>

// Generic handle with type safety and generation counter
template<typename T>
struct ResourceHandle {
    u32 index = 0;
    u32 generation = 0;

    static const ResourceHandle Invalid;

    bool operator==(const ResourceHandle& other) const {
        return index == other.index && generation == other.generation;
    }

    bool operator!=(const ResourceHandle& other) const {
        return !(*this == other);
    }

    bool IsValid() const {
        return *this != Invalid;
    }
};

template<typename T>
const ResourceHandle<T> ResourceHandle<T>::Invalid = {0xFFFFFFFF, 0xFFFFFFFF};

// Hash function for using handles in unordered_map
namespace std {
    template<typename T>
    struct hash<ResourceHandle<T>> {
        size_t operator()(const ResourceHandle<T>& handle) const {
            return ((u64)handle.generation << 32) | handle.index;
        }
    };
}

// Forward declarations for resource types
struct MeshData;
struct TextureData;
struct MaterialData;
struct ShaderData;

// Typed handles
using MeshHandle = ResourceHandle<MeshData>;
using TextureHandle = ResourceHandle<TextureData>;
using MaterialHandle = ResourceHandle<MaterialData>;
using ShaderHandle = ResourceHandle<ShaderData>;
