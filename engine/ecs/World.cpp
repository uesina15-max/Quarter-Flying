#include "World.h"
#include "../job/JobSystem.h"
#include "../core/logging/Logger.h"
#include "../core/CommandManager.h"
#include "SystemDependencyAnalyzer.h"
#include "ParallelGroupBuilder.h"
#include "SystemDispatcher.h"
#include <algorithm>
#include <unordered_set>
#include <queue>
#include <vector>
#include <string>
#include <memory>
#include <utility>

namespace Engine
{
    // ========================================
    // Constructor / Destructor
    // ========================================

    World::World()
        : registry(std::make_unique<ECSRegistry>())
        , jobSystem(nullptr)
        , initialized(false)
        , parallelExecution(false)
    {
        Logger::Info("World created");
    }

    World::~World()
    {
        if (initialized)
        {
            Shutdown();
        }
        Logger::Info("World destroyed");
    }

    // ========================================
    // World Lifecycle
    // ========================================

    void World::Initialize()
    {
        if (initialized)
        {
            Logger::Warning("World::Initialize() called on already initialized World");
            return;
        }

        Logger::Info("Initializing World with {} systems", systems.size());

        // ECSRegistry는 이미 생성자에서 초기화됨
        
        // 모든 System 초기화 (등록 순서대로)
        for (const auto& system : systems)
        {
            Logger::Debug("Initializing system: {}", system->GetName());
            system->Initialize(*registry);
        }

        initialized = true;
        Logger::Info("World initialization completed");
    }

    void World::Update(float deltaTime)
    {
        if (!initialized)
        {
            Logger::Error("World::Update() called on uninitialized World");
            return;
        }

        if (m_EditorState == EditorState::Edit) {
            // Edit 모드 업데이트 구현 (UI 조작/렌더링 등)
            UpdateSystemsSequential(deltaTime);
            return;
        }

        if (m_EditorState == EditorState::Pause) {
            // Pause 모드
            return;
        }

        // Runtime Update
        if (parallelExecution && jobSystem != nullptr)
        {
            UpdateSystemsParallel(deltaTime);
        }
        else
        {
            UpdateSystemsSequential(deltaTime);
        }
    }

    void World::Play() {
        if (m_EditorState == EditorState::Edit) {
            m_Snapshot = SerializeRegistry(*registry);
            m_EditorState = EditorState::Play;
            
            for (const auto& system : systems) {
                system->OnStart(*registry);
            }
            
            Logger::Info("PIE: Play started (Snapshot saved)");
        } else if (m_EditorState == EditorState::Pause) {
            m_EditorState = EditorState::Play;
            Logger::Info("PIE: Resumed");
        }
    }

    void World::Stop() {
        if (m_EditorState != EditorState::Edit) {
            // 파괴 직전 런타임 시스템 데이터 정리 (스크립트 자원 해제 등)
            for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
                (*it)->OnStop(*registry);
            }

            // 런타임 오염 방지: 레지스트리 내용 초기화 (객체 재생성 아님)
            // registry 객체를 재생성하면 시스템들이 캐시한 포인터가 dangling pointer가 됨
            registry->Clear();

            // 스냅샷에서 레지스트리 상태 복원
            DeserializeRegistry(*registry, m_Snapshot);
            CommandManager::GetInstance().Clear();
            m_EditorState = EditorState::Edit;
            Logger::Info("PIE: Stop (Snapshot restored)");
        }
    }

    void World::Pause() {
        if (m_EditorState == EditorState::Play) {
            m_EditorState = EditorState::Pause;
            Logger::Info("PIE: Paused");
        }
    }

    void World::Shutdown()
    {
        if (!initialized)
        {
            Logger::Warning("World::Shutdown() called on uninitialized World");
            return;
        }

        Logger::Info("Shutting down World with {} systems", systems.size());

        // 모든 System 종료 (등록의 역순으로)
        for (auto it = systems.rbegin(); it != systems.rend(); ++it)
        {
            Logger::Debug("Shutting down system: {}", (*it)->GetName());
            (*it)->Shutdown(*registry);
        }

        initialized = false;
        Logger::Info("World shutdown completed");
    }

    // ========================================
    // System Management
    // ========================================

    void World::RegisterSystem(std::unique_ptr<System> system)
    {
        if (!system)
        {
            Logger::Error("World::RegisterSystem() called with null system");
            return;
        }

        Logger::Info("Registering system: {}", system->GetName());

        // System을 목록에 추가
        systems.push_back(std::move(system));

        // 우선순위에 따라 정렬
        SortSystemsByPriority();

        // System 병렬 그룹 갱신
        RebuildParallelGroups();

        // World가 이미 초기화되어 있다면 새 System도 초기화
        if (initialized && registry)
        {
            Logger::Debug("Initializing newly registered system: {}", systems.back()->GetName());
            systems.back()->Initialize(*registry);
        }
    }

    // ========================================
    // Internal Methods
    // ========================================

    void World::SortSystemsByPriority()
    {
        std::sort(systems.begin(), systems.end(),
            [](const std::unique_ptr<System>& a, const std::unique_ptr<System>& b) {
                return a->GetPriority() < b->GetPriority();
            });
    }

    void World::UpdateSystemsSequential(float deltaTime)
    {
        // 모든 System을 우선순위 순서로 순차 업데이트
        for (const auto& system : systems)
        {
            system->Update(*registry, deltaTime);
        }
    }

    void World::UpdateSystemsParallel(float deltaTime)
    {
        if (!jobSystem)
        {
            // Job System이 없으면 순차 실행으로 폴백
            Logger::Warning("Job System not available, falling back to sequential execution");
            UpdateSystemsSequential(deltaTime);
            return;
        }

        // System 의존성 그래프가 구축되지 않았으면 구축
        if (parallelGroups.empty() && !systems.empty())
        {
            RebuildParallelGroups();
        }

        SystemDispatcher::Dispatch(parallelGroups, jobSystem, *registry, deltaTime);
    }

    void World::RebuildParallelGroups()
    {
        // 1. 시스템 간의 의존성(Edges) 분석
        auto dependencies = SystemDependencyAnalyzer::Analyze(systems);

        // 2. 의존성을 기반으로 병렬 실행 그룹 구축 (위상 정렬)
        parallelGroups = ParallelGroupBuilder::Build(systems, dependencies);
    }

} // namespace Engine