// Feature: game-engine-core-systems
// Unit tests for Result<T> – validates standard error handling contract

#include <gtest/gtest.h>
#include "core/EngineError.h"
#include <string>

using namespace Engine;

class ResultTest : public ::testing::Test
{
protected:
    Result<int> Divide(int a, int b)
    {
        if (b == 0)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, "Division by zero", "Math");
        }
        return a / b;
    }

    Result<void> PerformAction(bool fail)
    {
        if (fail)
        {
            return MakeUnexpected(EngineErrorCode::OperationFailed, "Action failed", "System");
        }
        return {};
    }
};

TEST_F(ResultTest, SuccessValueExtraction)
{
    auto res = Divide(10, 2);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 5);
}

TEST_F(ResultTest, FailureErrorExtraction)
{
    auto res = Divide(10, 0);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, EngineErrorCode::InvalidParameter);
    EXPECT_EQ(res.error().message, "Division by zero");
    EXPECT_EQ(res.error().component, "Math");
}

TEST_F(ResultTest, VoidResultSuccessAndFailure)
{
    auto successRes = PerformAction(false);
    EXPECT_TRUE(successRes.has_value());

    auto failRes = PerformAction(true);
    EXPECT_FALSE(failRes.has_value());
    EXPECT_EQ(failRes.error().code, EngineErrorCode::OperationFailed);
}
