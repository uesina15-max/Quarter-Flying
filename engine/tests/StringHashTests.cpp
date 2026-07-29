#include <gtest/gtest.h>
#include "../core/StringHash.h"
#include <unordered_map>
#include <string>

using namespace Engine;
using namespace Engine::Literals;

class StringHashTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringHashTest, CompileTimeEvaluation)
{
    constexpr StringHash hash1("TransformComponent");
    constexpr StringHash hash2("TransformComponent");
    constexpr StringHash hash3("RenderableComponent");

    EXPECT_NE(hash1.GetValue(), 0u);
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
}

TEST_F(StringHashTest, LiteralOperator)
{
    constexpr StringHash hash1 = "CameraComponent"_hash;
    StringHash hash2("CameraComponent");

    EXPECT_EQ(hash1, hash2);
    EXPECT_TRUE(hash1.IsValid());
}

TEST_F(StringHashTest, RuntimeStringCompatibility)
{
    std::string str = "ScriptComponent";
    StringHash runtimeHash(str);
    constexpr StringHash compileHash("ScriptComponent");

    EXPECT_EQ(runtimeHash, compileHash);
}

TEST_F(StringHashTest, EmptyAndNullHandling)
{
    StringHash defaultHash;
    StringHash nullHash(nullptr);
    StringHash emptyHash("");
    StringHash emptyStrHash(std::string(""));

    EXPECT_EQ(defaultHash.GetValue(), 0u);
    EXPECT_EQ(nullHash.GetValue(), 0u);
    EXPECT_EQ(emptyHash.GetValue(), 0u);
    EXPECT_EQ(emptyStrHash.GetValue(), 0u);
    EXPECT_FALSE(defaultHash.IsValid());
}

TEST_F(StringHashTest, UnorderedMapLookup)
{
    std::unordered_map<StringHash, int> map;
    map["TransformComponent"_hash] = 100;
    map["RenderableComponent"_hash] = 200;

    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map["TransformComponent"_hash], 100);
    EXPECT_EQ(map[StringHash("RenderableComponent")], 200);
    EXPECT_EQ(map.find("CameraComponent"_hash), map.end());
}
