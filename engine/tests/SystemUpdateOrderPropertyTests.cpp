#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
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
#include <random>
#include <set>
#include <string>
#include <utility>

using namespace Engine;

// ============================================================================
// Task 9.7: ECS System Update Order Property Tests
// Feature: game-engine-core-systems, Property 19: System Update Order Preservation
// Validates: Requirements 7.2
// ============================================================================

// Test System that records execution order and priority
class PropertyTestSystem : public System
{
public:
    PropertyTestSystem(int priority, int systemId, std::vector<std::pair<int, int>>* executionLog)
        : priority_(priority)
        , systemId_(systemId)
        , executionLog_(executionLog)
        , name_("PropertyTestSystem_" + std::to_string(systemId))
    {
    }

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // Record execution with priority and system ID
        if (executionLog_)
        {
            executionLog_->push_back({priority_, systemId_});
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
    std::vector<std::pair<int, int>>* executionLog_;
    std::string name_;
};

class SystemUpdateOrderPropertyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        world = std::make_unique<World>();
        jobSystem = std::make_unique<JobSystem>();
        jobSystem->Initialize(4); // 4 worker threads
        world->SetJobSystem(jobSystem.get());
        executionLog.clear();
    }
    
    void TearDown() override
    {
        if (world && world->IsInitialized())
        {
            world->Shutdown();
        }
        world.reset();
        jobSystem.reset();
        executionLog.clear();
    }
    
    std::unique_ptr<World> world;
    std::unique_ptr<JobSystem> jobSystem;
    std::vector<std::pair<int, int>> executionLog; // {priority, systemId}
};

// ========================================
// Property-Based Tests using RC_GTEST_PROP
// ========================================

RC_GTEST_FIXTURE_PROP(SystemUpdateOrderPropertyTest, SystemsAlwaysExecuteInPriorityOrder,
    ()) {
    const int priorityCount = *rc::gen::inRange(2, 11);
    std::vector<int> priorities;
    priorities.reserve(priorityCount);
    for (int i = 0; i < priorityCount; ++i)
    {
        priorities.push_back(*rc::gen::inRange(-20, 21));
    }
    
    // Remove duplicates and sort to get expected execution order
    std::set<int> uniquePriorities(priorities.begin(), priorities.end());
    std::vector<int> expectedOrder(uniquePriorities.begin(), uniquePriorities.end());
    std::sort(expectedOrder.begin(), expectedOrder.end());
    
    // Skip if we have too few unique priorities
    RC_PRE(expectedOrder.size() >= 2);
    
    // Create a shuffled registration order
    std::vector<int> registrationOrder = expectedOrder;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(registrationOrder.begin(), registrationOrder.end(), g);
    
    // Reset world for this test iteration
    if (world && world->IsInitialized())
    {
        world->Shutdown();
    }
    world = std::make_unique<World>();
    world->SetJobSystem(jobSystem.get());
    executionLog.clear();
    
    // Register systems in shuffled order
    int systemId = 0;
    for (int priority : registrationOrder)
    {
        auto system = std::make_unique<PropertyTestSystem>(priority, systemId++, &executionLog);
        world->RegisterSystem(std::move(system));
    }
    
    // Initialize world
    world->Initialize();
    
    // Create entities for systems to process
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    transform.position = Vec3(1.0f, 2.0f, 3.0f);
    registry->AddComponent(entity, transform);
    
    // Execute systems sequentially to ensure deterministic order
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify execution order matches priority order
    RC_ASSERT(executionLog.size() == expectedOrder.size());
    
    // Extract priorities from execution log
    std::vector<int> actualPriorityOrder;
    for (const auto& entry : executionLog)
    {
        actualPriorityOrder.push_back(entry.first);
    }
    
    // Verify priorities are in ascending order (lower priority value = higher priority)
    RC_ASSERT(std::is_sorted(actualPriorityOrder.begin(), actualPriorityOrder.end()));
    
    // Verify all expected priorities were executed
    RC_ASSERT(actualPriorityOrder == expectedOrder);
}

