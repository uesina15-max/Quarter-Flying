#include "WorldManager.h"
#include "../core/logging/Logger.h"
#include <string>
#include <memory>
#include <utility>

namespace Engine
{
    WorldManager::WorldManager()
        : activeWorld(nullptr)
        , jobSystem(nullptr)
        , parallelExecution(false)
    {
        Logger::Log(LogLevel::Info, "WorldManager created");
    }

    WorldManager::~WorldManager()
    {
        Shutdown();
    }

    World* WorldManager::CreateWorld(const std::string& name)
    {
        if (worlds.count(name))
        {
            Logger::Log(LogLevel::Warning, "WorldManager: World '%s' already exists", name.c_str());
            return worlds[name].get();
        }

        auto world = std::make_unique<World>();

        if (jobSystem)
        {
            world->SetJobSystem(jobSystem);
            world->SetParallelExecution(parallelExecution);
        }

        World* ptr = world.get();
        worlds[name] = std::move(world);

        Logger::Log(LogLevel::Info, "WorldManager: Created world '%s'", name.c_str());
        return ptr;
    }

    World* WorldManager::GetWorld(const std::string& name)
    {
        auto it = worlds.find(name);
        return (it != worlds.end()) ? it->second.get() : nullptr;
    }

    void WorldManager::SetActiveWorld(World* world)
    {
        activeWorld = world;
        Logger::Log(LogLevel::Info, "WorldManager: Active world changed");
    }

    void WorldManager::DestroyWorld(const std::string& name)
    {
        auto it = worlds.find(name);
        if (it == worlds.end())
        {
            Logger::Log(LogLevel::Warning, "WorldManager: World '%s' not found", name.c_str());
            return;
        }

        if (activeWorld == it->second.get())
        {
            activeWorld = nullptr;
        }

        worlds.erase(it);
        Logger::Log(LogLevel::Info, "WorldManager: Destroyed world '%s'", name.c_str());
    }

    void WorldManager::Update(float deltaTime)
    {
        if (activeWorld)
        {
            activeWorld->Update(deltaTime);
        }
    }

    void WorldManager::Shutdown()
    {
        activeWorld = nullptr;

        for (auto& [name, world] : worlds)
        {
            if (world->IsInitialized())
            {
                world->Shutdown();
            }
        }

        worlds.clear();
        Logger::Log(LogLevel::Info, "WorldManager shutdown complete");
    }

} // namespace Engine
