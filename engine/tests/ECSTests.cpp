#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"

using namespace Engine;

// Basic Entity Creation Test
TEST(ECSRegistry, CreateEntity)
{
    ECSRegistry registry;
    
    Entity e1 = registry.CreateEntity();
    Entity e2 = registry.CreateEntity();
    
    EXPECT_NE(e1.id, e2.id);
    EXPECT_GT(e1.id, 0u);
    EXPECT_GT(e2.id, 0u);
}

// Component Add/Get Test
TEST(ECSRegistry, AddAndGetComponent)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    TransformComponent transform;
    transform.position = Vec3(1.0f, 2.0f, 3.0f);
    transform.scale = Vec3(2.0f, 2.0f, 2.0f);
    
    registry.AddComponent(entity, transform);
    
    TransformComponent* retrieved = registry.GetComponent<TransformComponent>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->position.x, 1.0f);
    EXPECT_EQ(retrieved->position.y, 2.0f);
    EXPECT_EQ(retrieved->position.z, 3.0f);
}

// Component Remove Test
TEST(ECSRegistry, RemoveComponent)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    TransformComponent transform;
    registry.AddComponent(entity, transform);
    
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
    
    registry.RemoveComponent<TransformComponent>(entity);
    
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_EQ(registry.GetComponent<TransformComponent>(entity), nullptr);
}

// Entity Destroy Test
TEST(ECSRegistry, DestroyEntity)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    TransformComponent transform;
    RenderableComponent renderable;
    
    registry.AddComponent(entity, transform);
    registry.AddComponent(entity, renderable);
    
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(registry.HasComponent<RenderableComponent>(entity));
    
    registry.DestroyEntity(entity);
    
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_FALSE(registry.HasComponent<RenderableComponent>(entity));
}

// View Query Test - Single Component
TEST(ECSRegistry, ViewSingleComponent)
{
    ECSRegistry registry;
    
    Entity e1 = registry.CreateEntity();
    Entity e2 = registry.CreateEntity();
    Entity e3 = registry.CreateEntity();
    
    TransformComponent transform;
    registry.AddComponent(e1, transform);
    registry.AddComponent(e2, transform);
    // e3 has no transform
    
    auto allEntities = registry.GetAllEntities();
    std::vector<Entity> view;
    for (auto e : allEntities) {
        if (registry.HasComponent<TransformComponent>(e)) {
            view.push_back(e);
        }
    }
    
    EXPECT_EQ(view.size(), 2u);
    
    int count = 0;
    for (Entity entity : view)
    {
        EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
        count++;
    }
    EXPECT_EQ(count, 2);
}

// View Query Test - Multiple Components
TEST(ECSRegistry, ViewMultipleComponents)
{
    ECSRegistry registry;
    
    Entity e1 = registry.CreateEntity();
    Entity e2 = registry.CreateEntity();
    Entity e3 = registry.CreateEntity();
    
    TransformComponent transform;
    RenderableComponent renderable;
    
    // e1 has both
    registry.AddComponent(e1, transform);
    registry.AddComponent(e1, renderable);
    
    // e2 has only transform
    registry.AddComponent(e2, transform);
    
    // e3 has only renderable
    registry.AddComponent(e3, renderable);
    
    auto allEntities = registry.GetAllEntities();
    std::vector<Entity> view;
    for (auto e : allEntities) {
        if (registry.HasComponent<TransformComponent>(e) && registry.HasComponent<RenderableComponent>(e)) {
            view.push_back(e);
        }
    }
    
    EXPECT_EQ(view.size(), 1u);
    
    for (Entity entity : view)
    {
        EXPECT_EQ(entity.id, e1.id);
        EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
        EXPECT_TRUE(registry.HasComponent<RenderableComponent>(entity));
    }
}

