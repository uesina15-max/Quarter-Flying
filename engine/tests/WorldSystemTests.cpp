#include <gtest/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../ecs/Components.h"
#include "../job/JobSystem.h"
#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <utility>

using namespace Engine;

namespace
{
    // ========================================
    // Test System Implementations
    // ========================================
    
    // Simple test system that counts updates
    class TestSystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
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
        
        const char* GetName() const override { return "TestSystem"; }
        int GetPriority() const override { return 0; }
    };
    
    // High priority test system
    class HighPrioritySystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        static inline std::vector<std::string> executionOrder;
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            executionOrder.push_back("HighPrioritySystem");
        }
        
        const char* GetName() const override { return "HighPrioritySystem"; }
        int GetPriority() const override { return -10; } // Higher priority (lower number)
    };
    
    // Low priority test system
    class LowPrioritySystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            HighPrioritySystem::executionOrder.push_back("LowPrioritySystem");
        }
        
        const char* GetName() const override { return "LowPrioritySystem"; }
        int GetPriority() const override { return 10; } // Lower priority (higher number)
    };
    
    // System that reads TransformComponent
    class TransformReaderSystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            // Read transform components
            auto allEntities = registry.GetAllEntities();
            for (Entity entity : allEntities)
            {
                if (registry.HasComponent<TransformComponent>(entity)) {
                    auto* transform = registry.GetComponent<TransformComponent>(entity);
                    (void)transform; // Suppress unused variable warning
                }
            }
        }
        
        const char* GetName() const override { return "TransformReaderSystem"; }
        
        std::vector<size_t> GetReadComponentTypes() const override
        {
            return { typeid(TransformComponent).hash_code() };
        }
    };
    
    // System that writes TransformComponent
    class TransformWriterSystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
            // Write transform components
            auto allEntities = registry.GetAllEntities();
            for (Entity entity : allEntities)
            {
                if (registry.HasComponent<TransformComponent>(entity)) {
                    auto* transform = registry.GetComponent<TransformComponent>(entity);
                    transform->position.x += 1.0f * deltaTime;
                }
            }
        }
        
        const char* GetName() const override { return "TransformWriterSystem"; }
        
        std::vector<size_t> GetWriteComponentTypes() const override
        {
            return { typeid(TransformComponent).hash_code() };
        }
    };
    
    // System that supports chunked processing
    class ChunkedProcessingSystem : public System
    {
    public:
        mutable std::atomic<int> updateCount{0};
        mutable std::atomic<int> chunkUpdateCount{0};
        mutable std::atomic<int> prepareCount{0};
        mutable std::atomic<int> finalizeCount{0};
        
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            updateCount++;
        }
        
        bool SupportsChunkedProcessing() const override { return true; }
        
        void UpdateChunk(ECSRegistry& registry, float deltaTime, 
                        size_t chunkStart, size_t chunkSize) override
        {
            chunkUpdateCount++;
        }
        
        void PrepareChunkedProcessing(ECSRegistry& registry, float deltaTime) override
        {
            prepareCount++;
        }
        
        void FinalizeChunkedProcessing(ECSRegistry& registry, float deltaTime) override
        {
            finalizeCount++;
        }
        
        size_t GetOptimalChunkSize() const override { return 100; }
        
        const char* GetName() const override { return "ChunkedProcessingSystem"; }
    };
    
} // anonymous namespace

// ========================================
// World System Test Fixture
// ========================================

class WorldSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        world = std::make_unique<World>();
        // Clear execution order for priority tests
        HighPrioritySystem::executionOrder.clear();
    }
    
    void TearDown() override
    {
        world.reset();
    }
    
    std::unique_ptr<World> world;
};

// ========================================
// Task 9.6: ECS System 단위 테스트 작성
// Requirements: 7.1, 7.2
// ========================================

// Test: Single System Registration and Update
TEST_F(WorldSystemTest, SingleSystemUpdate)
{
    // Register a single system
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    world->RegisterSystem(std::move(system));
    
    // Initialize world
    world->Initialize();
    
    // Verify system was initialized
    EXPECT_TRUE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    EXPECT_EQ(systemPtr->updateCount.load(), 0);
    
    // Update world
    float deltaTime = 0.016f;
    world->Update(deltaTime);
    
    // Verify system was updated
    EXPECT_EQ(systemPtr->updateCount.load(), 1);
    EXPECT_FLOAT_EQ(systemPtr->lastDeltaTime, deltaTime);
    
    // Shutdown world
    world->Shutdown();
    
    // Verify system was shut down
    EXPECT_TRUE(systemPtr->shutdownCalled);
}

// Test: Empty World Handling
TEST_F(WorldSystemTest, EmptyWorldHandling)
{
    // Initialize empty world
    world->Initialize();
    EXPECT_TRUE(world->IsInitialized());
    EXPECT_EQ(world->GetSystemCount(), 0);
    
    // Update empty world should not crash
    world->Update(0.016f);
    
    // Shutdown empty world should not crash
    world->Shutdown();
    EXPECT_FALSE(world->IsInitialized());
}

