#include <gtest/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../ecs/Components.h"
#include <memory>
#include <vector>
#include <utility>

using namespace Engine;

namespace
{
    // Simple test system that counts updates
    class SimpleTestSystem : public System
    {
    public:
        mutable int updateCount = 0;
        mutable float lastDeltaTime = 0.0f;
        mutable bool initializeCalled = false;
        mutable bool shutdownCalled = false;
        
        void Initialize(ECSRegistry& registry) override
        {
            initializeCalled = true;
        }
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            lastDeltaTime = deltaTime;
        }
        
        void Shutdown(ECSRegistry& registry) override
        {
            shutdownCalled = true;
        }
        
        const char* GetName() const override { return "SimpleTestSystem"; }
        int GetPriority() const override { return 0; }
    };
    
} // anonymous namespace

// ========================================
// Task 9.6: ECS System 단위 테스트 작성
// Requirements: 7.1, 7.2
// ========================================

// Test: Single System Update
TEST(WorldSystemBasicTest, SingleSystemUpdate)
{
    World world;
    
    // Register a single system
    auto system = std::make_unique<SimpleTestSystem>();
    SimpleTestSystem* systemPtr = system.get();
    world.RegisterSystem(std::move(system));
    
    // Initialize world
    world.Initialize();
    
    // Verify system was initialized
    EXPECT_TRUE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    EXPECT_EQ(systemPtr->updateCount, 0);
    
    // Update world
    float deltaTime = 0.016f;
    world.Update(deltaTime);
    
    // Verify system was updated
    EXPECT_EQ(systemPtr->updateCount, 1);
    EXPECT_FLOAT_EQ(systemPtr->lastDeltaTime, deltaTime);
    
    // Shutdown world
    world.Shutdown();
    
    // Verify system was shut down
    EXPECT_TRUE(systemPtr->shutdownCalled);
}

// Test: Empty World Handling
TEST(WorldSystemBasicTest, EmptyWorldHandling)
{
    World world;
    
    // Initialize empty world
    world.Initialize();
    EXPECT_TRUE(world.IsInitialized());
    EXPECT_EQ(world.GetSystemCount(), 0);
    
    // Update empty world should not crash
    world.Update(0.016f);
    
    // Shutdown empty world should not crash
    world.Shutdown();
    EXPECT_FALSE(world.IsInitialized());
}

// Test: World Update Without Initialization
TEST(WorldSystemBasicTest, UpdateWithoutInitialization)
{
    World world;
    
    auto system = std::make_unique<SimpleTestSystem>();
    SimpleTestSystem* systemPtr = system.get();
    world.RegisterSystem(std::move(system));
    
    // Try to update without initialization
    world.Update(0.016f);
    
    // System should not be updated
    EXPECT_FALSE(systemPtr->initializeCalled);
    EXPECT_EQ(systemPtr->updateCount, 0);
}

// Test: System Registration After World Initialization
TEST(WorldSystemBasicTest, SystemRegistrationAfterInitialization)
{
    World world;
    
    // Initialize empty world first
    world.Initialize();
    EXPECT_TRUE(world.IsInitialized());
    
    // Register system after initialization
    auto system = std::make_unique<SimpleTestSystem>();
    SimpleTestSystem* systemPtr = system.get();
    world.RegisterSystem(std::move(system));
    
    // System should be initialized immediately
    EXPECT_TRUE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    
    // Update should work
    world.Update(0.016f);
    EXPECT_EQ(systemPtr->updateCount, 1);
}

// Test: World State Management
TEST(WorldSystemBasicTest, WorldStateManagement)
{
    World world;
    
    // Initial state
    EXPECT_FALSE(world.IsInitialized());
    EXPECT_EQ(world.GetSystemCount(), 0);
    
    // Register system
    world.RegisterSystem(std::make_unique<SimpleTestSystem>());
    EXPECT_EQ(world.GetSystemCount(), 1);
    EXPECT_FALSE(world.IsInitialized());
    
    // Initialize
    world.Initialize();
    EXPECT_TRUE(world.IsInitialized());
    
    // Shutdown
    world.Shutdown();
    EXPECT_FALSE(world.IsInitialized());
    EXPECT_EQ(world.GetSystemCount(), 1); // Systems remain registered
}