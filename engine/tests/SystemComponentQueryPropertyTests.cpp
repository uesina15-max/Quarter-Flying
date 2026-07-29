#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../ecs/Components.h"
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <set>
#include <atomic>
#include <memory>
#include <utility>

using namespace Engine;

// ============================================================================
// Task 9.8: ECS System Component Query Property Tests
// Feature: game-engine-core-systems, Property 20: System Component 쿼리 정확성
// **Validates: Requirements 7.3**
// ============================================================================

// Test component types for property testing
struct SystemQueryTestComponentA { 
    int value; 
    SystemQueryTestComponentA(int v = 0) : value(v) {}
    bool operator==(const SystemQueryTestComponentA& other) const { return value == other.value; }
};

struct SystemQueryTestComponentB { 
    float value; 
    SystemQueryTestComponentB(float v = 0.0f) : value(v) {}
    bool operator==(const SystemQueryTestComponentB& other) const { return value == other.value; }
};

struct SystemQueryTestComponentC { 
    bool value; 
    SystemQueryTestComponentC(bool v = false) : value(v) {}
    bool operator==(const SystemQueryTestComponentC& other) const { return value == other.value; }
};

struct SystemQueryTestComponentD { 
    double value; 
    SystemQueryTestComponentD(double v = 0.0) : value(v) {}
    bool operator==(const SystemQueryTestComponentD& other) const { return value == other.value; }
};

namespace
{
    // ========================================
    // Test System Implementations
    // ========================================
    
    // System that queries for single component type A
    class SingleComponentQuerySystem : public System
    {
    public:
        int priority = 0;
        int GetPriority() const override { return priority; }
        mutable std::atomic<int> updateCount{0};
        mutable std::vector<Entity> queriedEntities;
        mutable std::vector<SystemQueryTestComponentA> queriedComponents;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            queriedEntities.clear();
            queriedComponents.clear();
            
            // Query for entities with component A
                        auto array = registry.GetComponentArray<SystemQueryTestComponentA>();
            if (array) {
                auto& entities = array->GetEntityIDs();
                for (size_t i = 0; i < array->Size(); ++i) {
                    Entity entity(entities[i]);
                    queriedEntities.push_back(entity);
                    auto* component = registry.GetComponent<SystemQueryTestComponentA>(entity);
                    if (component) {
                        queriedComponents.push_back(*component);
                    }
                }
            }
        }
        
        const char* GetName() const override { return "SingleComponentQuerySystem"; }
        
