// 프로젝트 상대 경로(core/ProjectPath.h) 유닛 테스트. docs/USABILITY_IMPROVEMENT_PLAN.md X-1a.

#include <gtest/gtest.h>
#include "../core/ProjectPath.h"
#include <filesystem>

using namespace Engine;

namespace
{
    // 실제로 있는 폴더를 기준으로 쓴다(weakly_canonical이 실제 대소문자로 맞추는 경로를 지나도록).
    std::string MakeRoot(const std::string& utf8Name)
    {
        const auto root = std::filesystem::temp_directory_path() / "qf_projectpath_tests" / PathFromUtf8(utf8Name);
        std::filesystem::create_directories(root / "assets" / "prefabs");
        return Utf8FromPath(root);
    }
}

TEST(ProjectPathTest, AbsoluteInsideProject_BecomesRelativeWithForwardSlashes)
{
    const std::string root = MakeRoot("한글 프로젝트");
    const std::string abs = Utf8FromPath(PathFromUtf8(root) / "assets" / "prefabs" / PathFromUtf8("통.prefab.json"));
    bool inside = false;
    EXPECT_EQ(NormalizeProjectPath(abs, root, &inside), "assets/prefabs/통.prefab.json");
    EXPECT_TRUE(inside);
}

TEST(ProjectPathTest, AbsoluteOutsideProject_IsKeptAndFlagged)
{
    const std::string root = MakeRoot("Proj");
    const std::string outside = Utf8FromPath(std::filesystem::temp_directory_path() / "elsewhere" / "x.prefab.json");
    bool inside = true;
    EXPECT_EQ(NormalizeProjectPath(outside, root, &inside), outside);
    EXPECT_FALSE(inside);
}

TEST(ProjectPathTest, SiblingFolderWithSamePrefix_IsOutside)
{
    // "Proj"와 "Proj2"는 문자열 접두사가 같지만 다른 폴더다.
    const std::string root = MakeRoot("Proj");
    const std::string sibling = Utf8FromPath(PathFromUtf8(MakeRoot("Proj2")) / "a.prefab.json");
    bool inside = true;
    EXPECT_EQ(NormalizeProjectPath(sibling, root, &inside), sibling);
    EXPECT_FALSE(inside);
}

TEST(ProjectPathTest, RelativePath_IsCleanedOnly)
{
    bool inside = false;
    EXPECT_EQ(NormalizeProjectPath("assets\\prefabs\\a.prefab.json", MakeRoot("Proj"), &inside), "assets/prefabs/a.prefab.json");
    EXPECT_TRUE(inside);
    EXPECT_EQ(NormalizeProjectPath("assets/./prefabs/../prefabs/a.prefab.json", "", nullptr), "assets/prefabs/a.prefab.json");
}

TEST(ProjectPathTest, EmptyRoot_LeavesAbsolutePathUnchanged)
{
    const std::string abs = Utf8FromPath(std::filesystem::temp_directory_path() / "x.prefab.json");
    EXPECT_EQ(NormalizeProjectPath(abs, ""), abs);
}

TEST(ProjectPathTest, Resolve_RelativeUsesRoot_AbsoluteIsKept)
{
    const std::string root = MakeRoot("한글 프로젝트");
    EXPECT_EQ(ResolveProjectPath("assets/prefabs/a.prefab.json", root),
              (PathFromUtf8(root) / "assets" / "prefabs" / "a.prefab.json").lexically_normal());
    const auto abs = std::filesystem::temp_directory_path() / "y.prefab.json";
    EXPECT_EQ(ResolveProjectPath(Utf8FromPath(abs), root), abs.lexically_normal());
}

TEST(ProjectPathTest, NormalizeThenResolve_RoundTrips)
{
    const std::string root = MakeRoot("Round 한글");
    const auto abs = (PathFromUtf8(root) / "assets" / "prefabs" / "b.prefab.json").lexically_normal();
    const std::string stored = NormalizeProjectPath(Utf8FromPath(abs), root);
    EXPECT_EQ(std::filesystem::weakly_canonical(ResolveProjectPath(stored, root)), std::filesystem::weakly_canonical(abs));
}