// Component Memory Contiguity Test
TEST(ECSRegistry, ComponentMemoryContiguity)
{
    ECSRegistry registry;
    
    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i)
    {
        Entity e = registry.CreateEntity();
        TransformComponent transform;
        transform.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        registry.AddComponent(e, transform);
        entities.push_back(e);
    }
    
    auto* array = registry.GetComponentArray<TransformComponent>();
    ASSERT_NE(array, nullptr);
    EXPECT_EQ(array->Size(), 10u);
    
    // Verify components are in contiguous memory
    auto& denseArray = array->GetDenseArray();
    EXPECT_EQ(denseArray.size(), 10u);
    
    // Check that memory addresses are contiguous
    for (size_t i = 1; i < denseArray.size(); ++i)
    {
        const TransformComponent* prev = &denseArray[i - 1];
        const TransformComponent* curr = &denseArray[i];
        ptrdiff_t diff = reinterpret_cast<const char*>(curr) - reinterpret_cast<const char*>(prev);
        EXPECT_EQ(diff, sizeof(TransformComponent));
    }
}

// ============================================================================
// Task 8.5: Additional Unit Tests for Entity Creation/Destruction and Component Operations
// Requirements: 6.1, 6.2
// ============================================================================

// Test: Single Entity Creation and Destruction
TEST(ECSRegistry, SingleEntityLifecycle)
{
    ECSRegistry registry;
    
    // Create a single entity
    Entity entity = registry.CreateEntity();
    EXPECT_GT(entity.id, 0u);
    
    // Destroy the entity
    registry.DestroyEntity(entity);
    
    // Verify entity no longer has components
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(entity));
}

// Test: Multiple Entity Creation with Unique IDs
TEST(ECSRegistry, MultipleEntityUniqueIDs)
{
    ECSRegistry registry;
    
    const int numEntities = 100;
    std::vector<Entity> entities;
    std::set<EntityID> uniqueIDs;
    
    // Create multiple entities
    for (int i = 0; i < numEntities; ++i)
    {
        Entity e = registry.CreateEntity();
        entities.push_back(e);
        uniqueIDs.insert(e.id);
    }
    
    // Verify all IDs are unique
    EXPECT_EQ(uniqueIDs.size(), numEntities);
    
    // Verify all IDs are valid (non-zero)
    for (const Entity& e : entities)
    {
        EXPECT_GT(e.id, 0u);
    }
}

// Test: Component Add and Retrieve Specific Values
TEST(ECSRegistry, ComponentAddRetrieveSpecificValues)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    // Add component with specific values
    TransformComponent transform;
    transform.position = Vec3(10.5f, 20.3f, 30.7f);
    transform.rotation = Quaternion(0.0f, 0.707f, 0.0f, 0.707f);
    transform.scale = Vec3(2.5f, 3.5f, 4.5f);
    
    registry.AddComponent(entity, transform);
    
    // Retrieve and verify exact values
    TransformComponent* retrieved = registry.GetComponent<TransformComponent>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FLOAT_EQ(retrieved->position.x, 10.5f);
    EXPECT_FLOAT_EQ(retrieved->position.y, 20.3f);
    EXPECT_FLOAT_EQ(retrieved->position.z, 30.7f);
    EXPECT_FLOAT_EQ(retrieved->rotation.x, 0.0f);
    EXPECT_FLOAT_EQ(retrieved->rotation.y, 0.707f);
    EXPECT_FLOAT_EQ(retrieved->rotation.z, 0.0f);
    EXPECT_FLOAT_EQ(retrieved->rotation.w, 0.707f);
    EXPECT_FLOAT_EQ(retrieved->scale.x, 2.5f);
    EXPECT_FLOAT_EQ(retrieved->scale.y, 3.5f);
    EXPECT_FLOAT_EQ(retrieved->scale.z, 4.5f);
}

// Test: Component Remove and Verify Null Return
TEST(ECSRegistry, ComponentRemoveVerifyNull)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    // Add component
    RenderableComponent renderable;
    renderable.castShadows = true;
    registry.AddComponent(entity, renderable);
    
    // Verify component exists
    EXPECT_TRUE(registry.HasComponent<RenderableComponent>(entity));
    RenderableComponent* retrieved = registry.GetComponent<RenderableComponent>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(retrieved->castShadows);
    
    // Remove component
    registry.RemoveComponent<RenderableComponent>(entity);
    
    // Verify component no longer exists
    EXPECT_FALSE(registry.HasComponent<RenderableComponent>(entity));
    EXPECT_EQ(registry.GetComponent<RenderableComponent>(entity), nullptr);
}