// Test: Multiple System Registration and Update Order
TEST_F(WorldSystemTest, MultipleSystemUpdateOrder)
{
    // Register systems with different priorities
    auto highPrioritySystem = std::make_unique<HighPrioritySystem>();
    auto lowPrioritySystem = std::make_unique<LowPrioritySystem>();
    
    HighPrioritySystem* highPtr = highPrioritySystem.get();
    LowPrioritySystem* lowPtr = lowPrioritySystem.get();
    
    // Register in reverse priority order to test sorting
    world->RegisterSystem(std::move(lowPrioritySystem));
    world->RegisterSystem(std::move(highPrioritySystem));
    
    world->Initialize();
    
    // Update world
    world->Update(0.016f);
    
    // Verify both systems were updated
    EXPECT_EQ(highPtr->updateCount.load(), 1);
    EXPECT_EQ(lowPtr->updateCount.load(), 1);
    
    // Verify execution order (high priority first)
    ASSERT_EQ(HighPrioritySystem::executionOrder.size(), 2);
    EXPECT_EQ(HighPrioritySystem::executionOrder[0], "HighPrioritySystem");
    EXPECT_EQ(HighPrioritySystem::executionOrder[1], "LowPrioritySystem");
}

// Test: System Lifecycle Management
TEST_F(WorldSystemTest, SystemLifecycleManagement)
{
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    
    // Register system before initialization
    world->RegisterSystem(std::move(system));
    
    // System should not be initialized yet
    EXPECT_FALSE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    
    // Initialize world
    world->Initialize();
    
    // System should be initialized
    EXPECT_TRUE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    
    // Update multiple times
    for (int i = 0; i < 5; ++i)
    {
        world->Update(0.016f);
    }
    
    EXPECT_EQ(systemPtr->updateCount.load(), 5);
    
    // Shutdown world
    world->Shutdown();
    
    // System should be shut down
    EXPECT_TRUE(systemPtr->shutdownCalled);
}

// Test: System Registration After World Initialization
TEST_F(WorldSystemTest, SystemRegistrationAfterInitialization)
{
    // Initialize empty world first
    world->Initialize();
    EXPECT_TRUE(world->IsInitialized());
    
    // Register system after initialization
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    world->RegisterSystem(std::move(system));
    
    // System should be initialized immediately
    EXPECT_TRUE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
    
    // Update should work
    world->Update(0.016f);
    EXPECT_EQ(systemPtr->updateCount.load(), 1);
}

// Test: World Update Without Initialization
TEST_F(WorldSystemTest, UpdateWithoutInitialization)
{
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    world->RegisterSystem(std::move(system));
    
    // Try to update without initialization
    world->Update(0.016f);
    
    // System should not be updated
    EXPECT_FALSE(systemPtr->initializeCalled);
    EXPECT_EQ(systemPtr->updateCount.load(), 0);
}

// Test: World Shutdown Without Initialization
TEST_F(WorldSystemTest, ShutdownWithoutInitialization)
{
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    world->RegisterSystem(std::move(system));
    
    // Try to shutdown without initialization (should not crash)
    world->Shutdown();
    
    // System should not be affected
    EXPECT_FALSE(systemPtr->initializeCalled);
    EXPECT_FALSE(systemPtr->shutdownCalled);
}

// Test: System Component Type Access
TEST_F(WorldSystemTest, SystemComponentTypeAccess)
{
    auto readerSystem = std::make_unique<TransformReaderSystem>();
    auto writerSystem = std::make_unique<TransformWriterSystem>();
    
    TransformReaderSystem* readerPtr = readerSystem.get();
    TransformWriterSystem* writerPtr = writerSystem.get();
    
    world->RegisterSystem(std::move(readerSystem));
    world->RegisterSystem(std::move(writerSystem));
    
    world->Initialize();
    
    // Create entities with transform components
    auto* registry = world->GetRegistry();
    Entity e1 = registry->CreateEntity();
    Entity e2 = registry->CreateEntity();
    
    TransformComponent transform1;
    transform1.position = Vec3(1.0f, 2.0f, 3.0f);
    TransformComponent transform2;
    transform2.position = Vec3(4.0f, 5.0f, 6.0f);
    
    registry->AddComponent(e1, transform1);
    registry->AddComponent(e2, transform2);
    
    // Update world
    world->Update(0.016f);
    
    // Verify systems processed components
    EXPECT_EQ(readerPtr->updateCount.load(), 1);
    EXPECT_EQ(writerPtr->updateCount.load(), 1);
    
    // Verify writer system modified components
    auto* modifiedTransform1 = registry->GetComponent<TransformComponent>(e1);
    auto* modifiedTransform2 = registry->GetComponent<TransformComponent>(e2);
    
    EXPECT_FLOAT_EQ(modifiedTransform1->position.x, 1.0f + 0.016f);
    EXPECT_FLOAT_EQ(modifiedTransform2->position.x, 4.0f + 0.016f);
}

