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
#ifdef _MSC_VER
        strcpy_s(name, MaxLength, "Entity");
#else
        std::strcpy(name, "Entity");
#endif
    }

    Name(const char* str) {
        SetName(str);
    }

    void SetName(const char* str) {
#ifdef _MSC_VER
        strncpy_s(name, MaxLength, str, _TRUNCATE);
#else
        std::strncpy(name, str, MaxLength - 1);
        name[MaxLength - 1] = '\0';  // Ensure null termination
#endif
    }

    const char* GetName() const {
        return name;
    }
};