        std::vector<size_t> GetReadComponentTypes() const override
        {
            return { typeid(SystemQueryTestComponentA).hash_code() };
        }
    };
    
    // System that queries for two component types A and B
    class TwoComponentQuerySystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        mutable std::vector<Entity> queriedEntities;
        mutable std::vector<std::pair<SystemQueryTestComponentA, SystemQueryTestComponentB>> queriedComponents;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            queriedEntities.clear();
            queriedComponents.clear();
            
            // Query for entities with both components A and B
            auto arrayA = registry.GetComponentArray<SystemQueryTestComponentA>();
            if (arrayA) {
                auto& entities = arrayA->GetEntityIDs();
                for (size_t i = 0; i < arrayA->Size(); ++i) {
                    Entity entity(entities[i]);
                    if (registry.HasComponent<SystemQueryTestComponentB>(entity)) {
                        queriedEntities.push_back(entity);
                        auto* compA = registry.GetComponent<SystemQueryTestComponentA>(entity);
                        auto* compB = registry.GetComponent<SystemQueryTestComponentB>(entity);
                        if (compA && compB) {
                            queriedComponents.push_back({*compA, *compB});
                        }
                    }
                }
            }
        }
        
        const char* GetName() const override { return "TwoComponentQuerySystem"; }
        
        std::vector<size_t> GetReadComponentTypes() const override
        {
            return { 
                typeid(SystemQueryTestComponentA).hash_code(),
                typeid(SystemQueryTestComponentB).hash_code()
            };
        }
    };
    
    // System that queries for three component types A, B, and C
    class ThreeComponentQuerySystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        mutable std::vector<Entity> queriedEntities;
        mutable std::vector<std::tuple<SystemQueryTestComponentA, SystemQueryTestComponentB, SystemQueryTestComponentC>> queriedComponents;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            queriedEntities.clear();
            queriedComponents.clear();
            
            // Query for entities with components A, B, and C
            auto arrayA = registry.GetComponentArray<SystemQueryTestComponentA>();
            if (arrayA) {
                auto& entities = arrayA->GetEntityIDs();
                for (size_t i = 0; i < arrayA->Size(); ++i) {
                    Entity entity(entities[i]);
                    if (registry.HasComponent<SystemQueryTestComponentB>(entity) &&
                        registry.HasComponent<SystemQueryTestComponentC>(entity)) {
                        queriedEntities.push_back(entity);
                        auto* compA = registry.GetComponent<SystemQueryTestComponentA>(entity);
                        auto* compB = registry.GetComponent<SystemQueryTestComponentB>(entity);
                        auto* compC = registry.GetComponent<SystemQueryTestComponentC>(entity);
                        if (compA && compB && compC) {
                            queriedComponents.push_back({*compA, *compB, *compC});
                        }
                    }
                }
            }
        }
        
        const char* GetName() const override { return "ThreeComponentQuerySystem"; }
        
        std::vector<size_t> GetReadComponentTypes() const override
        {
            return { 
                typeid(SystemQueryTestComponentA).hash_code(),
                typeid(SystemQueryTestComponentB).hash_code(),
                typeid(SystemQueryTestComponentC).hash_code()
            };
        }
    };
    
    // System that queries for all four component types
    class FourComponentQuerySystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        mutable std::vector<Entity> queriedEntities;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            queriedEntities.clear();
            
            // Query for entities with all four components
                        auto arrayA = registry.GetComponentArray<SystemQueryTestComponentA>();
            if (arrayA) {
                auto& entities = arrayA->GetEntityIDs();
                for (size_t i = 0; i < arrayA->Size(); ++i) {
                    Entity entity(entities[i]);
                    if (registry.HasComponent<SystemQueryTestComponentB>(entity) &&
                        registry.HasComponent<SystemQueryTestComponentC>(entity) &&
                        registry.HasComponent<SystemQueryTestComponentD>(entity)) {
                        queriedEntities.push_back(entity);
                    }
                }
            }
        }
        
        const char* GetName() const override { return "FourComponentQuerySystem"; }
        
        std::vector<size_t> GetReadComponentTypes() const override
        {
            return { 
                typeid(SystemQueryTestComponentA).hash_code(),
                typeid(SystemQueryTestComponentB).hash_code(),
                typeid(SystemQueryTestComponentC).hash_code(),
                typeid(SystemQueryTestComponentD).hash_code()
            };
        }
    };
    
    // System that modifies components during query
    class ComponentModifyingSystem : public System
    {
    public:
        int priority = 0;
        int GetPriority() const override { return priority; }
        mutable std::atomic<int> updateCount{0};
        mutable std::vector<Entity> processedEntities;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            processedEntities.clear();
            
            // Query and modify component A
                        auto array = registry.GetComponentArray<SystemQueryTestComponentA>();
            if (array) {
                auto& entities = array->GetEntityIDs();
                for (size_t i = 0; i < array->Size(); ++i) {
                    Entity entity(entities[i]);
                    processedEntities.push_back(entity);
                    auto* component = registry.GetComponent<SystemQueryTestComponentA>(entity);
                    if (component) {
                        component->value += 1; // Modify the component
                    }
                }
            }
        }
        
        const char* GetName() const override { return "ComponentModifyingSystem"; }
        
        std::vector<size_t> GetWriteComponentTypes() const override
        {
            return { typeid(SystemQueryTestComponentA).hash_code() };
        }
    };

} // anonymous namespace

class SystemComponentQueryPropertyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        world = std::make_unique<World>();
    }
    
    void TearDown() override
    {
        world.reset();
    }
    
    // Helper functions to generate random component values
    SystemQueryTestComponentA GenerateRandomComponentA()
    {
        return SystemQueryTestComponentA(*rc::gen::arbitrary<int>());
    }
    
    SystemQueryTestComponentB GenerateRandomComponentB()
    {
        return SystemQueryTestComponentB(*rc::gen::arbitrary<float>());
    }
    
    SystemQueryTestComponentC GenerateRandomComponentC()
    {
        return SystemQueryTestComponentC(*rc::gen::arbitrary<bool>());
    }
    
    SystemQueryTestComponentD GenerateRandomComponentD()
    {
        return SystemQueryTestComponentD(*rc::gen::arbitrary<double>());
    }
    
    std::unique_ptr<World> world;
};