// Test: Multiple Components on Single Entity
TEST(ECSRegistry, MultipleComponentsSingleEntity)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    // Add multiple components
    TransformComponent transform;
    transform.position = Vec3(1.0f, 2.0f, 3.0f);
    
    RenderableComponent renderable;
    renderable.castShadows = false;
    
    registry.AddComponent(entity, transform);
    registry.AddComponent(entity, renderable);
    
    // Verify both components exist
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(registry.HasComponent<RenderableComponent>(entity));
    
    // Retrieve and verify values
    TransformComponent* t = registry.GetComponent<TransformComponent>(entity);
    RenderableComponent* r = registry.GetComponent<RenderableComponent>(entity);
    
    ASSERT_NE(t, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 1.0f);
    EXPECT_FALSE(r->castShadows);
}

// Test: Entity Destruction Removes All Components
TEST(ECSRegistry, EntityDestructionRemovesAllComponents)
{
    ECSRegistry registry;
    Entity entity = registry.CreateEntity();
    
    // Add multiple components
    TransformComponent transform;
    RenderableComponent renderable;
    
    registry.AddComponent(entity, transform);
    registry.AddComponent(entity, renderable);
    
    // Verify components exist
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(registry.HasComponent<RenderableComponent>(entity));
    
    // Destroy entity
    registry.DestroyEntity(entity);
    
    // Verify all components are removed
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_FALSE(registry.HasComponent<RenderableComponent>(entity));
    EXPECT_EQ(registry.GetComponent<TransformComponent>(entity), nullptr);
    EXPECT_EQ(registry.GetComponent<RenderableComponent>(entity), nullptr);
}

// Test: Component Operations on Non-Existent Entity
TEST(ECSRegistry, ComponentOperationsNonExistentEntity)
{
    ECSRegistry registry;
    
    // Create an entity with a specific ID
    Entity entity;
    entity.id = 999999;  // Non-existent entity
    
    // Verify component operations return expected results
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(entity));
    EXPECT_EQ(registry.GetComponent<TransformComponent>(entity), nullptr);
}

// Test: Sequential Entity Creation After Destruction
TEST(ECSRegistry, SequentialCreationAfterDestruction)
{
    ECSRegistry registry;
    
    // Create and destroy entities
    Entity e1 = registry.CreateEntity();
    Entity e2 = registry.CreateEntity();
    
    registry.DestroyEntity(e1);
    
    // Create new entity after destruction
    Entity e3 = registry.CreateEntity();
    
    // Verify new entity has unique ID
    EXPECT_NE(e3.id, e1.id);
    EXPECT_NE(e3.id, e2.id);
    EXPECT_GT(e3.id, 0u);
}

// ============================================================================
// Task 8.6: Property-Based Test for Entity ID Uniqueness
// Feature: game-engine-core-systems, Property 14: Entity ID 고유성
// Validates: Requirements 6.1
// ============================================================================

#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <set>
#include <vector>

// Property 14: Entity ID Uniqueness
// For all sequences of entity creation, all created entities must have unique IDs
RC_GTEST_PROP(ECSRegistry, EntityIDUniqueness, ())
{
    // Generate a reasonable number of entities to create (1-1000)
    auto numEntities = *rc::gen::inRange<uint32_t>(1u, 1001u);
    
    ECSRegistry registry;
    std::set<EntityID> uniqueIDs;
    std::vector<Entity> entities;
    
    // Create entities and track their IDs
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        // Check that this ID hasn't been seen before
        RC_ASSERT(uniqueIDs.find(entity.id) == uniqueIDs.end());
        
        // Check that the ID is valid (non-zero)
        RC_ASSERT(entity.id > 0);
        
        // Add to set of unique IDs
        uniqueIDs.insert(entity.id);
    }
    
    // Verify that all IDs are unique (set size equals number of entities)
    RC_ASSERT(uniqueIDs.size() == numEntities);
    RC_ASSERT(entities.size() == numEntities);
}