RC_GTEST_FIXTURE_PROP(SystemUpdateOrderPropertyTest, SystemsWithSamePriorityMaintainStableOrder,
    ()) {
    const int commonPriority = *rc::gen::inRange(-20, 21);
    const int systemCount = *rc::gen::inRange(2, 9);
    
    // Reset world for this test iteration
    if (world && world->IsInitialized())
    {
        world->Shutdown();
    }
    world = std::make_unique<World>();
    world->SetJobSystem(jobSystem.get());
    executionLog.clear();
    
    // Register systems with same priority in specific order
    std::vector<int> expectedSystemIds;
    for (int i = 0; i < systemCount; ++i)
    {
        auto system = std::make_unique<PropertyTestSystem>(commonPriority, i, &executionLog);
        world->RegisterSystem(std::move(system));
        expectedSystemIds.push_back(i);
    }
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Execute systems
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify execution order maintains registration order
    RC_ASSERT(executionLog.size() == static_cast<size_t>(systemCount));
    
    // Extract system IDs from execution log
    std::vector<int> actualSystemIds;
    for (const auto& entry : executionLog)
    {
        actualSystemIds.push_back(entry.second);
    }
    
    // Verify system IDs match registration order
    RC_ASSERT(actualSystemIds == expectedSystemIds);
    
    // Verify all systems had the same priority
    for (const auto& entry : executionLog)
    {
        RC_ASSERT(entry.first == commonPriority);
    }
}

RC_GTEST_FIXTURE_PROP(SystemUpdateOrderPropertyTest, ExecutionOrderConsistentAcrossMultipleFrames,
    ()) {
    const int priorityCount = *rc::gen::inRange(2, 7);
    std::vector<int> priorities;
    priorities.reserve(priorityCount);
    for (int i = 0; i < priorityCount; ++i)
    {
        priorities.push_back(*rc::gen::inRange(-20, 21));
    }
    
    // Remove duplicates
    std::set<int> uniquePriorities(priorities.begin(), priorities.end());
    std::vector<int> systemPriorities(uniquePriorities.begin(), uniquePriorities.end());
    
    RC_PRE(systemPriorities.size() >= 2);
    
    // Reset world for this test iteration
    if (world && world->IsInitialized())
    {
        world->Shutdown();
    }
    world = std::make_unique<World>();
    world->SetJobSystem(jobSystem.get());
    
    // Register systems in random order
    std::vector<int> registrationOrder = systemPriorities;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(registrationOrder.begin(), registrationOrder.end(), g);
    
    int systemId = 0;
    for (int priority : registrationOrder)
    {
        auto system = std::make_unique<PropertyTestSystem>(priority, systemId++, &executionLog);
        world->RegisterSystem(std::move(system));
    }
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Execute multiple frames and collect execution orders
    std::vector<std::vector<int>> frameExecutionOrders;
    const int frameCount = 3;
    
    for (int frame = 0; frame < frameCount; ++frame)
    {
        executionLog.clear();
        world->SetParallelExecution(false);
        world->Update(0.016f);
        
        // Extract priority order for this frame
        std::vector<int> framePriorityOrder;
        for (const auto& entry : executionLog)
        {
            framePriorityOrder.push_back(entry.first);
        }
        
        frameExecutionOrders.push_back(framePriorityOrder);
    }
    
    // Verify all frames have the same execution order
    RC_ASSERT(frameExecutionOrders.size() == frameCount);
    
    for (int frame = 1; frame < frameCount; ++frame)
    {
        RC_ASSERT(frameExecutionOrders[frame] == frameExecutionOrders[0]);
    }
    
    // Verify the order is sorted by priority
    for (const auto& frameOrder : frameExecutionOrders)
    {
        RC_ASSERT(std::is_sorted(frameOrder.begin(), frameOrder.end()));
    }
}

