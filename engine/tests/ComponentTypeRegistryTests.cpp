#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "../ecs/ComponentTypeRegistry.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include <set>
#include <vector>

using namespace Engine;

// ============================================================================
// Task 1.2: Property-Based Test for Component Type Registration Uniqueness
// Feature: ecs-system-refactoring, Property 1: Component Type Registration Uniqueness
// Validates: Requirements 1.1, 1.2
// ============================================================================

// Property 1: Component Type Registration Uniqueness
// For any sequence of component type registrations, each component type should 
// receive a unique ComponentTypeID, and subsequent registrations of the same 
// type should return the same ID.
RC_GTEST_PROP(ComponentTypeRegistry, ComponentTypeRegistrationUniqueness, ())
{
    ComponentTypeRegistry registry;
    
    // Generate a reasonable number of registration attempts (1-50)
    const auto numRegistrations = *rc::gen::inRange(1, 51);
    
    std::set<ComponentTypeID> uniqueIDs;
    std::vector<ComponentTypeID> transformIDs;
    std::vector<ComponentTypeID> renderableIDs;
    
    // Perform multiple registrations of the same types
    for (int i = 0; i < numRegistrations; ++i) {
        ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
        ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
        
        // Verify IDs are valid (non-zero)
        RC_ASSERT(transformID != INVALID_COMPONENT_TYPE_ID);
        RC_ASSERT(renderableID != INVALID_COMPONENT_TYPE_ID);
        RC_ASSERT(transformID > 0);
        RC_ASSERT(renderableID > 0);
        
        // Verify different types get different IDs
        RC_ASSERT(transformID != renderableID);
        
        // Store IDs for consistency checking
        transformIDs.push_back(transformID);
        renderableIDs.push_back(renderableID);
        
        // Add to unique ID set
        uniqueIDs.insert(transformID);
        uniqueIDs.insert(renderableID);
    }
    
    // Verify all registrations of the same type return the same ID
    for (size_t i = 1; i < transformIDs.size(); ++i) {
        RC_ASSERT(transformIDs[i] == transformIDs[0]);
    }
    for (size_t i = 1; i < renderableIDs.size(); ++i) {
        RC_ASSERT(renderableIDs[i] == renderableIDs[0]);
    }
    
    // Verify we only have 2 unique IDs (one for each component type)
    RC_ASSERT(uniqueIDs.size() == 2);
    
    // Verify GetComponentTypeID returns the same IDs
    RC_ASSERT(registry.GetComponentTypeID<TransformComponent>() == transformIDs[0]);
    RC_ASSERT(registry.GetComponentTypeID<RenderableComponent>() == renderableIDs[0]);
}

// ============================================================================
// Task 1.3: ComponentTypeRegistry Unit Tests
// Requirements: 1.1, 1.2, 1.3, 1.4, 1.5
// ============================================================================

// Test: Component Type Registration Returns Unique IDs
TEST(ComponentTypeRegistry, RegisterComponentTypeUniqueIDs)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    
    // Verify IDs are unique and valid
    EXPECT_NE(transformID, renderableID);
    EXPECT_NE(transformID, INVALID_COMPONENT_TYPE_ID);
    EXPECT_NE(renderableID, INVALID_COMPONENT_TYPE_ID);
    EXPECT_GT(transformID, 0u);
    EXPECT_GT(renderableID, 0u);
}

// Test: Same Component Type Returns Same ID (Requirement 1.2)
TEST(ComponentTypeRegistry, SameTypeReturnsSameID)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID id1 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id2 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id3 = registry.RegisterComponentType<TransformComponent>();
    
    // All registrations of the same type should return the same ID
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(id2, id3);
    EXPECT_EQ(id1, id3);
}

// Test: GetComponentTypeID Returns Correct ID
TEST(ComponentTypeRegistry, GetComponentTypeID)
{
    ComponentTypeRegistry registry;
    
    // Register component types
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    
    // Verify GetComponentTypeID returns the same IDs
    EXPECT_EQ(registry.GetComponentTypeID<TransformComponent>(), transformID);
    EXPECT_EQ(registry.GetComponentTypeID<RenderableComponent>(), renderableID);
}