// ============================================================================
// Task 8.7: Property-Based Test for Component Roundtrip
// Feature: game-engine-core-systems, Property 15: Component 라운드트립
// Validates: Requirements 6.2
// ============================================================================

// Property 15: Component Roundtrip
// For all Entity and Component, adding a component then retrieving it must return
// the same data, and after removal, retrieval must return null
RC_GTEST_PROP(ECSRegistry, ComponentRoundtrip, ())
{
    ECSRegistry registry;
    
    // Generate random component data for TransformComponent
    auto posX = *rc::gen::arbitrary<float>();
    auto posY = *rc::gen::arbitrary<float>();
    auto posZ = *rc::gen::arbitrary<float>();
    
    auto rotX = *rc::gen::arbitrary<float>();
    auto rotY = *rc::gen::arbitrary<float>();
    auto rotZ = *rc::gen::arbitrary<float>();
    auto rotW = *rc::gen::arbitrary<float>();
    
    auto scaleX = *rc::gen::arbitrary<float>();
    auto scaleY = *rc::gen::arbitrary<float>();
    auto scaleZ = *rc::gen::arbitrary<float>();
    
    // Create entity
    Entity entity = registry.CreateEntity();
    
    // Create component with random data
    TransformComponent originalTransform;
    originalTransform.position = Vec3(posX, posY, posZ);
    originalTransform.rotation = Quaternion(rotX, rotY, rotZ, rotW);
    originalTransform.scale = Vec3(scaleX, scaleY, scaleZ);
    
    // Add component
    registry.AddComponent(entity, originalTransform);
    
    // Retrieve component and verify data matches
    TransformComponent* retrieved = registry.GetComponent<TransformComponent>(entity);
    RC_ASSERT(retrieved != nullptr);
    RC_ASSERT(retrieved->position.x == originalTransform.position.x);
    RC_ASSERT(retrieved->position.y == originalTransform.position.y);
    RC_ASSERT(retrieved->position.z == originalTransform.position.z);
    RC_ASSERT(retrieved->rotation.x == originalTransform.rotation.x);
    RC_ASSERT(retrieved->rotation.y == originalTransform.rotation.y);
    RC_ASSERT(retrieved->rotation.z == originalTransform.rotation.z);
    RC_ASSERT(retrieved->rotation.w == originalTransform.rotation.w);
    RC_ASSERT(retrieved->scale.x == originalTransform.scale.x);
    RC_ASSERT(retrieved->scale.y == originalTransform.scale.y);
    RC_ASSERT(retrieved->scale.z == originalTransform.scale.z);
    
    // Verify HasComponent returns true
    RC_ASSERT(registry.HasComponent<TransformComponent>(entity));
    
    // Remove component
    registry.RemoveComponent<TransformComponent>(entity);
    
    // Verify retrieval returns null after removal
    TransformComponent* afterRemoval = registry.GetComponent<TransformComponent>(entity);
    RC_ASSERT(afterRemoval == nullptr);
    
    // Verify HasComponent returns false
    RC_ASSERT(!registry.HasComponent<TransformComponent>(entity));
}

// Property 15 (Extended): Component Roundtrip with RenderableComponent
// Test the same property with a different component type
RC_GTEST_PROP(ECSRegistry, ComponentRoundtripRenderable, ())
{
    ECSRegistry registry;
    
    // Generate random component data for RenderableComponent
    auto meshHandle = *rc::gen::arbitrary<uint32_t>();
    auto materialHandle = *rc::gen::arbitrary<uint32_t>();
    auto castShadows = *rc::gen::arbitrary<bool>();
    
    // Create entity
    Entity entity = registry.CreateEntity();
    
    // Create component with random data
    RenderableComponent originalRenderable;
    originalRenderable.meshHandle = meshHandle;
    originalRenderable.materialHandle = materialHandle;
    originalRenderable.castShadows = castShadows;
    
    // Add component
    registry.AddComponent(entity, originalRenderable);
    
    // Retrieve component and verify data matches
    RenderableComponent* retrieved = registry.GetComponent<RenderableComponent>(entity);
    RC_ASSERT(retrieved != nullptr);
    RC_ASSERT(retrieved->meshHandle == originalRenderable.meshHandle);
    RC_ASSERT(retrieved->materialHandle == originalRenderable.materialHandle);
    RC_ASSERT(retrieved->castShadows == originalRenderable.castShadows);
    
    // Verify HasComponent returns true
    RC_ASSERT(registry.HasComponent<RenderableComponent>(entity));
    
    // Remove component
    registry.RemoveComponent<RenderableComponent>(entity);
    
    // Verify retrieval returns null after removal
    RenderableComponent* afterRemoval = registry.GetComponent<RenderableComponent>(entity);
    RC_ASSERT(afterRemoval == nullptr);
    
    // Verify HasComponent returns false
    RC_ASSERT(!registry.HasComponent<RenderableComponent>(entity));
}