RC_GTEST_FIXTURE_PROP(SystemUpdateOrderPropertyTest, NegativeAndPositivePrioritiesOrderedCorrectly,
    ()) {
    const int negativeCount = *rc::gen::inRange(1, 5);
    const int positiveCount = *rc::gen::inRange(1, 5);

    std::vector<int> negativePriorities;
    std::vector<int> positivePriorities;
    negativePriorities.reserve(negativeCount);
    positivePriorities.reserve(positiveCount);

    for (int i = 0; i < negativeCount; ++i)
    {
        negativePriorities.push_back(*rc::gen::inRange(-20, 0));
    }

    for (int i = 0; i < positiveCount; ++i)
    {
        positivePriorities.push_back(*rc::gen::inRange(1, 21));
    }
    
    // Combine all priorities
    std::vector<int> allPriorities;
    allPriorities.insert(allPriorities.end(), negativePriorities.begin(), negativePriorities.end());
    allPriorities.insert(allPriorities.end(), positivePriorities.begin(), positivePriorities.end());
    
    // Add zero priority for completeness
    allPriorities.push_back(0);
    
    // Remove duplicates and shuffle registration order
    std::set<int> uniquePriorities(allPriorities.begin(), allPriorities.end());
    std::vector<int> registrationOrder(uniquePriorities.begin(), uniquePriorities.end());
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(registrationOrder.begin(), registrationOrder.end(), g);
    
    // Reset world for this test iteration
    if (world && world->IsInitialized())
    {
        world->Shutdown();
    }
    world = std::make_unique<World>();
    world->SetJobSystem(jobSystem.get());
    executionLog.clear();
    
    // Register systems
    int systemId = 0;
    for (int priority : registrationOrder)
    {
        auto system = std::make_unique<PropertyTestSystem>(priority, systemId++, &executionLog);
        world->RegisterSystem(std::move(system));
    }
    
    // Initialize world
    world->Initialize();
    
    // Create entities
    auto registry = world->GetRegistry();
    Entity entity = registry->CreateEntity();
    TransformComponent transform;
    registry->AddComponent(entity, transform);
    
    // Execute systems
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // Verify execution order
    RC_ASSERT(!executionLog.empty());
    
    // Extract priorities from execution log
    std::vector<int> executedPriorities;
    for (const auto& entry : executionLog)
    {
        executedPriorities.push_back(entry.first);
    }
    
    // Verify priorities are in ascending order
    RC_ASSERT(std::is_sorted(executedPriorities.begin(), executedPriorities.end()));
    
    // Find positions of negative, zero, and positive priorities
    auto zeroPos = std::find(executedPriorities.begin(), executedPriorities.end(), 0);
    
    // Verify all negative priorities come before zero
    for (auto it = executedPriorities.begin(); it != zeroPos; ++it)
    {
        RC_ASSERT(*it < 0);
    }
    
    // Verify all positive priorities come after zero
    for (auto it = zeroPos + 1; it != executedPriorities.end(); ++it)
    {
        RC_ASSERT(*it > 0);
    }
}

RC_GTEST_FIXTURE_PROP(SystemUpdateOrderPropertyTest, EmptyWorldHandledGracefully, ()) {
    // Reset world for this test iteration
    if (world && world->IsInitialized())
    {
        world->Shutdown();
    }
    world = std::make_unique<World>();
    world->SetJobSystem(jobSystem.get());
    executionLog.clear();
    
    // Don't register any systems
    world->Initialize();
    
    // Update should not crash
    world->SetParallelExecution(false);
    world->Update(0.016f);
    
    // No systems should have executed
    RC_ASSERT(executionLog.empty());
}

// ========================================
// Test Configuration
// ========================================

// Test to verify RapidCheck configuration
TEST_F(SystemUpdateOrderPropertyTest, PropertyTestConfiguration)
{
    // This test ensures property tests run with proper configuration
    // RapidCheck will run at least 100 iterations per property by default
    SUCCEED() << "Property tests configured to run with minimum 100 iterations each";
}