// Test: GetComponentTypeID Returns Invalid for Unregistered Type
TEST(ComponentTypeRegistry, GetComponentTypeIDUnregistered)
{
    ComponentTypeRegistry registry;
    
    // Try to get ID for unregistered type
    ComponentTypeID id = registry.GetComponentTypeID<TransformComponent>();
    EXPECT_EQ(id, INVALID_COMPONENT_TYPE_ID);
}

// Test: Component Metadata Storage (Requirement 1.3)
TEST(ComponentTypeRegistry, ComponentMetadata)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    
    const ComponentMetadata& metadata = registry.GetMetadata(transformID);
    
    // Verify metadata is correct
    EXPECT_EQ(metadata.id, transformID);
    EXPECT_EQ(metadata.size, sizeof(TransformComponent));
    EXPECT_EQ(metadata.alignment, alignof(TransformComponent));
    EXPECT_NE(metadata.name, nullptr);
    EXPECT_NE(metadata.destructor, nullptr);
}

// Test: GetMetadata Returns Invalid for Invalid ID
TEST(ComponentTypeRegistry, GetMetadataInvalidID)
{
    ComponentTypeRegistry registry;
    
    const ComponentMetadata& metadata = registry.GetMetadata(INVALID_COMPONENT_TYPE_ID);
    EXPECT_EQ(metadata.id, INVALID_COMPONENT_TYPE_ID);
    
    const ComponentMetadata& metadata2 = registry.GetMetadata(999999);
    EXPECT_EQ(metadata2.id, INVALID_COMPONENT_TYPE_ID);
}

// Test: GetAllComponentTypes Returns Complete List (Requirement 1.4)
TEST(ComponentTypeRegistry, GetAllComponentTypes)
{
    ComponentTypeRegistry registry;
    
    // Initially should be empty
    auto allTypes = registry.GetAllComponentTypes();
    EXPECT_EQ(allTypes.size(), 0u);
    
    // Register some types
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    
    // Should now contain both types
    allTypes = registry.GetAllComponentTypes();
    EXPECT_EQ(allTypes.size(), 2u);
    
    // Verify the IDs are in the list
    bool foundTransform = false;
    bool foundRenderable = false;
    for (ComponentTypeID id : allTypes) {
        if (id == transformID) foundTransform = true;
        if (id == renderableID) foundRenderable = true;
    }
    EXPECT_TRUE(foundTransform);
    EXPECT_TRUE(foundRenderable);
}

// Test: GetRegisteredTypeCount
TEST(ComponentTypeRegistry, GetRegisteredTypeCount)
{
    ComponentTypeRegistry registry;
    
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 0u);
    
    registry.RegisterComponentType<TransformComponent>();
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 1u);
    
    registry.RegisterComponentType<RenderableComponent>();
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 2u);
    
    // Registering same type again shouldn't increase count
    registry.RegisterComponentType<TransformComponent>();
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 2u);
}

// Test: Component Destructor Function Works
TEST(ComponentTypeRegistry, ComponentDestructor)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    const ComponentMetadata& metadata = registry.GetMetadata(transformID);
    
    // Create a component instance
    TransformComponent* component = new TransformComponent();
    component->position = Vec3(1.0f, 2.0f, 3.0f);
    
    // Call destructor through metadata (should not crash)
    EXPECT_NO_THROW(metadata.destructor(component));
    
    // Note: We don't delete the memory here since the destructor only calls ~T()
    // In real usage, the memory would be managed by the memory pool
    delete component;
}

// Test: Multiple Component Types Have Different Metadata
TEST(ComponentTypeRegistry, MultipleComponentTypesMetadata)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    
    const ComponentMetadata& transformMeta = registry.GetMetadata(transformID);
    const ComponentMetadata& renderableMeta = registry.GetMetadata(renderableID);
    
    // Verify different types have different metadata
    EXPECT_NE(transformMeta.id, renderableMeta.id);
    EXPECT_EQ(transformMeta.size, sizeof(TransformComponent));
    EXPECT_EQ(renderableMeta.size, sizeof(RenderableComponent));
    EXPECT_EQ(transformMeta.alignment, alignof(TransformComponent));
    EXPECT_EQ(renderableMeta.alignment, alignof(RenderableComponent));
}