// Property 15 (Multiple Components): Component Roundtrip with Multiple Components
// Test that adding/removing one component doesn't affect others
RC_GTEST_PROP(ECSRegistry, ComponentRoundtripMultiple, ())
{
    ECSRegistry registry;
    
    // Generate random data for both components
    auto posX = *rc::gen::arbitrary<float>();
    auto posY = *rc::gen::arbitrary<float>();
    auto posZ = *rc::gen::arbitrary<float>();
    
    auto meshHandle = *rc::gen::arbitrary<uint32_t>();
    auto materialHandle = *rc::gen::arbitrary<uint32_t>();
    
    // Create entity
    Entity entity = registry.CreateEntity();
    
    // Create and add TransformComponent
    TransformComponent transform;
    transform.position = Vec3(posX, posY, posZ);
    registry.AddComponent(entity, transform);
    
    // Create and add RenderableComponent
    RenderableComponent renderable;
    renderable.meshHandle = meshHandle;
    renderable.materialHandle = materialHandle;
    registry.AddComponent(entity, renderable);
    
    // Verify both components exist and have correct data
    TransformComponent* t = registry.GetComponent<TransformComponent>(entity);
    RenderableComponent* r = registry.GetComponent<RenderableComponent>(entity);
    RC_ASSERT(t != nullptr);
    RC_ASSERT(r != nullptr);
    RC_ASSERT(t->position.x == posX);
    RC_ASSERT(t->position.y == posY);
    RC_ASSERT(t->position.z == posZ);
    RC_ASSERT(r->meshHandle == meshHandle);
    RC_ASSERT(r->materialHandle == materialHandle);
    
    // Remove TransformComponent
    registry.RemoveComponent<TransformComponent>(entity);
    
    // Verify TransformComponent is gone but RenderableComponent remains
    TransformComponent* tAfter = registry.GetComponent<TransformComponent>(entity);
    RenderableComponent* rAfter = registry.GetComponent<RenderableComponent>(entity);
    RC_ASSERT(tAfter == nullptr);
    RC_ASSERT(rAfter != nullptr);
    RC_ASSERT(rAfter->meshHandle == meshHandle);
    RC_ASSERT(rAfter->materialHandle == materialHandle);
    
    // Remove RenderableComponent
    registry.RemoveComponent<RenderableComponent>(entity);
    
    // Verify both components are gone
    RC_ASSERT(registry.GetComponent<TransformComponent>(entity) == nullptr);
    RC_ASSERT(registry.GetComponent<RenderableComponent>(entity) == nullptr);
}

// ============================================================================
// Task 8.8: Property-Based Test for Component Memory Contiguity
// Feature: game-engine-core-systems, Property 16: Component 메모리 연속성
// Validates: Requirements 6.3
// ============================================================================

