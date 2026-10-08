#pragma once

#include "System.h"

namespace Engine
{
    struct ScriptComponent;

    class ScriptSystem : public System
    {
    public:
        // hotReload: 인스턴스화할 때마다 모듈을 importlib.reload로 다시 읽는다(에디터용).
        // 게임 실행 파일에서는 끈다(EngineConfig::scriptHotReload).
        explicit ScriptSystem(bool hotReload = true) : hotReload(hotReload) {}

        void Initialize(ECSRegistry& registry) override;
        void Shutdown(ECSRegistry& registry) override;
        void Update(ECSRegistry& registry, float deltaTime) override;
        
        void OnStart(ECSRegistry& registry);
        void OnStop(ECSRegistry& registry);

        const char* GetName() const override { return "ScriptSystem"; }

        // 스크립트는 게임 로직이라 Play 상태에서만 돈다(System.h의 RunsInEditMode 주석).
        bool RunsInEditMode() const override { return false; }
        // Python 호출은 GIL을 가진 스레드에서만 해야 한다. TickFrame을 부른 스레드(에디터의 Qt 메인
        // 스레드, Player의 main)가 GIL을 쥐고 있으므로 워커 스레드로 보내지 않는다.
        bool CanRunInParallel() const override { return false; }
        // 스크립트가 옮긴 Transform을 같은 프레임의 카메라 리그(-10)와 렌더(0)가 쓰도록 먼저 돈다.
        int GetPriority() const override { return -20; }

    private:
        bool hotReload = true;

        void ClearComponentCache(ScriptComponent& comp);
    };
}
