#pragma once

#include "core/types.h"
#include <cstring>

/**
 * Name Component
 *
 * Provides a human-readable name for entities.
 * Used in editor hierarchy and debugging.
 */
struct Name {
    static constexpr size_t MaxLength = 64;
    char name[MaxLength];

    Name() {
        std::strcpy(name, "Entity");
    }

    Name(const char* str) {
        SetName(str);
    }

    void SetName(const char* str) {
        std::strncpy(name, str, MaxLength - 1);
        name[MaxLength - 1] = '\0';  // Ensure null termination
    }

    const char* GetName() const {
        return name;
    }
};
