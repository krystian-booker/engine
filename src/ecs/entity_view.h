#pragma once

#include "component_registry.h"
#include "entity_manager.h"
#include <array>
#include <functional>
#include <limits>
#include <tuple>
#include <utility>
#include <iterator>
#include <memory>
#include <cstddef>
#include <algorithm>

// Lightweight zero-allocation view for iterating entities that match a component pack.
// Picks the smallest component array as the driver to minimise Has() checks.
template<typename... Components>
class EntityView {
    using ArrayTuple = std::tuple<std::shared_ptr<ComponentArray<Components>>...>;

public:
    using ValueType = std::tuple<Entity, Components&...>;

    EntityView(ComponentRegistry* registry, EntityManager* entityManager)
        : m_EntityManager(entityManager)
        , m_Arrays(registry->GetComponentArray<Components>()...) {
        SelectDriver();
    }

    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = ValueType;
        using pointer = void;
        using reference = value_type;
        using iterator_category = std::forward_iterator_tag;

        Iterator() = default;

        Iterator(const EntityView* view, size_t index, bool end = false)
            : m_View(view)
            , m_Index(index) {
            if (!end) {
                AdvanceToValid();
            }
        }

        reference operator*() const {
            Entity entity = m_View->m_GetEntity(m_Index);
            return m_View->MakeValue(entity);
        }

        Iterator& operator++() {
            ++m_Index;
            AdvanceToValid();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return m_View == other.m_View && m_Index == other.m_Index;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        void AdvanceToValid() {
            while (m_View && m_Index < m_View->m_DriverSize) {
                Entity entity = m_View->m_GetEntity(m_Index);
                if (m_View->PassesFilters(entity)) {
                    return;
                }
                ++m_Index;
            }
            // Mark as end
            m_Index = m_View ? m_View->m_DriverSize : 0;
        }

        const EntityView* m_View = nullptr;
        size_t m_Index = 0;
    };

    Iterator begin() const {
        return Iterator(this, 0);
    }

    Iterator end() const {
        return Iterator(this, m_DriverSize, true);
    }

    size_t Size() const { return m_DriverSize; }

    template<typename Func>
    void ForRange(size_t beginIndex, size_t endIndex, Func&& func) const {
        if (beginIndex >= m_DriverSize) {
            return;
        }

        endIndex = std::min(endIndex, m_DriverSize);

        for (size_t idx = beginIndex; idx < endIndex; ++idx) {
            Entity entity = m_GetEntity(idx);
            if (!PassesFilters(entity)) {
                continue;
            }

            func(MakeValue(entity));
        }
    }

private:
    template<typename T>
    ComponentArray<T>* GetArray() const {
        return std::get<std::shared_ptr<ComponentArray<T>>>(m_Arrays).get();
    }

    ValueType MakeValue(Entity entity) const {
        return ValueType(entity, GetArray<Components>()->Get(entity)...);
    }

    bool PassesFilters(Entity entity) const {
        if (!entity.IsValid()) {
            return false;
        }
        if (!m_EntityManager->IsAlive(entity)) {
            return false;
        }
        return (GetArray<Components>()->Has(entity) && ...);
    }

    void SelectDriver() {
        if constexpr (sizeof...(Components) == 0) {
            m_DriverSize = 0;
            return;
        }

        std::array<size_t, sizeof...(Components)> sizes = { GetArray<Components>()->Size()... };
        size_t driverIndex = 0;
        size_t smallest = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i] < smallest) {
                smallest = sizes[i];
                driverIndex = i;
            }
        }

        m_DriverSize = smallest;
        AssignDriverFetcher(driverIndex);
    }

    void AssignDriverFetcher(size_t driverIndex) {
        bool assigned = AssignDriverFetcherImpl(driverIndex, std::make_index_sequence<sizeof...(Components)>{});
        if (!assigned) {
            m_GetEntity = [](size_t) { return Entity::Invalid; };
            m_DriverSize = 0;
        }
    }

    template<size_t Index>
    std::function<Entity(size_t)> CreateFetcher() const {
        auto array = std::get<Index>(m_Arrays);
        return [array](size_t idx) -> Entity {
            if (!array || idx >= array->Size()) {
                return Entity::Invalid;
            }
            return array->GetEntity(idx);
        };
    }

    template<size_t... Indices>
    bool AssignDriverFetcherImpl(size_t driverIndex, std::index_sequence<Indices...>) {
        bool assignedFlags[] = { AssignDriverFetcherForIndex<Indices>(driverIndex)... };
        for (bool assigned : assignedFlags) {
            if (assigned) {
                return true;
            }
        }
        return false;
    }

    template<size_t Index>
    bool AssignDriverFetcherForIndex(size_t driverIndex) {
        if (driverIndex == Index) {
            m_GetEntity = CreateFetcher<Index>();
            return true;
        }
        return false;
    }

    EntityManager* m_EntityManager = nullptr;
    ArrayTuple m_Arrays;
    size_t m_DriverSize = 0;
    std::function<Entity(size_t)> m_GetEntity;
};