// Test: Error Handling for Edge Cases (Requirement 1.5)
TEST(ComponentTypeRegistry, ErrorHandling)
{
    ComponentTypeRegistry registry;
    
    // Test with invalid component type ID
    const ComponentMetadata& invalidMeta = registry.GetMetadata(0);
    EXPECT_EQ(invalidMeta.id, INVALID_COMPONENT_TYPE_ID);
    
    // Test with very large component type ID
    const ComponentMetadata& largeMeta = registry.GetMetadata(UINT32_MAX);
    EXPECT_EQ(largeMeta.id, INVALID_COMPONENT_TYPE_ID);
    
    // Test GetComponentTypeID for unregistered type
    ComponentTypeID unregisteredID = registry.GetComponentTypeID<TransformComponent>();
    EXPECT_EQ(unregisteredID, INVALID_COMPONENT_TYPE_ID);
}

// ============================================================================
// Task 1.3: Additional Unit Tests for Comprehensive Coverage
// ============================================================================

// Test: Duplicate Registration Handling - Extensive Testing (Requirement 1.2)
TEST(ComponentTypeRegistry, DuplicateRegistrationHandling)
{
    ComponentTypeRegistry registry;
    
    // Register the same type multiple times and verify same ID returned
    ComponentTypeID id1 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id2 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id3 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id4 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID id5 = registry.RegisterComponentType<TransformComponent>();
    
    // All should return the same ID
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(id2, id3);
    EXPECT_EQ(id3, id4);
    EXPECT_EQ(id4, id5);
    
    // Verify the ID is valid
    EXPECT_NE(id1, INVALID_COMPONENT_TYPE_ID);
    EXPECT_GT(id1, 0u);
    
    // Verify GetComponentTypeID also returns the same ID
    ComponentTypeID retrievedID = registry.GetComponentTypeID<TransformComponent>();
    EXPECT_EQ(retrievedID, id1);
    
    // Verify count doesn't increase with duplicate registrations
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 1u);
}

// Test: Metadata Access Completeness (Requirement 1.3)
TEST(ComponentTypeRegistry, MetadataAccessCompleteness)
{
    ComponentTypeRegistry registry;
    
    // Register multiple component types
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    
    // Test TransformComponent metadata
    const ComponentMetadata& transformMeta = registry.GetMetadata(transformID);
    EXPECT_EQ(transformMeta.id, transformID);
    EXPECT_EQ(transformMeta.typeIndex, std::type_index(typeid(TransformComponent)));
    EXPECT_EQ(transformMeta.size, sizeof(TransformComponent));
    EXPECT_EQ(transformMeta.alignment, alignof(TransformComponent));
    EXPECT_NE(transformMeta.name, nullptr);
    EXPECT_NE(transformMeta.destructor, nullptr);
    
    // Test RenderableComponent metadata
    const ComponentMetadata& renderableMeta = registry.GetMetadata(renderableID);
    EXPECT_EQ(renderableMeta.id, renderableID);
    EXPECT_EQ(renderableMeta.typeIndex, std::type_index(typeid(RenderableComponent)));
    EXPECT_EQ(renderableMeta.size, sizeof(RenderableComponent));
    EXPECT_EQ(renderableMeta.alignment, alignof(RenderableComponent));
    EXPECT_NE(renderableMeta.name, nullptr);
    EXPECT_NE(renderableMeta.destructor, nullptr);
    
    // Verify metadata is different for different types
    EXPECT_NE(transformMeta.id, renderableMeta.id);
    EXPECT_NE(transformMeta.typeIndex, renderableMeta.typeIndex);
    EXPECT_NE(transformMeta.size, renderableMeta.size);
}

