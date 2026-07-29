#include <gtest/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../ecs/Components.h"
#include "../job/JobSystem.h"
#include <vector>
#include <algorithm>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <utility>

using namespace Engine;

// ============================================================================
// Task 9.7: System Update Order Preservation Tests (Simplified)
// Feature: game-engine-core-systems, Property 19: System Update Order Preservation
// Validates: Requirements 7.2
// ============================================================================

// Test System that records execution order
class OrderTrackingSystem : public System
{
public:
    OrderTrackingSystem(int priority, int systemId, std::vector<int>* executionOrder)
        : priority_(priority)
        , systemId_(systemId)
        , executionOrder_(executionOrder)
        , name_("OrderTrackingSystem_" + std::to_string(systemId))
    {
    }

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // Record execution order
        if (executionOrder_)
        {
            executionOrder_->push_back(systemId_);
        }
        
        // Small delay to make execution order more deterministic
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    const char* GetName() const override
    {
        return name_.c_str();
    }

    int GetPriority() const override
    {
        return priority_;
    }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        // Read TransformComponent to create some dependency
        return { typeid(TransformComponent).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        // No write dependencies for this test
        return {};
    }

private:
    int priority_;
    int systemId_;
    std::vector<int>* executionOrder_;
    std::string name_;
};

class SystemUpdateOrderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        world = std::make_unique<World>();
        jobSystem = std::make_unique<JobSystem>();
        jobSystem->Initialize(4); // 4 worker threads
        world->SetJobSystem(jobSystem.get());
        executionOrder.clear();
    }
    
    void TearDown() override
    {
        if (world && world->IsInitialized())
        {
            world->Shutdown();
        }
        world.reset();
        jobSystem.reset();
        executionOrder.clear();
    }
    
    std::unique_ptr<World> world;
    std::unique_ptr<JobSystem> jobSystem;
    std::vector<int> executionOrder;
};

// ========================================
// Basic System Order Tests
// ========================================

TEST_F(SystemUpdateOrderTest, SystemsExecuteInPriorityOrder)
{
    // Create systems with different priorities
    auto system1 = std::make_unique<OrderTrackingSystem>(10, 1, &executionOrder);
    auto system2 = std::make_unique<OrderTrackingSystem>(5, 2, &executionOrder);
    auto system3 = std::make_unique<OrderTrackingSystem>(15, 3, &executionOrder);
    
    // Register systems in random order
    world->RegisterSystem(std::move(system1));
    world->RegisterSystem(std::move(system3));
    world->RegisterSystem(std::move(system2));
    
    // Initialize world
    world->Initialize();
    
    // Create some entities with TransformComponent for systems to process
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    transform.position = Vec3(1.0f, 2.0f, 3.0f);
    registry->AddComponent(entity, transform);
    
    // Update world (sequential execution)
    world->SetParallelExecution(false);
    world->Update(0.016f); // 16ms frame time
    
    // Verify execution order matches priority order
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 2); // System 2 (priority 5)
    EXPECT_EQ(executionOrder[1], 1); // System 1 (priority 10)
    EXPECT_EQ(executionOrder[2], 3); // System 3 (priority 15)
}

TEST_F(SystemUpdateOrderTest, SystemsWithSamePriorityMaintainRegistrationOrder)
{
    // Create systems with same priority
    auto system1 = std::make_unique<OrderTrackingSystem>(10, 1, &executionOrder);
    auto system2 = std::make_unique<OrderTrackingSystem>(10, 2, &executionOrder);
    auto system3 = std::make_unique<OrderTrackingSystem>(10, 3, &executionOrder);
    
    // Register systems in specific order
    world->RegisterSystem(std::move(system1));
    world->RegisterSystem(std::move(system2));
    world->RegisterSystem(std::move(system3));
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Update world
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify execution order maintains registration order for same priority
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 1); // System 1 (registered first)
    EXPECT_EQ(executionOrder[1], 2); // System 2 (registered second)
    EXPECT_EQ(executionOrder[2], 3); // System 3 (registered third)
}