// Property 16: Component Memory Contiguity
// For all component types, components of the same type must be stored in contiguous memory,
// and the pointer difference between consecutive components must be a multiple of sizeof(Component)
RC_GTEST_PROP(ECSRegistry, ComponentMemoryContiguityProperty, ())
{
    // Generate a reasonable number of entities to create (2-500)
    // We need at least 2 to check contiguity
    auto numEntities = *rc::gen::inRange<uint32_t>(2u, 501u);
    
    ECSRegistry registry;
    std::vector<Entity> entities;
    
    // Create entities and add TransformComponents with random data
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        // Generate random component data
        auto posX = *rc::gen::arbitrary<float>();
        auto posY = *rc::gen::arbitrary<float>();
        auto posZ = *rc::gen::arbitrary<float>();
        
        TransformComponent transform;
        transform.position = Vec3(posX, posY, posZ);
        registry.AddComponent(entity, transform);
    }
    
    // Get the component array
    auto* array = registry.GetComponentArray<TransformComponent>();
    RC_ASSERT(array != nullptr);
    RC_ASSERT(array->Size() == numEntities);
    
    // Get the dense array (contiguous memory)
    auto& denseArray = array->GetDenseArray();
    RC_ASSERT(denseArray.size() == numEntities);
    
    // Verify memory contiguity: check that consecutive components are exactly
    // sizeof(TransformComponent) bytes apart
    for (size_t i = 1; i < denseArray.size(); ++i)
    {
        const TransformComponent* prev = &denseArray[i - 1];
        const TransformComponent* curr = &denseArray[i];
        
        // Calculate byte difference between consecutive components
        ptrdiff_t byteDiff = reinterpret_cast<const char*>(curr) - reinterpret_cast<const char*>(prev);
        
        // The difference must be exactly sizeof(TransformComponent)
        RC_ASSERT(byteDiff == static_cast<ptrdiff_t>(sizeof(TransformComponent)));
    }
}

// Property 16 (Extended): Component Memory Contiguity with RenderableComponent
// Test the same property with a different component type
RC_GTEST_PROP(ECSRegistry, ComponentMemoryContiguityRenderable, ())
{
    // Generate a reasonable number of entities to create (2-500)
    auto numEntities = *rc::gen::inRange<uint32_t>(2u, 501u);
    
    ECSRegistry registry;
    std::vector<Entity> entities;
    
    // Create entities and add RenderableComponents with random data
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        // Generate random component data
        auto meshHandle = *rc::gen::arbitrary<uint32_t>();
        auto materialHandle = *rc::gen::arbitrary<uint32_t>();
        auto castShadows = *rc::gen::arbitrary<bool>();
        
        RenderableComponent renderable;
        renderable.meshHandle = meshHandle;
        renderable.materialHandle = materialHandle;
        renderable.castShadows = castShadows;
        registry.AddComponent(entity, renderable);
    }
    
    // Get the component array
    auto* array = registry.GetComponentArray<RenderableComponent>();
    RC_ASSERT(array != nullptr);
    RC_ASSERT(array->Size() == numEntities);
    
    // Get the dense array (contiguous memory)
    auto& denseArray = array->GetDenseArray();
    RC_ASSERT(denseArray.size() == numEntities);
    
    // Verify memory contiguity
    for (size_t i = 1; i < denseArray.size(); ++i)
    {
        const RenderableComponent* prev = &denseArray[i - 1];
        const RenderableComponent* curr = &denseArray[i];
        
        ptrdiff_t byteDiff = reinterpret_cast<const char*>(curr) - reinterpret_cast<const char*>(prev);
        RC_ASSERT(byteDiff == static_cast<ptrdiff_t>(sizeof(RenderableComponent)));
    }
}

// Property 16 (With Removal): Component Memory Contiguity After Removal
// Test that memory remains contiguous even after removing components
RC_GTEST_PROP(ECSRegistry, ComponentMemoryContiguityAfterRemoval, ())
{
    // Generate initial number of entities (10-100)
    auto numEntities = *rc::gen::inRange<uint32_t>(10u, 101u);
    
    // Generate number of entities to remove (1 to half of total)
    auto numToRemove = *rc::gen::inRange<uint32_t>(1u, numEntities / 2 + 1);
    
    ECSRegistry registry;
    std::vector<Entity> entities;
    
    // Create entities and add components
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        TransformComponent transform;
        transform.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        registry.AddComponent(entity, transform);
    }
    
    // Remove some components randomly
    for (uint32_t i = 0; i < numToRemove; ++i)
    {
        registry.RemoveComponent<TransformComponent>(entities[i]);
    }
    
    // Get the component array
    auto* array = registry.GetComponentArray<TransformComponent>();
    RC_ASSERT(array != nullptr);
    
    uint32_t expectedSize = numEntities - numToRemove;
    RC_ASSERT(array->Size() == expectedSize);
    
    // If we have at least 2 components remaining, check contiguity
    if (expectedSize >= 2)
    {
        auto& denseArray = array->GetDenseArray();
        RC_ASSERT(denseArray.size() == expectedSize);
        
        // Verify memory contiguity after removal
        for (size_t i = 1; i < denseArray.size(); ++i)
        {
            const TransformComponent* prev = &denseArray[i - 1];
            const TransformComponent* curr = &denseArray[i];
            
            ptrdiff_t byteDiff = reinterpret_cast<const char*>(curr) - reinterpret_cast<const char*>(prev);
            RC_ASSERT(byteDiff == static_cast<ptrdiff_t>(sizeof(TransformComponent)));
        }
    }
}

