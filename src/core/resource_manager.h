#pragma once
#include "resource_handle.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>

// Base resource manager with handle-based access
template<typename T, typename Handle>
class ResourceManager {
public:
    ResourceManager(u32 initialCapacity = 256) {
        m_Resources.reserve(initialCapacity);
        m_Generations.reserve(initialCapacity);
    }

    virtual ~ResourceManager() = default;

    // Create resource from data
    Handle Create(std::unique_ptr<T> resource) {
        Handle handle;

        if (!m_FreeList.empty()) {
            // Reuse freed slot
            handle.index = m_FreeList.front();
            m_FreeList.pop();
            handle.generation = ++m_Generations[handle.index];
            m_Resources[handle.index] = std::move(resource);
        } else {
            // Allocate new slot
            handle.index = static_cast<u32>(m_Resources.size());
            handle.generation = 0;
            m_Resources.push_back(std::move(resource));
            m_Generations.push_back(0);
        }

        return handle;
    }

    // Load from file (returns handle, caches by path)
    Handle Load(const std::string& filepath) {
        // Check if already loaded
        auto it = m_PathToHandle.find(filepath);
        if (it != m_PathToHandle.end() && IsValid(it->second)) {
            return it->second;
        }

        // Load resource (virtual function, implemented by derived class)
        auto resource = LoadResource(filepath);
        if (!resource) {
            std::cerr << "Failed to load resource: " << filepath << std::endl;
            return Handle::Invalid;
        }

        Handle handle = Create(std::move(resource));
        m_PathToHandle[filepath] = handle;
        m_HandleToPath[handle] = filepath;

        return handle;
    }

    // Destroy resource
    void Destroy(Handle handle) {
        if (!IsValid(handle)) {
            return;
        }

        // Remove from path maps
        auto pathIt = m_HandleToPath.find(handle);
        if (pathIt != m_HandleToPath.end()) {
            m_PathToHandle.erase(pathIt->second);
            m_HandleToPath.erase(pathIt);
        }

        // Free resource
        m_Resources[handle.index].reset();
        m_FreeList.push(handle.index);
    }

    // Get resource (returns nullptr if invalid)
    T* Get(Handle handle) {
        if (!IsValid(handle)) {
            return nullptr;
        }
        return m_Resources[handle.index].get();
    }

    const T* Get(Handle handle) const {
        if (!IsValid(handle)) {
            return nullptr;
        }
        return m_Resources[handle.index].get();
    }

    // Check if handle is valid
    bool IsValid(Handle handle) const {
        return handle.index < m_Generations.size() &&
               m_Generations[handle.index] == handle.generation &&
               m_Resources[handle.index] != nullptr;
    }

    // Get handle from filepath
    Handle GetHandle(const std::string& filepath) const {
        auto it = m_PathToHandle.find(filepath);
        return (it != m_PathToHandle.end()) ? it->second : Handle::Invalid;
    }

    // Get filepath from handle
    std::string GetPath(Handle handle) const {
        auto it = m_HandleToPath.find(handle);
        return (it != m_HandleToPath.end()) ? it->second : "";
    }

    // Resource count (active resources)
    size_t Count() const {
        return m_Resources.size() - m_FreeList.size();
    }

protected:
    // Override in derived classes to implement loading
    virtual std::unique_ptr<T> LoadResource(const std::string& filepath) {
        (void)filepath;
        return nullptr;  // Base implementation
    }

private:
    std::vector<std::unique_ptr<T>> m_Resources;
    std::vector<u32> m_Generations;
    std::queue<u32> m_FreeList;

    // Path <-> Handle mapping (prevents duplicate loads)
    std::unordered_map<std::string, Handle> m_PathToHandle;
    std::unordered_map<Handle, std::string> m_HandleToPath;
};