TEST_F(SystemUpdateOrderTest, NegativePrioritiesExecuteFirst)
{
    // Create systems with negative and positive priorities
    auto system1 = std::make_unique<OrderTrackingSystem>(10, 1, &executionOrder);
    auto system2 = std::make_unique<OrderTrackingSystem>(-5, 2, &executionOrder);
    auto system3 = std::make_unique<OrderTrackingSystem>(0, 3, &executionOrder);
    
    // Register systems
    world->RegisterSystem(std::move(system1));
    world->RegisterSystem(std::move(system2));
    world->RegisterSystem(std::move(system3));
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Update world
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify execution order: negative priority first, then 0, then positive
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 2); // System 2 (priority -5)
    EXPECT_EQ(executionOrder[1], 3); // System 3 (priority 0)
    EXPECT_EQ(executionOrder[2], 1); // System 1 (priority 10)
}

TEST_F(SystemUpdateOrderTest, ExecutionOrderConsistentAcrossMultipleFrames)
{
    // Create systems with different priorities
    auto system1 = std::make_unique<OrderTrackingSystem>(20, 1, &executionOrder);
    auto system2 = std::make_unique<OrderTrackingSystem>(10, 2, &executionOrder);
    auto system3 = std::make_unique<OrderTrackingSystem>(30, 3, &executionOrder);
    
    // Register systems
    world->RegisterSystem(std::move(system1));
    world->RegisterSystem(std::move(system2));
    world->RegisterSystem(std::move(system3));
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Update multiple frames and verify consistent order
    for (int frame = 0; frame < 3; ++frame)
    {
        executionOrder.clear();
        world->SetParallelExecution(false);
        world->Update(0.016f);
        
        // Verify execution order is consistent
        ASSERT_EQ(executionOrder.size(), 3u);
        EXPECT_EQ(executionOrder[0], 2); // System 2 (priority 10)
        EXPECT_EQ(executionOrder[1], 1); // System 1 (priority 20)
        EXPECT_EQ(executionOrder[2], 3); // System 3 (priority 30)
    }
}

TEST_F(SystemUpdateOrderTest, DynamicSystemRegistrationMaintainsOrder)
{
    // Start with initial systems
    auto system1 = std::make_unique<OrderTrackingSystem>(20, 1, &executionOrder);
    auto system2 = std::make_unique<OrderTrackingSystem>(10, 2, &executionOrder);
    
    world->RegisterSystem(std::move(system1));
    world->RegisterSystem(std::move(system2));
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Test initial execution order
    executionOrder.clear();
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 2); // System 2 (priority 10)
    EXPECT_EQ(executionOrder[1], 1); // System 1 (priority 20)
    
    // Dynamically register a system with priority 15
    auto system3 = std::make_unique<OrderTrackingSystem>(15, 3, &executionOrder);
    world->RegisterSystem(std::move(system3));
    
    // Test execution order after dynamic registration
    executionOrder.clear();
    world->Update(0.016f);
    
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 2); // System 2 (priority 10)
    EXPECT_EQ(executionOrder[1], 3); // System 3 (priority 15)
    EXPECT_EQ(executionOrder[2], 1); // System 1 (priority 20)
}

TEST_F(SystemUpdateOrderTest, EmptyWorldHandledGracefully)
{
    // Don't register any systems
    world->Initialize();
    
    // Update should not crash
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // No systems should have executed
    EXPECT_EQ(executionOrder.size(), 0u);
}

TEST_F(SystemUpdateOrderTest, SingleSystemExecutes)
{
    // Register only one system
    auto system1 = std::make_unique<OrderTrackingSystem>(10, 1, &executionOrder);
    world->RegisterSystem(std::move(system1));
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Update world
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify single system executed
    ASSERT_EQ(executionOrder.size(), 1u);
    EXPECT_EQ(executionOrder[0], 1);
}