// Test: System with No Entities
TEST_F(WorldSystemTest, SystemWithNoEntities)
{
    auto readerSystem = std::make_unique<TransformReaderSystem>();
    TransformReaderSystem* readerPtr = readerSystem.get();
    
    world->RegisterSystem(std::move(readerSystem));
    world->Initialize();
    
    // Update with no entities
    world->Update(0.016f);
    
    // System should still be called
    EXPECT_EQ(readerPtr->updateCount.load(), 1);
}

// Test: System Priority Sorting
TEST_F(WorldSystemTest, SystemPrioritySorting)
{
    // Create systems with various priorities
    class Priority5System : public System
    {
    public:
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            HighPrioritySystem::executionOrder.push_back("Priority5");
        }
        const char* GetName() const override { return "Priority5System"; }
        int GetPriority() const override { return 5; }
    };
    
    class Priority1System : public System
    {
    public:
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            HighPrioritySystem::executionOrder.push_back("Priority1");
        }
        const char* GetName() const override { return "Priority1System"; }
        int GetPriority() const override { return 1; }
    };
    
    class Priority3System : public System
    {
    public:
        void Update(ECSRegistry& registry, float deltaTime) override
        {
            HighPrioritySystem::executionOrder.push_back("Priority3");
        }
        const char* GetName() const override { return "Priority3System"; }
        int GetPriority() const override { return 3; }
    };
    
    // Register in random order
    world->RegisterSystem(std::make_unique<Priority5System>());
    world->RegisterSystem(std::make_unique<Priority1System>());
    world->RegisterSystem(std::make_unique<Priority3System>());
    
    world->Initialize();
    world->Update(0.016f);
    
    // Verify execution order (lower priority number = higher priority)
    ASSERT_EQ(HighPrioritySystem::executionOrder.size(), 3);
    EXPECT_EQ(HighPrioritySystem::executionOrder[0], "Priority1");
    EXPECT_EQ(HighPrioritySystem::executionOrder[1], "Priority3");
    EXPECT_EQ(HighPrioritySystem::executionOrder[2], "Priority5");
}

// Test: World State Management
TEST_F(WorldSystemTest, WorldStateManagement)
{
    // Initial state
    EXPECT_FALSE(world->IsInitialized());
    EXPECT_EQ(world->GetSystemCount(), 0);
    
    // Register system
    world->RegisterSystem(std::make_unique<TestSystem>());
    EXPECT_EQ(world->GetSystemCount(), 1);
    EXPECT_FALSE(world->IsInitialized());
    
    // Initialize
    world->Initialize();
    EXPECT_TRUE(world->IsInitialized());
    
    // Shutdown
    world->Shutdown();
    EXPECT_FALSE(world->IsInitialized());
    EXPECT_EQ(world->GetSystemCount(), 1); // Systems remain registered
}

// Test: Multiple Initialize/Shutdown Cycles
TEST_F(WorldSystemTest, MultipleInitializeShutdownCycles)
{
    auto system = std::make_unique<TestSystem>();
    TestSystem* systemPtr = system.get();
    world->RegisterSystem(std::move(system));
    
    // First cycle
    world->Initialize();
    EXPECT_TRUE(systemPtr->initializeCalled);
    world->Update(0.016f);
    EXPECT_EQ(systemPtr->updateCount.load(), 1);
    world->Shutdown();
    EXPECT_TRUE(systemPtr->shutdownCalled);
    
    // Reset flags for second cycle
    systemPtr->initializeCalled = false;
    systemPtr->shutdownCalled = false;
    systemPtr->updateCount = 0;
    
    // Second cycle
    world->Initialize();
    EXPECT_TRUE(systemPtr->initializeCalled);
    world->Update(0.016f);
    EXPECT_EQ(systemPtr->updateCount.load(), 1);
    world->Shutdown();
    EXPECT_TRUE(systemPtr->shutdownCalled);
}

// Test: System Chunked Processing Support
TEST_F(WorldSystemTest, SystemChunkedProcessingSupport)
{
    auto chunkedSystem = std::make_unique<ChunkedProcessingSystem>();
    ChunkedProcessingSystem* chunkedPtr = chunkedSystem.get();
    
    world->RegisterSystem(std::move(chunkedSystem));
    world->Initialize();
    
    // Create some entities to process
    auto* registry = world->GetRegistry();
    for (int i = 0; i < 10; ++i)
    {
        Entity e = registry->CreateEntity();
        TransformComponent transform;
        registry->AddComponent(e, transform);
    }
    
    // Update world (should use regular update since no job system is set)
    world->Update(0.016f);
    
    // Verify regular update was called
    EXPECT_EQ(chunkedPtr->updateCount.load(), 1);
    EXPECT_EQ(chunkedPtr->chunkUpdateCount.load(), 0);
    EXPECT_EQ(chunkedPtr->prepareCount.load(), 0);
    EXPECT_EQ(chunkedPtr->finalizeCount.load(), 0);
}
