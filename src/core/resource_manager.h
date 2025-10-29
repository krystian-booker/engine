#pragma once
#include "resource_handle.h"
#include "platform/platform.h"
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
        m_Mutex = Platform::CreateMutex();
    }

    virtual ~ResourceManager() = default;

    // Create resource from data (thread-safe)
    Handle Create(std::unique_ptr<T> resource) {
        Platform::Lock(m_Mutex.get());

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

        Platform::Unlock(m_Mutex.get());
        return handle;
    }

    // Load from file (returns handle, caches by path)
    Handle Load(const std::string& filepath) {
        // Check if already loaded (thread-safe)
        Platform::Lock(m_Mutex.get());
        auto it = m_PathToHandle.find(filepath);
        if (it != m_PathToHandle.end() && IsValid(it->second)) {
            Handle existingHandle = it->second;
            Platform::Unlock(m_Mutex.get());
            return existingHandle;
        }
        Platform::Unlock(m_Mutex.get());

        // Load resource (virtual function, implemented by derived class)
        auto resource = LoadResource(filepath);
        if (!resource) {
            std::cerr << "Failed to load resource: " << filepath << std::endl;
            return Handle::Invalid;
        }

        // Create handle (already thread-safe via mutex in Create())
        Handle handle = Create(std::move(resource));

        // Update path maps (thread-safe)
        Platform::Lock(m_Mutex.get());
        m_PathToHandle[filepath] = handle;
        m_HandleToPath[handle] = filepath;
        Platform::Unlock(m_Mutex.get());

        return handle;
    }

    // Destroy resource (thread-safe)
    void Destroy(Handle handle) {
        if (!IsValid(handle)) {
            return;
        }

        Platform::Lock(m_Mutex.get());

        // Remove from path maps
        auto pathIt = m_HandleToPath.find(handle);
        if (pathIt != m_HandleToPath.end()) {
            m_PathToHandle.erase(pathIt->second);
            m_HandleToPath.erase(pathIt);
        }

        // Free resource
        m_Resources[handle.index].reset();
        m_FreeList.push(handle.index);

        Platform::Unlock(m_Mutex.get());
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

    // Get handle from filepath (thread-safe)
    Handle GetHandle(const std::string& filepath) const {
        Platform::Lock(m_Mutex.get());
        auto it = m_PathToHandle.find(filepath);
        Handle result = (it != m_PathToHandle.end()) ? it->second : Handle::Invalid;
        Platform::Unlock(m_Mutex.get());
        return result;
    }

    // Get filepath from handle (thread-safe)
    std::string GetPath(Handle handle) const {
        Platform::Lock(m_Mutex.get());
        auto it = m_HandleToPath.find(handle);
        std::string result = (it != m_HandleToPath.end()) ? it->second : "";
        Platform::Unlock(m_Mutex.get());
        return result;
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

    // Iterate over all active resources (for invalidation, etc.)
    template<typename Func>
    void ForEachResource(Func func) {
        for (size_t i = 0; i < m_Resources.size(); ++i) {
            if (m_Resources[i]) {
                func(*m_Resources[i]);
            }
        }
    }

private:
    std::vector<std::unique_ptr<T>> m_Resources;
    std::vector<u32> m_Generations;
    std::queue<u32> m_FreeList;

    // Path <-> Handle mapping (prevents duplicate loads)
    std::unordered_map<std::string, Handle> m_PathToHandle;
    std::unordered_map<Handle, std::string> m_HandleToPath;

    // Thread safety
    mutable Platform::MutexPtr m_Mutex;
};