// Test: Unregistered Type Error Handling (Requirement 1.5)
TEST(ComponentTypeRegistry, UnregisteredTypeErrorHandling)
{
    ComponentTypeRegistry registry;
    
    // Test GetComponentTypeID for completely unregistered type
    ComponentTypeID unregisteredID = registry.GetComponentTypeID<TransformComponent>();
    EXPECT_EQ(unregisteredID, INVALID_COMPONENT_TYPE_ID);
    
    // Test GetMetadata for unregistered type ID
    const ComponentMetadata& unregisteredMeta = registry.GetMetadata(999);
    EXPECT_EQ(unregisteredMeta.id, INVALID_COMPONENT_TYPE_ID);
    EXPECT_EQ(unregisteredMeta.size, 0u);
    EXPECT_EQ(unregisteredMeta.alignment, 0u);
    EXPECT_EQ(unregisteredMeta.name, nullptr);
    EXPECT_EQ(unregisteredMeta.destructor, nullptr);
    
    // Register one type, then test access to different unregistered type
    registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID stillUnregisteredID = registry.GetComponentTypeID<RenderableComponent>();
    EXPECT_EQ(stillUnregisteredID, INVALID_COMPONENT_TYPE_ID);
}

// Test: Boundary Conditions and Edge Cases
TEST(ComponentTypeRegistry, BoundaryConditions)
{
    ComponentTypeRegistry registry;
    
    // Test with ID 0 (invalid)
    const ComponentMetadata& zeroMeta = registry.GetMetadata(0);
    EXPECT_EQ(zeroMeta.id, INVALID_COMPONENT_TYPE_ID);
    
    // Test with ID 1 before any registration
    const ComponentMetadata& oneMeta = registry.GetMetadata(1);
    EXPECT_EQ(oneMeta.id, INVALID_COMPONENT_TYPE_ID);
    
    // Register a type and verify ID 1 is now valid
    ComponentTypeID firstID = registry.RegisterComponentType<TransformComponent>();
    EXPECT_EQ(firstID, 1u);
    
    const ComponentMetadata& validOneMeta = registry.GetMetadata(1);
    EXPECT_EQ(validOneMeta.id, 1u);
    
    // Test with ID just beyond registered range
    const ComponentMetadata& beyondMeta = registry.GetMetadata(2);
    EXPECT_EQ(beyondMeta.id, INVALID_COMPONENT_TYPE_ID);
    
    // Test with maximum possible ID
    const ComponentMetadata& maxMeta = registry.GetMetadata(UINT32_MAX);
    EXPECT_EQ(maxMeta.id, INVALID_COMPONENT_TYPE_ID);
}

// Test: Metadata Destructor Functionality
TEST(ComponentTypeRegistry, MetadataDestructorFunctionality)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    const ComponentMetadata& metadata = registry.GetMetadata(transformID);
    
    // Verify destructor function exists
    EXPECT_NE(metadata.destructor, nullptr);
    
    // Test destructor with valid component
    TransformComponent* component = new TransformComponent();
    component->position = Vec3(1.0f, 2.0f, 3.0f);
    
    // Call destructor - should not crash
    EXPECT_NO_THROW(metadata.destructor(component));
    delete component;
    
    // Test destructor with nullptr - should handle gracefully
    EXPECT_NO_THROW(metadata.destructor(nullptr));
}

// Test: Type Safety and Consistency
TEST(ComponentTypeRegistry, TypeSafetyAndConsistency)
{
    ComponentTypeRegistry registry;
    
    // Register types and get their IDs
    ComponentTypeID transformID1 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID1 = registry.RegisterComponentType<RenderableComponent>();
    
    // Register same types again
    ComponentTypeID transformID2 = registry.RegisterComponentType<TransformComponent>();
    ComponentTypeID renderableID2 = registry.RegisterComponentType<RenderableComponent>();
    
    // Verify consistency
    EXPECT_EQ(transformID1, transformID2);
    EXPECT_EQ(renderableID1, renderableID2);
    EXPECT_NE(transformID1, renderableID1);
    
    // Verify GetComponentTypeID consistency
    EXPECT_EQ(registry.GetComponentTypeID<TransformComponent>(), transformID1);
    EXPECT_EQ(registry.GetComponentTypeID<RenderableComponent>(), renderableID1);
    
    // Verify metadata consistency
    const ComponentMetadata& transformMeta1 = registry.GetMetadata(transformID1);
    const ComponentMetadata& transformMeta2 = registry.GetMetadata(transformID2);
    
    EXPECT_EQ(transformMeta1.id, transformMeta2.id);
    EXPECT_EQ(transformMeta1.size, transformMeta2.size);
    EXPECT_EQ(transformMeta1.alignment, transformMeta2.alignment);
    EXPECT_EQ(transformMeta1.typeIndex, transformMeta2.typeIndex);
}

