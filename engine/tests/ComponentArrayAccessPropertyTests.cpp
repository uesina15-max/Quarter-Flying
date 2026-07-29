#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/ComponentTypeRegistry.h"
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <memory>

using namespace Engine;

// ============================================================================
// Task 10: Component Array Access Safety Property Tests
// Feature: ecs-system-refactoring
// ============================================================================

// Test component types for property testing
struct PropertyTestComponentX { 
    int value; 
    PropertyTestComponentX(int v = 0) : value(v) {}
    bool operator==(const PropertyTestComponentX& other) const { return value == other.value; }
};

struct PropertyTestComponentY { 
    float value; 
    PropertyTestComponentY(float v = 0.0f) : value(v) {}
    bool operator==(const PropertyTestComponentY& other) const { return value == other.value; }
};

struct PropertyTestComponentZ { 
    char value; 
    PropertyTestComponentZ(char v = 'a') : value(v) {}
    bool operator==(const PropertyTestComponentZ& other) const { return value == other.value; }
};

class ComponentArrayAccessPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

RC_GTEST_FIXTURE_PROP(ComponentArrayAccessPropertyTest, ComponentAccessConsistencyProperty, ())
{
    auto registry = std::make_unique<ECSRegistry>();

    const auto numEntities = *rc::gen::inRange(10, 50);
    std::vector<Entity> entities;
    std::unordered_set<Entity> entitiesWithX;
    std::unordered_set<Entity> entitiesWithY;
    std::unordered_set<Entity> entitiesWithZ;

    for (int i = 0; i < numEntities; ++i) {
        Entity e = registry->CreateEntity();
        entities.push_back(e);

        if (*rc::gen::arbitrary<bool>()) {
            registry->AddComponent<PropertyTestComponentX>(e, PropertyTestComponentX(i));
            entitiesWithX.insert(e);
        }
        if (*rc::gen::arbitrary<bool>()) {
            registry->AddComponent<PropertyTestComponentY>(e, PropertyTestComponentY(static_cast<float>(i)));
            entitiesWithY.insert(e);
        }
        if (*rc::gen::arbitrary<bool>()) {
            registry->AddComponent<PropertyTestComponentZ>(e, PropertyTestComponentZ('a' + (i % 26)));
            entitiesWithZ.insert(e);
        }
    }

    for (const Entity& entity : entities) {
        bool hasX = entitiesWithX.find(entity) != entitiesWithX.end();
        bool registryHasX = registry->HasComponent<PropertyTestComponentX>(entity);
        PropertyTestComponentX* componentX = registry->GetComponent<PropertyTestComponentX>(entity);
        
        // Property check: HasComponent and GetComponent must be consistent
        RC_ASSERT(hasX == registryHasX);
        
        // Property check: GetComponent returns valid pointer iff entity has component
        if (hasX) {
            RC_ASSERT(componentX != nullptr);
        } else {
            RC_ASSERT(componentX == nullptr);
        }
        
        // Test PropertyTestComponentY
        bool hasY = entitiesWithY.find(entity) != entitiesWithY.end();
        bool registryHasY = registry->HasComponent<PropertyTestComponentY>(entity);
        PropertyTestComponentY* componentY = registry->GetComponent<PropertyTestComponentY>(entity);
        
        RC_ASSERT(hasY == registryHasY);
        
        if (hasY) {
            RC_ASSERT(componentY != nullptr);
        } else {
            RC_ASSERT(componentY == nullptr);
        }
        
        // Test PropertyTestComponentZ
        bool hasZ = entitiesWithZ.find(entity) != entitiesWithZ.end();
        bool registryHasZ = registry->HasComponent<PropertyTestComponentZ>(entity);
        PropertyTestComponentZ* componentZ = registry->GetComponent<PropertyTestComponentZ>(entity);
        
        RC_ASSERT(hasZ == registryHasZ);
        
        if (hasZ) {
            RC_ASSERT(componentZ != nullptr);
        } else {
            RC_ASSERT(componentZ == nullptr);
        }
    }
    
    // Step 3: Test component removal consistency
    for (const Entity& entity : entities) {
        // Randomly decide whether to remove components
        const auto componentType = *rc::gen::inRange(0, 3);
        
        if (componentType == 0 && entitiesWithX.find(entity) != entitiesWithX.end()) {
            // Remove component X
            registry->RemoveComponent<PropertyTestComponentX>(entity);
            entitiesWithX.erase(entity);
            
            // Verify component is no longer accessible
            bool hasCompX = registry->HasComponent<PropertyTestComponentX>(entity);
            auto* compX = registry->GetComponent<PropertyTestComponentX>(entity);
            RC_ASSERT(!hasCompX);
            RC_ASSERT(compX == nullptr);
            
        } else if (componentType == 1 && entitiesWithY.find(entity) != entitiesWithY.end()) {
            // Remove component Y
            registry->RemoveComponent<PropertyTestComponentY>(entity);
            entitiesWithY.erase(entity);
            
            // Verify component is no longer accessible
            bool hasCompY = registry->HasComponent<PropertyTestComponentY>(entity);
            auto* compY = registry->GetComponent<PropertyTestComponentY>(entity);
            RC_ASSERT(!hasCompY);
            RC_ASSERT(compY == nullptr);
            
        } else if (componentType == 2 && entitiesWithZ.find(entity) != entitiesWithZ.end()) {
            // Remove component Z
            registry->RemoveComponent<PropertyTestComponentZ>(entity);
            entitiesWithZ.erase(entity);
            
            // Verify component is no longer accessible
            bool hasCompZ = registry->HasComponent<PropertyTestComponentZ>(entity);
            auto* compZ = registry->GetComponent<PropertyTestComponentZ>(entity);
            RC_ASSERT(!hasCompZ);
            RC_ASSERT(compZ == nullptr);
        }
    }
    
    // Step 4: Final verification - all remaining components should still be accessible
    for (const Entity& entity : entities) {
        // Verify remaining components are still accessible
        if (entitiesWithX.find(entity) != entitiesWithX.end()) {
            bool hasCompX = registry->HasComponent<PropertyTestComponentX>(entity);
            auto* compX = registry->GetComponent<PropertyTestComponentX>(entity);
            RC_ASSERT(hasCompX);
            RC_ASSERT(compX != nullptr);
        }
        
        if (entitiesWithY.find(entity) != entitiesWithY.end()) {
            bool hasCompY = registry->HasComponent<PropertyTestComponentY>(entity);
            auto* compY = registry->GetComponent<PropertyTestComponentY>(entity);
            RC_ASSERT(hasCompY);
            RC_ASSERT(compY != nullptr);
        }
        
        if (entitiesWithZ.find(entity) != entitiesWithZ.end()) {
            bool hasCompZ = registry->HasComponent<PropertyTestComponentZ>(entity);
            auto* compZ = registry->GetComponent<PropertyTestComponentZ>(entity);
            RC_ASSERT(hasCompZ);
            RC_ASSERT(compZ != nullptr);
        }
    }
}