// ============================================================================
// Task 8.9: Property-Based Test for Entity Destruction Component Cleanup
// Feature: game-engine-core-systems, Property 17: Entity 파괴 시 Component 정리
// Validates: Requirements 6.4
// ============================================================================

// Property 17: Entity Destruction Component Cleanup
// For all entities, after entity destruction, all components of that entity must be removed,
// and retrieval of any component must return null
RC_GTEST_PROP(ECSRegistry, EntityDestructionComponentCleanup, ())
{
    ECSRegistry registry;
    
    // Generate random component data
    auto posX = *rc::gen::arbitrary<float>();
    auto posY = *rc::gen::arbitrary<float>();
    auto posZ = *rc::gen::arbitrary<float>();
    auto meshHandle = *rc::gen::arbitrary<uint32_t>();
    auto materialHandle = *rc::gen::arbitrary<uint32_t>();
    auto castShadows = *rc::gen::arbitrary<bool>();
    
    // Create entity
    Entity entity = registry.CreateEntity();
    
    // Add multiple components to the entity
    TransformComponent transform;
    transform.position = Vec3(posX, posY, posZ);
    registry.AddComponent(entity, transform);
    
    RenderableComponent renderable;
    renderable.meshHandle = meshHandle;
    renderable.materialHandle = materialHandle;
    renderable.castShadows = castShadows;
    registry.AddComponent(entity, renderable);
    
    // Verify components exist before destruction
    RC_ASSERT(registry.HasComponent<TransformComponent>(entity));
    RC_ASSERT(registry.HasComponent<RenderableComponent>(entity));
    RC_ASSERT(registry.GetComponent<TransformComponent>(entity) != nullptr);
    RC_ASSERT(registry.GetComponent<RenderableComponent>(entity) != nullptr);
    
    // Destroy the entity
    registry.DestroyEntity(entity);
    
    // Verify all components are removed after destruction
    RC_ASSERT(!registry.HasComponent<TransformComponent>(entity));
    RC_ASSERT(!registry.HasComponent<RenderableComponent>(entity));
    RC_ASSERT(registry.GetComponent<TransformComponent>(entity) == nullptr);
    RC_ASSERT(registry.GetComponent<RenderableComponent>(entity) == nullptr);
}

