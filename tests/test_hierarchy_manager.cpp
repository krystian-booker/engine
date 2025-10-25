#include "ecs/hierarchy_manager.h"
#include "ecs/entity_manager.h"
#include <iostream>
#include <set>

// Test result tracking
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " << #expr << std::endl; \
        testsFailed++; \
        testsRun++; \
        return; \
    }

// ============================================================================
// HierarchyManager Tests
// ============================================================================

TEST(HierarchyManager_BasicParentChild) {
    HierarchyManager hm;
    Entity parent = {0, 0};
    Entity child = {1, 0};

    hm.SetParent(child, parent);

    ASSERT(hm.GetParent(child) == parent);
    ASSERT(hm.HasChildren(parent));
    ASSERT(hm.GetChildren(parent).size() == 1);
    ASSERT(hm.GetChildren(parent)[0] == child);
}

TEST(HierarchyManager_NoParentReturnsInvalid) {
    HierarchyManager hm;
    Entity entity = {0, 0};

    Entity parent = hm.GetParent(entity);
    ASSERT(parent == Entity::Invalid);
    ASSERT(!parent.IsValid());
}

TEST(HierarchyManager_MultipleChildren) {
    HierarchyManager hm;
    Entity parent = {0, 0};
    Entity child1 = {1, 0};
    Entity child2 = {2, 0};
    Entity child3 = {3, 0};

    hm.SetParent(child1, parent);
    hm.SetParent(child2, parent);
    hm.SetParent(child3, parent);

    ASSERT(hm.HasChildren(parent));
    ASSERT(hm.GetChildren(parent).size() == 3);

    // Verify all children are present
    const auto& children = hm.GetChildren(parent);
    ASSERT(std::find(children.begin(), children.end(), child1) != children.end());
    ASSERT(std::find(children.begin(), children.end(), child2) != children.end());
    ASSERT(std::find(children.begin(), children.end(), child3) != children.end());
}

TEST(HierarchyManager_Reparenting) {
    HierarchyManager hm;
    Entity parent1 = {0, 0};
    Entity parent2 = {1, 0};
    Entity child = {2, 0};

    // Set initial parent
    hm.SetParent(child, parent1);
    ASSERT(hm.GetParent(child) == parent1);
    ASSERT(hm.GetChildren(parent1).size() == 1);

    // Reparent to parent2
    hm.SetParent(child, parent2);
    ASSERT(hm.GetParent(child) == parent2);
    ASSERT(hm.GetChildren(parent2).size() == 1);
    ASSERT(hm.GetChildren(parent1).size() == 0);
    ASSERT(!hm.HasChildren(parent1));
}

TEST(HierarchyManager_RemoveParent) {
    HierarchyManager hm;
    Entity parent = {0, 0};
    Entity child = {1, 0};

    hm.SetParent(child, parent);
    ASSERT(hm.GetParent(child) == parent);

    hm.RemoveParent(child);
    ASSERT(hm.GetParent(child) == Entity::Invalid);
    ASSERT(!hm.HasChildren(parent));
}

TEST(HierarchyManager_RemoveParentOnRootEntity) {
    HierarchyManager hm;
    Entity entity = {0, 0};

    // Should not crash when removing parent from entity with no parent
    hm.RemoveParent(entity);
    ASSERT(hm.GetParent(entity) == Entity::Invalid);
}

TEST(HierarchyManager_OnEntityDestroyed_OrphansChildren) {
    HierarchyManager hm;
    Entity parent = {0, 0};
    Entity child1 = {1, 0};
    Entity child2 = {2, 0};

    hm.SetParent(child1, parent);
    hm.SetParent(child2, parent);

    ASSERT(hm.GetChildren(parent).size() == 2);

    // Destroy parent
    hm.OnEntityDestroyed(parent);

    // Children should be orphaned (no parent)
    ASSERT(hm.GetParent(child1) == Entity::Invalid);
    ASSERT(hm.GetParent(child2) == Entity::Invalid);
    ASSERT(!hm.HasChildren(parent));
}

TEST(HierarchyManager_OnEntityDestroyed_RemovesFromParent) {
    HierarchyManager hm;
    Entity parent = {0, 0};
    Entity child = {1, 0};

    hm.SetParent(child, parent);
    ASSERT(hm.GetChildren(parent).size() == 1);

    // Destroy child
    hm.OnEntityDestroyed(child);

    // Should be removed from parent's children list
    ASSERT(!hm.HasChildren(parent));
    ASSERT(hm.GetChildren(parent).size() == 0);
}

