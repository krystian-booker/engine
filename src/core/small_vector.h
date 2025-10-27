#pragma once

#include <cstddef>
#include <new>
#include <utility>
#include <type_traits>
#include <algorithm>

// Minimal small-buffer optimization vector for POD/small component storage.
// Provides a subset of std::vector API required by ComponentArray:
// reserve, push_back, pop_back, operator[], data(), begin()/end(), size().
template<typename T, std::size_t InlineCapacity>
class SmallVector {
    static_assert(InlineCapacity > 0, "InlineCapacity must be greater than zero");

public:
    SmallVector() noexcept
        : m_Data(InlineStorage())
        , m_Size(0)
        , m_Capacity(InlineCapacity) {
    }

    SmallVector(const SmallVector& other)
        : SmallVector() {
        CopyFrom(other);
    }

    SmallVector(SmallVector&& other) noexcept
        : SmallVector() {
        MoveFrom(std::move(other));
    }

    ~SmallVector() {
        clear();
        if (UsingHeap()) {
            operator delete[](m_Data);
        }
    }

    SmallVector& operator=(const SmallVector& other) {
        if (this != &other) {
            clear();
            CopyFrom(other);
        }
        return *this;
    }

    SmallVector& operator=(SmallVector&& other) noexcept {
        if (this != &other) {
            clear();
            if (UsingHeap()) {
                operator delete[](m_Data);
            }
            m_Data = InlineStorage();
            m_Capacity = InlineCapacity;
            MoveFrom(std::move(other));
        }
        return *this;
    }

    void reserve(std::size_t newCapacity) {
        if (newCapacity <= m_Capacity) {
            return;
        }
        Reallocate(newCapacity);
    }

    void push_back(const T& value) {
        EnsureCapacityForInsert();
        new (m_Data + m_Size) T(value);
        ++m_Size;
    }

    void push_back(T&& value) {
        EnsureCapacityForInsert();
        new (m_Data + m_Size) T(std::move(value));
        ++m_Size;
    }

    void pop_back() {
        --m_Size;
        m_Data[m_Size].~T();
    }

    std::size_t size() const noexcept {
        return m_Size;
    }

    bool empty() const noexcept {
        return m_Size == 0;
    }

    T* data() noexcept {
        return m_Data;
    }

    const T* data() const noexcept {
        return m_Data;
    }

    T& operator[](std::size_t index) {
        return m_Data[index];
    }

    const T& operator[](std::size_t index) const {
        return m_Data[index];
    }

    T* begin() noexcept { return m_Data; }
    const T* begin() const noexcept { return m_Data; }
    T* end() noexcept { return m_Data + m_Size; }
    const T* end() const noexcept { return m_Data + m_Size; }

    void clear() {
        while (m_Size > 0) {
            pop_back();
        }
    }

private:
    T* InlineStorage() noexcept {
        return reinterpret_cast<T*>(m_InlineBuffer);
    }

    const T* InlineStorage() const noexcept {
        return reinterpret_cast<const T*>(m_InlineBuffer);
    }

    bool UsingHeap() const noexcept {
        return m_Data != InlineStorage();
    }

    void EnsureCapacityForInsert() {
        if (m_Size >= m_Capacity) {
            std::size_t newCapacity = std::max<std::size_t>(m_Capacity * 2, m_Capacity + 1);
            Reallocate(newCapacity);
        }
    }

    void Reallocate(std::size_t newCapacity) {
        T* newStorage = static_cast<T*>(operator new[](newCapacity * sizeof(T)));
        for (std::size_t i = 0; i < m_Size; ++i) {
            new (newStorage + i) T(std::move_if_noexcept(m_Data[i]));
            m_Data[i].~T();
        }

        if (UsingHeap()) {
            operator delete[](m_Data);
        }

        m_Data = newStorage;
        m_Capacity = newCapacity;
    }

    void CopyFrom(const SmallVector& other) {
        reserve(other.m_Size);
        for (std::size_t i = 0; i < other.m_Size; ++i) {
            new (m_Data + i) T(other.m_Data[i]);
        }
        m_Size = other.m_Size;
    }

    void MoveFrom(SmallVector&& other) {
        if (other.UsingHeap()) {
            m_Data = other.m_Data;
            m_Size = other.m_Size;
            m_Capacity = other.m_Capacity;

            other.m_Data = other.InlineStorage();
            other.m_Size = 0;
            other.m_Capacity = InlineCapacity;
        } else {
            for (std::size_t i = 0; i < other.m_Size; ++i) {
                new (m_Data + i) T(std::move(other.m_Data[i]));
                other.m_Data[i].~T();
            }
            m_Size = other.m_Size;
            other.m_Size = 0;
        }
    }

    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    T* m_Data;
    std::size_t m_Size;
    std::size_t m_Capacity;
    Storage m_InlineBuffer[InlineCapacity];
};
