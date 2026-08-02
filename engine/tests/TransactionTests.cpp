// Tests for Transaction – validates atomic apply/rollback and empty-check behavior
// Validates: Requirements 1.3, 1.4

#include <gtest/gtest.h>
#include "core/Transaction.h"
#include "core/EngineError.h"
#include <memory>
#include <vector>

using namespace Engine;

// ─── Helpers ───────────────────────────────────────────────────────────────

// A simple command that records apply/undo calls against external counters.
struct TrackingCommand : public ICommand
{
    std::string name_;
    int& applyCalls_;
    int& undoCalls_;
    bool failOnApply_;

    TrackingCommand(std::string name, int& applyCalls, int& undoCalls, bool failOnApply = false)
        : name_(std::move(name)), applyCalls_(applyCalls), undoCalls_(undoCalls), failOnApply_(failOnApply)
    {}

    std::expected<void, EngineError> Apply() override
    {
        if (failOnApply_)
            return MakeError(EngineErrorCode::InternalCommandFailure, "forced failure", "Test");
        ++applyCalls_;
        return {};
    }

    std::expected<void, EngineError> Undo() override
    {
        ++undoCalls_;
        return {};
    }

    std::string GetName() const override { return name_; }
};

// ─── Tests for Req 1.3: atomic rollback on partial failure ─────────────────

// When the 2nd command in a Transaction fails to Apply, the 1st command
// that already applied must be Undo-ed (rollback).
TEST(TransactionTests, SecondCommandFailure_RollsBackFirstCommand)
{
    int applyA = 0, undoA = 0;
    int applyB = 0, undoB = 0;

    Transaction tx("RollbackTest");
    tx.AddCommand(std::make_unique<TrackingCommand>("CmdA", applyA, undoA));
    tx.AddCommand(std::make_unique<TrackingCommand>("CmdB", applyB, undoB, /*failOnApply=*/true));

    auto result = tx.Apply();

    EXPECT_FALSE(result.has_value()) << "Transaction should fail when a sub-command fails";
    EXPECT_EQ(result.error().code, EngineErrorCode::InternalCommandFailure);

    // CmdA was applied, so it must be rolled back
    EXPECT_EQ(applyA, 1) << "CmdA should have been applied once before failure";
    EXPECT_EQ(undoA, 1) << "CmdA should be undone during rollback";

    // CmdB never succeeded, so it should never be undone
    EXPECT_EQ(applyB, 0) << "CmdB (failing) should not count as applied";
    EXPECT_EQ(undoB, 0) << "CmdB should not be undone (it never applied)";
}

// A successful Apply followed by Undo should undo all commands in reverse.
TEST(TransactionTests, SuccessfulApply_ThenUndo_UndoesAll)
{
    int applyA = 0, undoA = 0;
    int applyB = 0, undoB = 0;

    Transaction tx("UndoTest");
    tx.AddCommand(std::make_unique<TrackingCommand>("CmdA", applyA, undoA));
    tx.AddCommand(std::make_unique<TrackingCommand>("CmdB", applyB, undoB));

    ASSERT_TRUE(tx.Apply().has_value());
    ASSERT_TRUE(tx.Undo().has_value());

    EXPECT_EQ(undoA, 1);
    EXPECT_EQ(undoB, 1);
}

// ─── Tests for Req 1.4: Empty() ────────────────────────────────────────────

// A freshly constructed Transaction with no commands must be empty.
TEST(TransactionTests, EmptyTransaction_EmptyReturnsTrue)
{
    Transaction tx("EmptyTx");
    EXPECT_TRUE(tx.Empty());
    EXPECT_EQ(tx.Size(), 0u);
}

// Adding a command makes Empty() return false.
TEST(TransactionTests, NonEmptyTransaction_EmptyReturnsFalse)
{
    int a = 0, b = 0;
    Transaction tx("NonEmptyTx");
    tx.AddCommand(std::make_unique<TrackingCommand>("Cmd", a, b));
    EXPECT_FALSE(tx.Empty());
    EXPECT_EQ(tx.Size(), 1u);
}

// ─── Tests for GetName() ───────────────────────────────────────────────────

TEST(TransactionTests, GetName_EmptyTransaction_ReturnsEmptyString)
{
    Transaction tx("SomeName");
    EXPECT_EQ(tx.GetName(), "");
}

TEST(TransactionTests, GetName_MultipleCommands_ReturnsCommaSeparated)
{
    int a = 0, b = 0;
    Transaction tx("Multi");
    tx.AddCommand(std::make_unique<TrackingCommand>("Alpha", a, b));
    tx.AddCommand(std::make_unique<TrackingCommand>("Beta",  a, b));
    tx.AddCommand(std::make_unique<TrackingCommand>("Gamma", a, b));
    EXPECT_EQ(tx.GetName(), "Alpha,Beta,Gamma");
}

// ─── Additional edge cases ─────────────────────────────────────────────────

// Applying an empty Transaction should succeed (no-op).
TEST(TransactionTests, EmptyTransaction_ApplySucceeds)
{
    Transaction tx("EmptyApply");
    EXPECT_TRUE(tx.Apply().has_value());
}

// Applying an empty Transaction then undoing should succeed (no-op).
TEST(TransactionTests, EmptyTransaction_UndoSucceeds)
{
    Transaction tx("EmptyUndo");
    EXPECT_TRUE(tx.Apply().has_value());
    EXPECT_TRUE(tx.Undo().has_value());
}

// Error code from a failing sub-command is propagated through the Transaction.
TEST(TransactionTests, FailingSubCommand_ErrorCodePropagated)
{
    int a = 0, b = 0;
    Transaction tx("ErrorProp");
    tx.AddCommand(std::make_unique<TrackingCommand>("Fail", a, b, /*failOnApply=*/true));

    auto result = tx.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InternalCommandFailure);
}