TEST(HierarchyManager_GetRootEntities_SingleRoot) {
    HierarchyManager hm;
    Entity root = {0, 0};
    Entity child1 = {1, 0};
    Entity child2 = {2, 0};

    hm.SetParent(child1, root);
    hm.SetParent(child2, root);

    auto roots = hm.GetRootEntities();
    ASSERT(roots.size() == 1);
    ASSERT(roots[0] == root);
}

TEST(HierarchyManager_GetRootEntities_MultipleRoots) {
    HierarchyManager hm;
    Entity root1 = {0, 0};
    Entity root2 = {1, 0};
    Entity child1 = {2, 0};
    Entity child2 = {3, 0};

    hm.SetParent(child1, root1);
    hm.SetParent(child2, root2);

    auto roots = hm.GetRootEntities();
    ASSERT(roots.size() == 2);

    // Verify both roots are present
    ASSERT(std::find(roots.begin(), roots.end(), root1) != roots.end());
    ASSERT(std::find(roots.begin(), roots.end(), root2) != roots.end());
}

TEST(HierarchyManager_GetRootEntities_EmptyHierarchy) {
    HierarchyManager hm;

    auto roots = hm.GetRootEntities();
    ASSERT(roots.size() == 0);
}

TEST(HierarchyManager_TraverseDepthFirst_SingleLevel) {
    HierarchyManager hm;
    Entity root = {0, 0};
    Entity child1 = {1, 0};
    Entity child2 = {2, 0};

    hm.SetParent(child1, root);
    hm.SetParent(child2, root);

    std::vector<Entity> visited;
    hm.TraverseDepthFirst(root, [&visited](Entity e) {
        visited.push_back(e);
    });

    ASSERT(visited.size() == 3);
    ASSERT(visited[0] == root);  // Root visited first

    // Children should be visited after root
    ASSERT(std::find(visited.begin(), visited.end(), child1) != visited.end());
    ASSERT(std::find(visited.begin(), visited.end(), child2) != visited.end());
}

TEST(HierarchyManager_TraverseDepthFirst_MultiLevel) {
    HierarchyManager hm;
    Entity root = {0, 0};
    Entity child1 = {1, 0};
    Entity child2 = {2, 0};
    Entity grandchild1 = {3, 0};
    Entity grandchild2 = {4, 0};

    // Build hierarchy
    // root has two children: child1 and child2
    // child1 has grandchild1
    // grandchild1 has grandchild2

    hm.SetParent(child1, root);
    hm.SetParent(child2, root);
    hm.SetParent(grandchild1, child1);
    hm.SetParent(grandchild2, grandchild1);

    std::vector<Entity> visited;
    hm.TraverseDepthFirst(root, [&visited](Entity e) {
        visited.push_back(e);
    });

    ASSERT(visited.size() == 5);
    ASSERT(visited[0] == root);  // Root visited first

    // Find indices
    size_t rootIdx = 0;
    size_t child1Idx = std::find(visited.begin(), visited.end(), child1) - visited.begin();
    size_t child2Idx = std::find(visited.begin(), visited.end(), child2) - visited.begin();
    size_t grandchild1Idx = std::find(visited.begin(), visited.end(), grandchild1) - visited.begin();
    size_t grandchild2Idx = std::find(visited.begin(), visited.end(), grandchild2) - visited.begin();

    // Verify depth-first order: parent visited before children
    ASSERT(rootIdx < child1Idx);
    ASSERT(rootIdx < child2Idx);
    ASSERT(child1Idx < grandchild1Idx);
    ASSERT(grandchild1Idx < grandchild2Idx);
}

TEST(HierarchyManager_TraverseDepthFirst_LeafNode) {
    HierarchyManager hm;
    Entity leaf = {0, 0};

    std::vector<Entity> visited;
    hm.TraverseDepthFirst(leaf, [&visited](Entity e) {
        visited.push_back(e);
    });

    ASSERT(visited.size() == 1);
    ASSERT(visited[0] == leaf);
}

TEST(HierarchyManager_HasChildren_ReturnsFalseForLeaf) {
    HierarchyManager hm;
    Entity leaf = {0, 0};

    ASSERT(!hm.HasChildren(leaf));
}

TEST(HierarchyManager_GetChildren_EmptyForLeaf) {
    HierarchyManager hm;
    Entity leaf = {0, 0};

    const auto& children = hm.GetChildren(leaf);
    ASSERT(children.size() == 0);
}