// Property 17 (Multiple Entities): Entity Destruction Component Cleanup with Multiple Entities
// Test that destroying one entity doesn't affect components of other entities
RC_GTEST_PROP(ECSRegistry, EntityDestructionComponentCleanupMultipleEntities, ())
{
    // Generate number of entities to create (2-50)
    auto numEntities = *rc::gen::inRange<uint32_t>(2u, 51u);
    
    // Generate which entity to destroy (0 to numEntities-1)
    auto entityToDestroy = *rc::gen::inRange<uint32_t>(0u, numEntities);
    
    ECSRegistry registry;
    std::vector<Entity> entities;
    
    // Create entities and add components with unique data
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        TransformComponent transform;
        transform.position = Vec3(static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3));
        registry.AddComponent(entity, transform);
        
        RenderableComponent renderable;
        renderable.meshHandle = i * 100;
        renderable.materialHandle = i * 200;
        renderable.castShadows = (i % 2 == 0);
        registry.AddComponent(entity, renderable);
    }
    
    // Verify all entities have components before destruction
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        RC_ASSERT(registry.HasComponent<TransformComponent>(entities[i]));
        RC_ASSERT(registry.HasComponent<RenderableComponent>(entities[i]));
    }
    
    // Destroy one entity
    Entity destroyedEntity = entities[entityToDestroy];
    registry.DestroyEntity(destroyedEntity);
    
    // Verify the destroyed entity has no components
    RC_ASSERT(!registry.HasComponent<TransformComponent>(destroyedEntity));
    RC_ASSERT(!registry.HasComponent<RenderableComponent>(destroyedEntity));
    RC_ASSERT(registry.GetComponent<TransformComponent>(destroyedEntity) == nullptr);
    RC_ASSERT(registry.GetComponent<RenderableComponent>(destroyedEntity) == nullptr);
    
    // Verify all other entities still have their components with correct data
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        if (i != entityToDestroy)
        {
            RC_ASSERT(registry.HasComponent<TransformComponent>(entities[i]));
            RC_ASSERT(registry.HasComponent<RenderableComponent>(entities[i]));
            
            TransformComponent* t = registry.GetComponent<TransformComponent>(entities[i]);
            RenderableComponent* r = registry.GetComponent<RenderableComponent>(entities[i]);
            
            RC_ASSERT(t != nullptr);
            RC_ASSERT(r != nullptr);
            
            // Verify data integrity
            RC_ASSERT(t->position.x == static_cast<float>(i));
            RC_ASSERT(t->position.y == static_cast<float>(i * 2));
            RC_ASSERT(t->position.z == static_cast<float>(i * 3));
            RC_ASSERT(r->meshHandle == i * 100);
            RC_ASSERT(r->materialHandle == i * 200);
            RC_ASSERT(r->castShadows == (i % 2 == 0));
        }
    }
}

// Property 17 (Sequential Destruction): Entity Destruction Component Cleanup with Sequential Destruction
// Test that destroying multiple entities in sequence properly cleans up all components
RC_GTEST_PROP(ECSRegistry, EntityDestructionComponentCleanupSequential, ())
{
    // Generate number of entities to create (5-100)
    auto numEntities = *rc::gen::inRange<uint32_t>(5u, 101u);
    
    // Generate number of entities to destroy (1 to all)
    auto numToDestroy = *rc::gen::inRange<uint32_t>(1u, numEntities + 1);
    
    ECSRegistry registry;
    std::vector<Entity> entities;
    
    // Create entities and add components
    for (uint32_t i = 0; i < numEntities; ++i)
    {
        Entity entity = registry.CreateEntity();
        entities.push_back(entity);
        
        TransformComponent transform;
        transform.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        registry.AddComponent(entity, transform);
        
        RenderableComponent renderable;
        renderable.meshHandle = i;
        registry.AddComponent(entity, renderable);
    }
    
    // Destroy entities sequentially
    for (uint32_t i = 0; i < numToDestroy; ++i)
    {
        registry.DestroyEntity(entities[i]);
    }
    
    // Verify destroyed entities have no components
    for (uint32_t i = 0; i < numToDestroy; ++i)
    {
        RC_ASSERT(!registry.HasComponent<TransformComponent>(entities[i]));
        RC_ASSERT(!registry.HasComponent<RenderableComponent>(entities[i]));
        RC_ASSERT(registry.GetComponent<TransformComponent>(entities[i]) == nullptr);
        RC_ASSERT(registry.GetComponent<RenderableComponent>(entities[i]) == nullptr);
    }
    
    // Verify remaining entities still have their components
    for (uint32_t i = numToDestroy; i < numEntities; ++i)
    {
        RC_ASSERT(registry.HasComponent<TransformComponent>(entities[i]));
        RC_ASSERT(registry.HasComponent<RenderableComponent>(entities[i]));
        
        TransformComponent* t = registry.GetComponent<TransformComponent>(entities[i]);
        RenderableComponent* r = registry.GetComponent<RenderableComponent>(entities[i]);
        
        RC_ASSERT(t != nullptr);
        RC_ASSERT(r != nullptr);
        RC_ASSERT(t->position.x == static_cast<float>(i));
        RC_ASSERT(r->meshHandle == i);
    }
}
