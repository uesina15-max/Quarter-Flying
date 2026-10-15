// 스크립트 실행 문맥(ecs/ScriptContext.h) 유닛 테스트. docs/USABILITY_IMPROVEMENT_PLAN.md G1.

#include <gtest/gtest.h>
#include "../ecs/ScriptContext.h"
#include "../ecs/ECSRegistry.h"

using namespace Engine;

TEST(ScriptContextTest, RegistryIsOnlyAvailableInsideScope_AndNestedScopesRestore)
{
    EXPECT_EQ(ScriptContext::CurrentRegistry(), nullptr);   // 스크립트 밖
    ECSRegistry a;
    ECSRegistry b;
    {
        ScriptContext::ScopedRegistry outer(&a);
        EXPECT_EQ(ScriptContext::CurrentRegistry(), &a);
        {
            ScriptContext::ScopedRegistry inner(&b);           // 다른 World의 스크립트가 중첩 실행되는 경우
            EXPECT_EQ(ScriptContext::CurrentRegistry(), &b);
        }
        EXPECT_EQ(ScriptContext::CurrentRegistry(), &a);
    }
    EXPECT_EQ(ScriptContext::CurrentRegistry(), nullptr);
}