TEST(HierarchyManager_ComplexHierarchy) {
    HierarchyManager hm;

    // Build a complex hierarchy
    // root has children A and B
    // A has children C and D
    // B has child E
    // C has child F

    Entity root = {0, 0};
    Entity A = {1, 0};
    Entity B = {2, 0};
    Entity C = {3, 0};
    Entity D = {4, 0};
    Entity E = {5, 0};
    Entity F = {6, 0};

    hm.SetParent(A, root);
    hm.SetParent(B, root);
    hm.SetParent(C, A);
    hm.SetParent(D, A);
    hm.SetParent(E, B);
    hm.SetParent(F, C);

    // Verify structure
    ASSERT(hm.GetParent(A) == root);
    ASSERT(hm.GetParent(B) == root);
    ASSERT(hm.GetParent(C) == A);
    ASSERT(hm.GetParent(D) == A);
    ASSERT(hm.GetParent(E) == B);
    ASSERT(hm.GetParent(F) == C);

    ASSERT(hm.GetChildren(root).size() == 2);
    ASSERT(hm.GetChildren(A).size() == 2);
    ASSERT(hm.GetChildren(B).size() == 1);
    ASSERT(hm.GetChildren(C).size() == 1);
    ASSERT(hm.GetChildren(D).size() == 0);
    ASSERT(hm.GetChildren(E).size() == 0);
    ASSERT(hm.GetChildren(F).size() == 0);

    // Verify traversal visits all nodes
    std::vector<Entity> visited;
    hm.TraverseDepthFirst(root, [&visited](Entity e) {
        visited.push_back(e);
    });

    ASSERT(visited.size() == 7);
}

TEST(HierarchyManager_ReparentingWithGrandchildren) {
    HierarchyManager hm;
    Entity root = {0, 0};
    Entity parent1 = {1, 0};
    Entity parent2 = {2, 0};
    Entity child = {3, 0};
    Entity grandchild = {4, 0};

    // Initial hierarchy: root -> parent1 -> child -> grandchild
    hm.SetParent(parent1, root);
    hm.SetParent(parent2, root);
    hm.SetParent(child, parent1);
    hm.SetParent(grandchild, child);

    ASSERT(hm.GetChildren(parent1).size() == 1);
    ASSERT(hm.GetChildren(parent2).size() == 0);

    // Reparent child from parent1 to parent2
    hm.SetParent(child, parent2);

    ASSERT(hm.GetParent(child) == parent2);
    ASSERT(hm.GetChildren(parent1).size() == 0);
    ASSERT(hm.GetChildren(parent2).size() == 1);

    // Grandchild should still be child of child
    ASSERT(hm.GetParent(grandchild) == child);
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    std::cout << "=== Hierarchy Manager Unit Tests ===" << std::endl;
    std::cout << std::endl;

    std::cout << "--- Basic Hierarchy Tests ---" << std::endl;
    HierarchyManager_BasicParentChild_runner();
    HierarchyManager_NoParentReturnsInvalid_runner();
    HierarchyManager_MultipleChildren_runner();

    std::cout << std::endl;
    std::cout << "--- Reparenting Tests ---" << std::endl;
    HierarchyManager_Reparenting_runner();
    HierarchyManager_ReparentingWithGrandchildren_runner();

    std::cout << std::endl;
    std::cout << "--- Parent Removal Tests ---" << std::endl;
    HierarchyManager_RemoveParent_runner();
    HierarchyManager_RemoveParentOnRootEntity_runner();

    std::cout << std::endl;
    std::cout << "--- Entity Destruction Tests ---" << std::endl;
    HierarchyManager_OnEntityDestroyed_OrphansChildren_runner();
    HierarchyManager_OnEntityDestroyed_RemovesFromParent_runner();

    std::cout << std::endl;
    std::cout << "--- Root Entity Tests ---" << std::endl;
    HierarchyManager_GetRootEntities_SingleRoot_runner();
    HierarchyManager_GetRootEntities_MultipleRoots_runner();
    HierarchyManager_GetRootEntities_EmptyHierarchy_runner();

    std::cout << std::endl;
    std::cout << "--- Traversal Tests ---" << std::endl;
    HierarchyManager_TraverseDepthFirst_SingleLevel_runner();
    HierarchyManager_TraverseDepthFirst_MultiLevel_runner();
    HierarchyManager_TraverseDepthFirst_LeafNode_runner();

    std::cout << std::endl;
    std::cout << "--- Children Query Tests ---" << std::endl;
    HierarchyManager_HasChildren_ReturnsFalseForLeaf_runner();
    HierarchyManager_GetChildren_EmptyForLeaf_runner();

    std::cout << std::endl;
    std::cout << "--- Complex Hierarchy Tests ---" << std::endl;
    HierarchyManager_ComplexHierarchy_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