// ========================================
// Property-Based Tests
// ========================================

/**
 * Property 20: System Component Query Accuracy
 * **Validates: Requirements 7.3**
 * 
 * Systems should be able to accurately query for entities with specific component
 * combinations through the ECS registry. The query results must be complete and
 * accurate, returning exactly those entities that have the requested components.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryAccuracyProperty, ())
{
    // Generate random entities (10-50 entities for comprehensive testing)
    const auto numEntities = *rc::gen::inRange(10, 51);
    std::vector<Entity> entities;
    
    // Track which entities have which components
    std::unordered_set<Entity> entitiesWithA;
    std::unordered_set<Entity> entitiesWithB;
    std::unordered_set<Entity> entitiesWithC;
    std::unordered_set<Entity> entitiesWithD;
    
    auto* registry = world->GetRegistry();
    
    // Step 1: Create entities with random component combinations
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        // Randomly decide which components to add (40% chance for each to ensure variety)
        bool addA = (*rc::gen::inRange(0, 10) < 4);
        bool addB = (*rc::gen::inRange(0, 10) < 4);
        bool addC = (*rc::gen::inRange(0, 10) < 4);
        bool addD = (*rc::gen::inRange(0, 10) < 4);
        
        if (addA) {
            SystemQueryTestComponentA compA = GenerateRandomComponentA();
            registry->AddComponent(entity, compA);
            entitiesWithA.insert(entity);
        }
        
        if (addB) {
            SystemQueryTestComponentB compB = GenerateRandomComponentB();
            registry->AddComponent(entity, compB);
            entitiesWithB.insert(entity);
        }
        
        if (addC) {
            SystemQueryTestComponentC compC = GenerateRandomComponentC();
            registry->AddComponent(entity, compC);
            entitiesWithC.insert(entity);
        }
        
        if (addD) {
            SystemQueryTestComponentD compD = GenerateRandomComponentD();
            registry->AddComponent(entity, compD);
            entitiesWithD.insert(entity);
        }
    }
    
    // Step 2: Register and test single component query system
    {
        auto singleQuerySystem = std::make_unique<SingleComponentQuerySystem>();
        SingleComponentQuerySystem* systemPtr = singleQuerySystem.get();
        
        world->RegisterSystem(std::move(singleQuerySystem));
        world->Initialize();
        world->Update(0.016f);
        
        // Property check: System should have been updated
        EXPECT_TRUE(systemPtr->updateCount.load() == 1);
        
        // Property check: System should query exactly entities with component A
        std::unordered_set<Entity> systemQueriedEntities(
            systemPtr->queriedEntities.begin(), 
            systemPtr->queriedEntities.end()
        );
        
        EXPECT_TRUE(systemQueriedEntities.size() == entitiesWithA.size());
        
        for (const Entity& entity : entitiesWithA) {
            EXPECT_TRUE(systemQueriedEntities.find(entity) != systemQueriedEntities.end());
        }
        
        for (const Entity& entity : systemPtr->queriedEntities) {
            EXPECT_TRUE(entitiesWithA.find(entity) != entitiesWithA.end());
            
            // Property check: System should be able to access the component
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentA>(entity); RC_ASSERT(_tmp); }
        }
        
        // Property check: Component values should match
        EXPECT_TRUE(systemPtr->queriedComponents.size() == systemPtr->queriedEntities.size());
        
        world->Shutdown();
    }
    
    // Step 3: Test two component query system
    {
        // Calculate expected entities with both A and B
        std::unordered_set<Entity> expectedAB;
        for (const Entity& entity : entitiesWithA) {
            if (entitiesWithB.find(entity) != entitiesWithB.end()) {
                expectedAB.insert(entity);
            }
        }
        
        auto twoQuerySystem = std::make_unique<TwoComponentQuerySystem>();
        TwoComponentQuerySystem* systemPtr = twoQuerySystem.get();
        
        world->RegisterSystem(std::move(twoQuerySystem));
        world->Initialize();
        world->Update(0.016f);
        
        // Property check: System should have been updated
        EXPECT_TRUE(systemPtr->updateCount.load() == 1);
        
        // Property check: System should query exactly entities with both A and B
        std::unordered_set<Entity> systemQueriedEntities(
            systemPtr->queriedEntities.begin(), 
            systemPtr->queriedEntities.end()
        );
        
        EXPECT_TRUE(systemQueriedEntities.size() == expectedAB.size());
        
        for (const Entity& entity : expectedAB) {
            EXPECT_TRUE(systemQueriedEntities.find(entity) != systemQueriedEntities.end());
        }
        
        for (const Entity& entity : systemPtr->queriedEntities) {
            EXPECT_TRUE(expectedAB.find(entity) != expectedAB.end());
            
            // Property check: System should be able to access both components
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentA>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentB>(entity); RC_ASSERT(_tmp); }
        }
        
        world->Shutdown();
    }
    
    // Step 4: Test three component query system
    {
        // Calculate expected entities with A, B, and C
        std::unordered_set<Entity> expectedABC;
        for (const Entity& entity : entitiesWithA) {
            if (entitiesWithB.find(entity) != entitiesWithB.end() &&
                entitiesWithC.find(entity) != entitiesWithC.end()) {
                expectedABC.insert(entity);
            }
        }
        
        auto threeQuerySystem = std::make_unique<ThreeComponentQuerySystem>();
        ThreeComponentQuerySystem* systemPtr = threeQuerySystem.get();
        
        world->RegisterSystem(std::move(threeQuerySystem));
        world->Initialize();
        world->Update(0.016f);
        
        // Property check: System should have been updated
        EXPECT_TRUE(systemPtr->updateCount.load() == 1);
        
        // Property check: System should query exactly entities with A, B, and C
        std::unordered_set<Entity> systemQueriedEntities(
            systemPtr->queriedEntities.begin(), 
            systemPtr->queriedEntities.end()
        );
        
        EXPECT_TRUE(systemQueriedEntities.size() == expectedABC.size());
        
        for (const Entity& entity : expectedABC) {
            EXPECT_TRUE(systemQueriedEntities.find(entity) != systemQueriedEntities.end());
        }
        
        for (const Entity& entity : systemPtr->queriedEntities) {
            EXPECT_TRUE(expectedABC.find(entity) != expectedABC.end());
            
            // Property check: System should be able to access all three components
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentA>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentB>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentC>(entity); RC_ASSERT(_tmp); }
        }
        
        world->Shutdown();
    }
    
    // Step 5: Test four component query system
    {
        // Calculate expected entities with all four components
        std::unordered_set<Entity> expectedABCD;
        for (const Entity& entity : entitiesWithA) {
            if (entitiesWithB.find(entity) != entitiesWithB.end() &&
                entitiesWithC.find(entity) != entitiesWithC.end() &&
                entitiesWithD.find(entity) != entitiesWithD.end()) {
                expectedABCD.insert(entity);
            }
        }
        
        auto fourQuerySystem = std::make_unique<FourComponentQuerySystem>();
        FourComponentQuerySystem* systemPtr = fourQuerySystem.get();
        
        world->RegisterSystem(std::move(fourQuerySystem));
        world->Initialize();
        world->Update(0.016f);
        
        // Property check: System should have been updated
        EXPECT_TRUE(systemPtr->updateCount.load() == 1);
        
        // Property check: System should query exactly entities with all four components
        std::unordered_set<Entity> systemQueriedEntities(
            systemPtr->queriedEntities.begin(), 
            systemPtr->queriedEntities.end()
        );
        
        EXPECT_TRUE(systemQueriedEntities.size() == expectedABCD.size());
        
        for (const Entity& entity : expectedABCD) {
            EXPECT_TRUE(systemQueriedEntities.find(entity) != systemQueriedEntities.end());
        }
        
        for (const Entity& entity : systemPtr->queriedEntities) {
            EXPECT_TRUE(expectedABCD.find(entity) != expectedABCD.end());
            
            // Property check: System should be able to access all four components
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentA>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentB>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentC>(entity); RC_ASSERT(_tmp); }
            { bool _tmp = registry->HasComponent<SystemQueryTestComponentD>(entity); RC_ASSERT(_tmp); }
        }
        
        world->Shutdown();
    }
}
/**
 * Property 20 (Extended): System Component Query Accuracy with Dynamic Changes
 * **Validates: Requirements 7.3**
 * 
 * Systems should maintain query accuracy even when components are added or removed
 * during runtime, ensuring that subsequent queries reflect the current state.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryDynamicChangesProperty, ())
{
    // Generate initial entities
    const auto numEntities = *rc::gen::inRange(5, 25);
    std::vector<Entity> entities;
    std::unordered_set<Entity> entitiesWithA;
    
    auto* registry = world->GetRegistry();
    
    // Step 1: Create entities with component A
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        if (i % 2 == 0) { // Half the entities get component A initially
            SystemQueryTestComponentA compA = GenerateRandomComponentA();
            registry->AddComponent(entity, compA);
            entitiesWithA.insert(entity);
        }
    }
    
    // Step 2: Register system and perform initial query
    auto querySystem = std::make_unique<SingleComponentQuerySystem>();
    SingleComponentQuerySystem* systemPtr = querySystem.get();
    
    world->RegisterSystem(std::move(querySystem));
    world->Initialize();
    world->Update(0.016f);
    
    // Property check: Initial query should be accurate
    std::unordered_set<Entity> initialQueriedEntities(
        systemPtr->queriedEntities.begin(), 
        systemPtr->queriedEntities.end()
    );
    
    EXPECT_TRUE(initialQueriedEntities.size() == entitiesWithA.size());
    
    // Step 3: Dynamically add components to some entities
    const auto numAdditions = *rc::gen::inRange(1, std::min(5, numEntities / 2));
    for (int i = 0; i < numAdditions; ++i) {
        // Find an entity without component A
        Entity targetEntity;
        bool foundEntity = false;
        for (const Entity& entity : entities) {
            if (entitiesWithA.find(entity) == entitiesWithA.end()) {
                targetEntity = entity;
                foundEntity = true;
                break;
            }
        }
        
        if (foundEntity) {
            SystemQueryTestComponentA compA = GenerateRandomComponentA();
            registry->AddComponent(targetEntity, compA);
            entitiesWithA.insert(targetEntity);
        }
    }
    
    // Step 4: Update system and verify query reflects changes
    world->Update(0.016f);
    
    std::unordered_set<Entity> updatedQueriedEntities(
        systemPtr->queriedEntities.begin(), 
        systemPtr->queriedEntities.end()
    );
    
    EXPECT_TRUE(updatedQueriedEntities.size() == entitiesWithA.size());
    
    for (const Entity& entity : entitiesWithA) {
        EXPECT_TRUE(updatedQueriedEntities.find(entity) != updatedQueriedEntities.end());
    }
    
    // Step 5: Dynamically remove components from some entities
    const auto numRemovals = *rc::gen::inRange(1, std::min(3, (int)entitiesWithA.size()));
    std::vector<Entity> entitiesToRemove;
    
    auto it = entitiesWithA.begin();
    for (int i = 0; i < numRemovals && it != entitiesWithA.end(); ++i, ++it) {
        entitiesToRemove.push_back(*it);
    }
    
    for (const Entity& entity : entitiesToRemove) {
        registry->RemoveComponent<SystemQueryTestComponentA>(entity);
        entitiesWithA.erase(entity);
    }
    
    // Step 6: Update system and verify query reflects removals
    world->Update(0.016f);
    
    std::unordered_set<Entity> finalQueriedEntities(
        systemPtr->queriedEntities.begin(), 
        systemPtr->queriedEntities.end()
    );
    
    EXPECT_TRUE(finalQueriedEntities.size() == entitiesWithA.size());
    
    for (const Entity& entity : entitiesWithA) {
        EXPECT_TRUE(finalQueriedEntities.find(entity) != finalQueriedEntities.end());
    }
    
    // Property check: Removed entities should not be in query results
    for (const Entity& removedEntity : entitiesToRemove) {
        EXPECT_TRUE(finalQueriedEntities.find(removedEntity) == finalQueriedEntities.end());
    }
    
    world->Shutdown();
}

/**
 * Property 20 (Stress Test): System Component Query Accuracy under High Load
 * **Validates: Requirements 7.3**
 * 
 * Systems should maintain query accuracy even with many entities and multiple
 * systems querying simultaneously.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryStressTestProperty, ())
{
    // Generate a larger number of entities for stress testing
    const auto numEntities = *rc::gen::inRange(100, 300);
    std::vector<Entity> entities;
    
    // Track component assignments with predictable patterns
    std::vector<std::set<int>> entityComponents(numEntities); // 0=A, 1=B, 2=C, 3=D
    
    auto* registry = world->GetRegistry();
    
    // Create entities with various component combinations using deterministic patterns
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        // Use entity index to create predictable but varied patterns
        bool addA = (i % 3) == 0;  // Every 3rd entity
        bool addB = (i % 5) == 0;  // Every 5th entity  
        bool addC = (i % 7) == 0;  // Every 7th entity
        bool addD = (i % 11) == 0; // Every 11th entity
        
        if (addA) {
            registry->AddComponent(entity, GenerateRandomComponentA());
            entityComponents[i].insert(0);
        }
        
        if (addB) {
            registry->AddComponent(entity, GenerateRandomComponentB());
            entityComponents[i].insert(1);
        }
        
        if (addC) {
            registry->AddComponent(entity, GenerateRandomComponentC());
            entityComponents[i].insert(2);
        }
        
        if (addD) {
            registry->AddComponent(entity, GenerateRandomComponentD());
            entityComponents[i].insert(3);
        }
    }
    
    // Register multiple systems with different query patterns
    auto singleQuerySystem = std::make_unique<SingleComponentQuerySystem>();
    auto twoQuerySystem = std::make_unique<TwoComponentQuerySystem>();
    auto threeQuerySystem = std::make_unique<ThreeComponentQuerySystem>();
    auto fourQuerySystem = std::make_unique<FourComponentQuerySystem>();
    
    SingleComponentQuerySystem* singlePtr = singleQuerySystem.get();
    TwoComponentQuerySystem* twoPtr = twoQuerySystem.get();
    ThreeComponentQuerySystem* threePtr = threeQuerySystem.get();
    FourComponentQuerySystem* fourPtr = fourQuerySystem.get();
    
    world->RegisterSystem(std::move(singleQuerySystem));
    world->RegisterSystem(std::move(twoQuerySystem));
    world->RegisterSystem(std::move(threeQuerySystem));
    world->RegisterSystem(std::move(fourQuerySystem));
    
    world->Initialize();
    
    // Run multiple update cycles to stress test
    const int numUpdateCycles = *rc::gen::inRange(3, 10);
    for (int cycle = 0; cycle < numUpdateCycles; ++cycle) {
        world->Update(0.016f);
        
        // Calculate expected results for verification
        auto countEntitiesWithComponents = [&](const std::set<int>& requiredComponents) -> size_t {
            size_t count = 0;
            for (const auto& entityComps : entityComponents) {
                bool hasAll = true;
                for (int comp : requiredComponents) {
                    if (entityComps.find(comp) == entityComps.end()) {
                        hasAll = false;
                        break;
                    }
                }
                if (hasAll) count++;
            }
            return count;
        };
        
        // Verify single component query
        size_t expectedA = countEntitiesWithComponents({0});
        EXPECT_TRUE(singlePtr->queriedEntities.size() == expectedA);
        
        // Verify two component query
        size_t expectedAB = countEntitiesWithComponents({0, 1});
        EXPECT_TRUE(twoPtr->queriedEntities.size() == expectedAB);
        
        // Verify three component query
        size_t expectedABC = countEntitiesWithComponents({0, 1, 2});
        EXPECT_TRUE(threePtr->queriedEntities.size() == expectedABC);
        
        // Verify four component query
        size_t expectedABCD = countEntitiesWithComponents({0, 1, 2, 3});
        EXPECT_TRUE(fourPtr->queriedEntities.size() == expectedABCD);
    }
    
    // Property check: All systems should have been updated the expected number of times
    EXPECT_TRUE(singlePtr->updateCount.load() == numUpdateCycles);
    EXPECT_TRUE(twoPtr->updateCount.load() == numUpdateCycles);
    EXPECT_TRUE(threePtr->updateCount.load() == numUpdateCycles);
    EXPECT_TRUE(fourPtr->updateCount.load() == numUpdateCycles);
    
    world->Shutdown();
}
/**
 * Property 20 (Component Modification): System Component Query with Modifications
 * **Validates: Requirements 7.3**
 * 
 * Systems should be able to query and modify components accurately, with subsequent
 * queries reflecting the modifications made by previous systems.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryModificationProperty, ())
{
    // Generate entities with component A
    const auto numEntities = *rc::gen::inRange(10, 30);
    std::vector<Entity> entities;
    std::vector<int> originalValues;
    
    auto* registry = world->GetRegistry();
    
    // Create entities with component A and track original values
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        SystemQueryTestComponentA compA = GenerateRandomComponentA();
        originalValues.push_back(compA.value);
        registry->AddComponent(entity, compA);
    }
    
    // Register a system that modifies components and a system that reads them
    auto modifyingSystem = std::make_unique<ComponentModifyingSystem>();
    auto readingSystem = std::make_unique<SingleComponentQuerySystem>();
    
    ComponentModifyingSystem* modifyPtr = modifyingSystem.get();
    SingleComponentQuerySystem* readPtr = readingSystem.get();
    
    // Register modifying system first (lower priority) so it runs before reading system
    modifyingSystem->priority = -1; // Higher priority
    readingSystem->priority = 1;    // Lower priority
    
    world->RegisterSystem(std::move(modifyingSystem));
    world->RegisterSystem(std::move(readingSystem));
    
    world->Initialize();
    world->Update(0.016f);
    
    // Property check: Both systems should have been updated
    EXPECT_TRUE(modifyPtr->updateCount.load() == 1);
    EXPECT_TRUE(readPtr->updateCount.load() == 1);
    
    // Property check: Modifying system should have processed all entities
    EXPECT_TRUE(modifyPtr->processedEntities.size() == numEntities);
    
    // Property check: Reading system should have queried all entities
    EXPECT_TRUE(readPtr->queriedEntities.size() == numEntities);
    
    // Property check: Component values should be modified correctly
    for (size_t i = 0; i < entities.size(); ++i) {
        Entity entity(entities[i]);
        auto* component = registry->GetComponent<SystemQueryTestComponentA>(entity);
        
        EXPECT_TRUE(component != nullptr);
        
        int expectedValue = originalValues[i] + 1; // Modifying system adds 1
        EXPECT_TRUE(component->value == expectedValue);
    }
    
    // Property check: Reading system should have read the modified values
    EXPECT_TRUE(readPtr->queriedComponents.size() == numEntities);
    
    for (size_t i = 0; i < readPtr->queriedComponents.size(); ++i) {
        int expectedValue = originalValues[i] + 1; // Should reflect the modification
        EXPECT_TRUE(readPtr->queriedComponents[i].value == expectedValue);
    }
    
    world->Shutdown();
}

/**
 * Property 20 (Empty Query): System Component Query with No Matching Entities
 * **Validates: Requirements 7.3**
 * 
 * Systems should handle empty query results gracefully when no entities match
 * the requested component combination.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryEmptyResultsProperty, ())
{
    // Generate entities with only component A (no B, C, or D)
    const auto numEntities = *rc::gen::inRange(5, 20);
    std::vector<Entity> entities;
    
    auto* registry = world->GetRegistry();
    
    // Create entities with only component A
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        SystemQueryTestComponentA compA = GenerateRandomComponentA();
        registry->AddComponent(entity, compA);
    }
    
    // Register systems that query for component combinations that don't exist
    auto twoQuerySystem = std::make_unique<TwoComponentQuerySystem>();      // Queries A+B (should be empty)
    auto threeQuerySystem = std::make_unique<ThreeComponentQuerySystem>();  // Queries A+B+C (should be empty)
    auto fourQuerySystem = std::make_unique<FourComponentQuerySystem>();    // Queries A+B+C+D (should be empty)
    
    TwoComponentQuerySystem* twoPtr = twoQuerySystem.get();
    ThreeComponentQuerySystem* threePtr = threeQuerySystem.get();
    FourComponentQuerySystem* fourPtr = fourQuerySystem.get();
    
    world->RegisterSystem(std::move(twoQuerySystem));
    world->RegisterSystem(std::move(threeQuerySystem));
    world->RegisterSystem(std::move(fourQuerySystem));
    
    world->Initialize();
    world->Update(0.016f);
    
    // Property check: All systems should have been updated
    EXPECT_TRUE(twoPtr->updateCount.load() == 1);
    EXPECT_TRUE(threePtr->updateCount.load() == 1);
    EXPECT_TRUE(fourPtr->updateCount.load() == 1);
    
    // Property check: All queries should return empty results
    EXPECT_TRUE(twoPtr->queriedEntities.empty());
    EXPECT_TRUE(threePtr->queriedEntities.empty());
    EXPECT_TRUE(fourPtr->queriedEntities.empty());
    
    // Property check: Component collections should also be empty
    EXPECT_TRUE(twoPtr->queriedComponents.empty());
    EXPECT_TRUE(threePtr->queriedComponents.empty());
    
    world->Shutdown();
}

/**
 * Property 20 (Multiple Updates): System Component Query Consistency Across Updates
 * **Validates: Requirements 7.3**
 * 
 * Systems should maintain consistent query results across multiple update cycles
 * when the underlying entity/component data hasn't changed.
 */