// Test: Registry State After Multiple Operations
TEST(ComponentTypeRegistry, RegistryStateConsistency)
{
    ComponentTypeRegistry registry;
    
    // Initially empty
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 0u);
    EXPECT_EQ(registry.GetAllComponentTypes().size(), 0u);
    
    // Register first type
    ComponentTypeID transformID = registry.RegisterComponentType<TransformComponent>();
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 1u);
    EXPECT_EQ(registry.GetAllComponentTypes().size(), 1u);
    
    // Register second type
    ComponentTypeID renderableID = registry.RegisterComponentType<RenderableComponent>();
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 2u);
    EXPECT_EQ(registry.GetAllComponentTypes().size(), 2u);
    
    // Re-register first type (should not change counts)
    ComponentTypeID transformID2 = registry.RegisterComponentType<TransformComponent>();
    EXPECT_EQ(transformID, transformID2);
    EXPECT_EQ(registry.GetRegisteredTypeCount(), 2u);
    EXPECT_EQ(registry.GetAllComponentTypes().size(), 2u);
    
    // Verify all types are in the list
    auto allTypes = registry.GetAllComponentTypes();
    bool foundTransform = false, foundRenderable = false;
    for (ComponentTypeID id : allTypes) {
        if (id == transformID) foundTransform = true;
        if (id == renderableID) foundRenderable = true;
    }
    EXPECT_TRUE(foundTransform);
    EXPECT_TRUE(foundRenderable);
}

// Test: Read API Never Mutates Registry State (Stateless Read)
TEST(ComponentTypeRegistryTest, ReadAPINeverMutatesRegistryState)
{
    struct NeverRegisteredComponent { int dummy; };
    struct FooComponent { float value; };

    auto registry = std::make_unique<ECSRegistry>();
    Entity e = registry->CreateEntity();

    size_t beforeCount = registry->GetComponentTypeRegistry()->GetRegisteredTypeCount();

    // 1. Read API 호출 (미등록 컴포넌트 조회)
    auto* result = registry->try_get<NeverRegisteredComponent>(e);
    size_t afterCount = registry->GetComponentTypeRegistry()->GetRegisteredTypeCount();

    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(afterCount, beforeCount) << "Read API must never mutate Registry state on unregistered type!";

    // 2. 이미 등록된 타입 조회 케이스
    registry->AddComponent(e, FooComponent{42.0f});
    size_t beforeFoo = registry->GetComponentTypeRegistry()->GetRegisteredTypeCount();
    
    // Read API 호출 (기등록 컴포넌트 조회)
    auto* fooResult = registry->try_get<FooComponent>(e);
    size_t afterFoo = registry->GetComponentTypeRegistry()->GetRegisteredTypeCount();

    EXPECT_NE(fooResult, nullptr);
    EXPECT_EQ(fooResult->value, 42.0f);
    EXPECT_EQ(afterFoo, beforeFoo) << "Read API must never mutate Registry state on already registered type!";
}

// Test: Unified Hash/Name Lookups
TEST(ComponentTypeRegistryTest, UnifiedHashAndNameLookups)
{
    ComponentTypeRegistry registry;
    
    ComponentTypeID id1 = registry.RegisterComponentType<TransformComponent>();
    const ComponentMetadata& meta = registry.GetMetadata(id1);
    
    EXPECT_NE(meta.name, nullptr);
    EXPECT_NE(meta.typeHash, 0u);
    
    ComponentTypeID idByHash = registry.GetComponentTypeIDByHash(meta.typeHash);
    ComponentTypeID idByName = registry.GetComponentTypeIDByName(meta.name);
    
    EXPECT_EQ(idByHash, id1);
    EXPECT_EQ(idByName, id1);
    
    const ComponentMetadata& metaByHash = registry.GetMetadataByHash(meta.typeHash);
    const ComponentMetadata& metaByName = registry.GetMetadataByName(meta.name);
    
    EXPECT_EQ(metaByHash.id, id1);
    EXPECT_EQ(metaByName.id, id1);
    EXPECT_EQ(metaByHash.typeHash, meta.typeHash);
    EXPECT_STREQ(metaByName.name, meta.name);
}