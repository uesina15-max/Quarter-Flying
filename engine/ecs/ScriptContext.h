#pragma once

namespace Engine
{
    class ECSRegistry;

    // ========================================
    // 스크립트 실행 문맥 (docs/USABILITY_IMPROVEMENT_PLAN.md G1)
    // ========================================
    //
    // Python 스크립트(ScriptComponent)는 생성자에서 Entity 하나만 받는다. 자기 Transform을 읽고 옮기려면 그 엔티티가
    // 속한 레지스트리가 필요한데, 예전에는 스크립트가 레지스트리에 닿을 방법이 없었다(바인딩 어디에도 없음). 그래서
    // "on_update에서 캐릭터를 움직이는" 가장 기본적인 게임 스크립트도 쓸 수 없었다.
    //
    // ScriptSystem이 스크립트 훅(on_create/on_update/on_destroy)을 부르는 동안만 Current()가 그 레지스트리를
    // 가리키고, 그 밖에서는 nullptr이다(quarterflying.script.registry()가 이유를 적은 예외를 낸다).
    // quarterflying.input과 같은 "프레임 안에서만 유효" 규칙이다(InputActions::Current).
    // 값은 .cpp의 thread_local에 있다 - 헤더 inline 변수면 엔진 DLL과 바인딩 .pyd가 각자 사본을 가진다.
    class ScriptContext
    {
    public:
        static ECSRegistry* CurrentRegistry();

        class ScopedRegistry
        {
        public:
            explicit ScopedRegistry(ECSRegistry* registry);
            ~ScopedRegistry();
            ScopedRegistry(const ScopedRegistry&) = delete;
            ScopedRegistry& operator=(const ScopedRegistry&) = delete;
        private:
            ECSRegistry* previous_;
        };
    };
}
