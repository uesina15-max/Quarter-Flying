#include <gtest/gtest.h>
#include "../animation/MotionPreviewState.h"

using namespace Engine;

namespace
{
    const char* kSkeletonJson = R"JSON(
{
    "format": "core.skeleton", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
    "name": "TestRig",
    "bones": [
        { "name": "Hips",  "parent": -1, "bindPos": [0, 1.0, 0] },
        { "name": "Spine", "parent": 0,  "bindPos": [0, 0.5, 0] }
    ]
}
)JSON";

    const char* kIdleClipJson = R"JSON(
{
    "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
    "skeletonRef": "TestRig", "fps": 30, "totalFrames": 60,
    "tracks": [
        { "bone": "Spine", "keyframes": [ { "frame": 0, "pos": [0, 0.5, 0], "rot": [0,0,0,1] } ] }
    ]
}
)JSON";

    const char* kAttackClipJson = R"JSON(
{
    "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
    "skeletonRef": "TestRig", "fps": 30, "totalFrames": 60,
    "tracks": [
        { "bone": "Spine", "keyframes": [ { "frame": 30, "pos": [10, 0.5, 0], "rot": [0,0,0,1] } ] }
    ]
}
)JSON";
}

TEST(MotionPreviewStateTest, NoSkeleton_ComputeBoneWorldLines_ReturnsEmpty)
{
    MotionPreviewState state;
    EXPECT_TRUE(state.ComputeBoneWorldLines().empty());
}

TEST(MotionPreviewStateTest, LoadBaseClip_WithoutSkeleton_Fails)
{
    MotionPreviewState state;
    EXPECT_FALSE(state.LoadBaseClip(kIdleClipJson));
    EXPECT_FALSE(state.GetLastError().empty());
}

TEST(MotionPreviewStateTest, LoadSkeletonThenBaseClip_Succeeds)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    EXPECT_TRUE(state.HasSkeleton());
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));
    EXPECT_TRUE(state.HasBaseClip());
}

TEST(MotionPreviewStateTest, SkeletonOnly_NoBaseClip_ComputeBoneWorldLines_ReturnsEmpty)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    EXPECT_TRUE(state.ComputeBoneWorldLines().empty());
}

TEST(MotionPreviewStateTest, BaseClipOnly_ProducesOneLine)
{
    // Hips(root)->Spine 1개 관절 = 선분 1개 (루트는 부모가 없어서 선을 안 그림).
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));

    auto lines = state.ComputeBoneWorldLines();
    ASSERT_EQ(lines.size(), 1u);
    // Hips(0,1,0) -> Spine local(0,0.5,0) -> world(0,1.5,0)
    EXPECT_NEAR(lines[0].first.y, 1.0f, 1e-4f);
    EXPECT_NEAR(lines[0].second.y, 1.5f, 1e-4f);
    EXPECT_NEAR(lines[0].second.x, 0.0f, 1e-4f);
}

TEST(MotionPreviewStateTest, LayerWeightZero_MatchesBaseOnly)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));
    int layerId = state.AddLayer(kAttackClipJson);
    ASSERT_GE(layerId, 0);
    ASSERT_TRUE(state.SetLayerFrame(layerId, 30.0f));
    ASSERT_TRUE(state.SetLayerWeight(layerId, 0.0f));  // Idle 그대로여야 함

    auto lines = state.ComputeBoneWorldLines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NEAR(lines[0].second.x, 0.0f, 1e-4f);  // Attack의 x=10이 전혀 안 섞임
}

TEST(MotionPreviewStateTest, LayerWeightOne_MatchesLayerOnly)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));
    int layerId = state.AddLayer(kAttackClipJson);
    ASSERT_GE(layerId, 0);
    ASSERT_TRUE(state.SetLayerFrame(layerId, 30.0f));
    ASSERT_TRUE(state.SetLayerWeight(layerId, 1.0f));  // Attack으로 완전히 덮임

    auto lines = state.ComputeBoneWorldLines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NEAR(lines[0].second.x, 10.0f, 1e-4f);
}