RC_GTEST_FIXTURE_PROP(SystemComponentQueryPropertyTest, SystemComponentQueryConsistencyProperty, ())
{
    // Generate entities with stable component combinations
    const auto numEntities = *rc::gen::inRange(10, 40);
    std::vector<Entity> entities;
    std::unordered_set<Entity> entitiesWithA;
    std::unordered_set<Entity> entitiesWithAB;
    
    auto* registry = world->GetRegistry();
    
    // Create entities with predictable component patterns
    for (int i = 0; i < numEntities; ++i) {
        Entity entity = registry->CreateEntity();
        entities.push_back(entity);
        
        // Every entity gets component A
        SystemQueryTestComponentA compA = GenerateRandomComponentA();
        registry->AddComponent(entity, compA);
        entitiesWithA.insert(entity);
        
        // Every other entity also gets component B
        if (i % 2 == 0) {
            SystemQueryTestComponentB compB = GenerateRandomComponentB();
            registry->AddComponent(entity, compB);
            entitiesWithAB.insert(entity);
        }
    }
    
    // Register query systems
    auto singleQuerySystem = std::make_unique<SingleComponentQuerySystem>();
    auto twoQuerySystem = std::make_unique<TwoComponentQuerySystem>();
    
    SingleComponentQuerySystem* singlePtr = singleQuerySystem.get();
    TwoComponentQuerySystem* twoPtr = twoQuerySystem.get();
    
    world->RegisterSystem(std::move(singleQuerySystem));
    world->RegisterSystem(std::move(twoQuerySystem));
    
    world->Initialize();
    
    // Run multiple update cycles and verify consistency
    const int numUpdateCycles = *rc::gen::inRange(5, 15);
    std::vector<std::vector<Entity>> singleQueryResults;
    std::vector<std::vector<Entity>> twoQueryResults;
    
    for (int cycle = 0; cycle < numUpdateCycles; ++cycle) {
        world->Update(0.016f);
        
        // Store query results for consistency checking
        singleQueryResults.push_back(singlePtr->queriedEntities);
        twoQueryResults.push_back(twoPtr->queriedEntities);
        
        // Property check: Query results should match expected sets
        std::unordered_set<Entity> currentSingleResults(
            singlePtr->queriedEntities.begin(), 
            singlePtr->queriedEntities.end()
        );
        std::unordered_set<Entity> currentTwoResults(
            twoPtr->queriedEntities.begin(), 
            twoPtr->queriedEntities.end()
        );
        
        EXPECT_TRUE(currentSingleResults.size() == entitiesWithA.size());
        
        EXPECT_TRUE(currentTwoResults.size() == entitiesWithAB.size());
        
        // Verify exact entity matches
        for (const Entity& entity : entitiesWithA) {
            EXPECT_TRUE(currentSingleResults.find(entity) != currentSingleResults.end());
        }
        
        for (const Entity& entity : entitiesWithAB) {
            EXPECT_TRUE(currentTwoResults.find(entity) != currentTwoResults.end());
        }
    }
    
    // Property check: All update cycles should produce identical results
    for (int cycle = 1; cycle < numUpdateCycles; ++cycle) {
        // Compare single query results
        std::sort(singleQueryResults[0].begin(), singleQueryResults[0].end(), 
                  [](const Entity& a, const Entity& b) { return a.id < b.id; });
        std::sort(singleQueryResults[cycle].begin(), singleQueryResults[cycle].end(), 
                  [](const Entity& a, const Entity& b) { return a.id < b.id; });
        
        EXPECT_TRUE(singleQueryResults[0] == singleQueryResults[cycle]);
        
        // Compare two query results
        std::sort(twoQueryResults[0].begin(), twoQueryResults[0].end(), 
                  [](const Entity& a, const Entity& b) { return a.id < b.id; });
        std::sort(twoQueryResults[cycle].begin(), twoQueryResults[cycle].end(), 
                  [](const Entity& a, const Entity& b) { return a.id < b.id; });
        
        EXPECT_TRUE(twoQueryResults[0] == twoQueryResults[cycle]);
    }
    
    // Property check: Update counts should match number of cycles
    EXPECT_TRUE(singlePtr->updateCount.load() == numUpdateCycles);
    EXPECT_TRUE(twoPtr->updateCount.load() == numUpdateCycles);
    
    world->Shutdown();
}