TEST(MotionPreviewStateTest, LayerWeightHalf_IsBetweenBaseAndLayer)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));
    int layerId = state.AddLayer(kAttackClipJson);
    ASSERT_GE(layerId, 0);
    ASSERT_TRUE(state.SetLayerFrame(layerId, 30.0f));
    ASSERT_TRUE(state.SetLayerWeight(layerId, 0.5f));

    auto lines = state.ComputeBoneWorldLines();
    ASSERT_EQ(lines.size(), 1u);
    // lerp(0, 10, 0.5) = 5
    EXPECT_NEAR(lines[0].second.x, 5.0f, 1e-3f);
}

TEST(MotionPreviewStateTest, ReloadSkeleton_ClearsPreviousClipsAndLayers)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));
    int layerId = state.AddLayer(kAttackClipJson);
    ASSERT_GE(layerId, 0);
    EXPECT_TRUE(state.HasBaseClip());
    EXPECT_EQ(state.GetLayerCount(), 1);

    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));  // 다시 로드
    EXPECT_FALSE(state.HasBaseClip());  // 이전 클립은 비워져야 함
    EXPECT_EQ(state.GetLayerCount(), 0);  // 이전 레이어도 비워져야 함
}

TEST(MotionPreviewStateTest, RemoveLayer_ById_DoesNotDisturbOtherLayers)
{
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));

    int firstId = state.AddLayer(kAttackClipJson);
    int secondId = state.AddLayer(kAttackClipJson);
    ASSERT_GE(firstId, 0);
    ASSERT_GE(secondId, 0);
    ASSERT_NE(firstId, secondId);
    EXPECT_EQ(state.GetLayerCount(), 2);

    ASSERT_TRUE(state.RemoveLayer(firstId));
    EXPECT_EQ(state.GetLayerCount(), 1);
    // 남은 레이어(secondId)는 여전히 자신의 id로 조작 가능해야 한다 - index 재배치로
    // 인한 혼선이 없어야 함(레이어를 안정적인 id로 식별하는 이유).
    EXPECT_TRUE(state.SetLayerWeight(secondId, 0.25f));
    EXPECT_FALSE(state.SetLayerWeight(firstId, 0.25f));  // 이미 지운 id는 실패해야 함
}

TEST(MotionPreviewStateTest, CheckClipCompatible_TrueForMatchingSkeleton_FalseWithoutSkeleton)
{
    MotionPreviewState fresh;
    EXPECT_FALSE(fresh.CheckClipCompatible(kAttackClipJson));  // 스켈레톤 없음

    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    EXPECT_TRUE(state.CheckClipCompatible(kAttackClipJson));
    EXPECT_TRUE(state.CheckClipCompatible(kIdleClipJson));
    // 상태를 바꾸지 않는지 확인 - 검사 후에도 레이어/베이스클립은 그대로 없어야 함.
    EXPECT_EQ(state.GetLayerCount(), 0);
    EXPECT_FALSE(state.HasBaseClip());
}

TEST(MotionPreviewStateTest, TwoLayersStack_LastAddedWins)
{
    // Attack(Spine, x=10, weight=1)을 먼저 얹고 그 위에 Idle(Spine 트랙 없음 -> bind pose
    // x=0, weight=1)을 얹으면, "뒤에 오는 레이어일수록 우선순위가 높다"(순서 있는
    // override)는 LayerMixer 의미론대로 최종 결과는 두 번째 레이어(bind pose, x=0)여야
    // 한다 - LayerMixerTests의 pairwise override 검증을 MotionPreviewState 경로로 재확인.
    MotionPreviewState state;
    ASSERT_TRUE(state.LoadSkeleton(kSkeletonJson));
    ASSERT_TRUE(state.LoadBaseClip(kIdleClipJson));

    int firstId = state.AddLayer(kAttackClipJson);
    int secondId = state.AddLayer(kIdleClipJson);
    ASSERT_GE(firstId, 0);
    ASSERT_GE(secondId, 0);
    ASSERT_TRUE(state.SetLayerFrame(firstId, 30.0f));
    ASSERT_TRUE(state.SetLayerWeight(firstId, 1.0f));
    ASSERT_TRUE(state.SetLayerWeight(secondId, 1.0f));

    auto lines = state.ComputeBoneWorldLines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NEAR(lines[0].second.x, 0.0f, 1e-4f);
}
