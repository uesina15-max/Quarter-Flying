#include <gtest/gtest.h>
#include "../animation/LayerMixer.h"
#include "../animation/Skeleton.h"
#include <cmath>
#include <limits>
#include <optional>

using namespace Engine;

namespace
{
    Bone MakeBone(const std::string& name, int parentIndex, const glm::vec3& pos = { 0, 0, 0 })
    {
        Bone bone;
        bone.name = name;
        bone.parentIndex = parentIndex;
        bone.bind = Transform(pos, glm::quat(1, 0, 0, 0), { 1, 1, 1 });
        return bone;
    }

    // 테스트용 클립 - 실제 트랙 데이터는 필요 없다(MixLayers는 이미 샘플링된 LayerSpec::sampledPose만
    // 사용). signature만 skeleton과 맞춰준다.
    KeyframeClip MakeCompatibleClip(const Skeleton& skeleton)
    {
        KeyframeClip clip;
        clip.name = "TestClip";
        clip.signature = skeleton.GetSignature();
        return clip;
    }
}

class LayerMixerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Hips(root) -> Spine -> [Arm, Head], Hips -> Leg
        std::vector<Bone> bones = {
            MakeBone("Hips", -1),
            MakeBone("Spine", 0),
            MakeBone("Arm", 1),
            MakeBone("Head", 1),
            MakeBone("Leg", 0),
        };
        auto skeleton = Skeleton::Create(bones);
        ASSERT_TRUE(skeleton.has_value());
        skeleton_ = std::move(skeleton.value());
        clip_ = MakeCompatibleClip(*skeleton_);

        basePose_.resize(skeleton_->GetBoneCount());
        for (auto& t : basePose_)
        {
            t = Transform({ 0, 0, 0 }, glm::quat(1, 0, 0, 0), { 1, 1, 1 });
        }
    }

    Pose MakeFlatPose(float x) const
    {
        Pose pose(skeleton_->GetBoneCount());
        for (auto& t : pose)
        {
            t = Transform({ x, 0, 0 }, glm::quat(1, 0, 0, 0), { 1, 1, 1 });
        }
        return pose;
    }

    std::optional<Skeleton> skeleton_;
    KeyframeClip clip_;
    Pose basePose_;
};

// --- pairwise override: weight=1, mask=Full은 완전 교체 ---
TEST_F(LayerMixerTest, MixLayers_SingleFullWeightLayer_OverridesBaseCompletely)
{
    LayerSpec layer;
    layer.clip = &clip_;
    layer.sampledPose = MakeFlatPose(42.0f);
    layer.weight = 1.0f;
    layer.maskPreset = MaskPreset::Full;

    auto result = MixLayers(*skeleton_, basePose_, { layer });
    ASSERT_TRUE(result.has_value());
    for (const auto& t : *result)
    {
        EXPECT_NEAR(t.position.x, 42.0f, 1e-4f);
    }
}

// --- pairwise 누적: "각 레이어 N%씩 균등 배분"이 아니라 순서대로 lerp가 누적됨을 확인 ---
TEST_F(LayerMixerTest, MixLayers_TwoLayers_AccumulatePairwiseNotAveraged)
{
    LayerSpec layer0;
    layer0.clip = &clip_;
    layer0.sampledPose = MakeFlatPose(10.0f);
    layer0.weight = 0.5f;
    layer0.maskPreset = MaskPreset::Full;

    LayerSpec layer1;
    layer1.clip = &clip_;
    layer1.sampledPose = MakeFlatPose(100.0f);
    layer1.weight = 0.5f;
    layer1.maskPreset = MaskPreset::Full;

    auto result = MixLayers(*skeleton_, basePose_, { layer0, layer1 });
    ASSERT_TRUE(result.has_value());

    // base(0) -> lerp(0,10,0.5)=5 -> lerp(5,100,0.5)=52.5
    // (단순 평균이면 55가 나와야 하지만 pairwise 누적이므로 52.5가 맞다)
    for (const auto& t : *result)
    {
        EXPECT_NEAR(t.position.x, 52.5f, 1e-3f);
    }
}

// --- mask preset: UpperBody/LowerBody 접두사 매칭 ---
TEST_F(LayerMixerTest, MaskWeightForBone_UpperBodyPreset_MatchesExpectedBones)
{
    LayerSpec layer;
    layer.maskPreset = MaskPreset::UpperBody;

    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Spine"), *skeleton_), 1.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Arm"), *skeleton_), 1.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Head"), *skeleton_), 1.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Hips"), *skeleton_), 0.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Leg"), *skeleton_), 0.0f);
}

TEST_F(LayerMixerTest, MaskWeightForBone_LowerBodyPreset_MatchesExpectedBones)
{
    LayerSpec layer;
    layer.maskPreset = MaskPreset::LowerBody;

    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Hips"), *skeleton_), 1.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Leg"), *skeleton_), 1.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Spine"), *skeleton_), 0.0f);
    EXPECT_FLOAT_EQ(MaskWeightForBone(layer, skeleton_->FindBoneIndex("Arm"), *skeleton_), 0.0f);
}

TEST(MaskPresetTest, PrefixTablesDoNotOverlap)
{
    EXPECT_TRUE(AreMaskPresetTablesNonOverlapping());
}

// --- 위계 불일치 마스크: 부모-자식이 서로 다른 mask 값이어도 그대로 적용된다
//     (v3에서 폐기된 "부모-자식 동일 mask" 안전규칙이 실제로 없다는 걸 확인) ---
TEST_F(LayerMixerTest, MixLayers_CustomMask_HierarchyInconsistentMask_AppliesPerBoneIndependently)
{
    LayerSpec layer;
    layer.clip = &clip_;
    layer.sampledPose = MakeFlatPose(999.0f);
    layer.weight = 1.0f;
    layer.maskPreset = MaskPreset::Custom;
    // Arm(자식)만 마스크 1로 지정. 부모인 Spine은 지정 안 함(= 0, missing entry).
    layer.customPerBoneWeight[skeleton_->FindBoneIndex("Arm")] = 1.0f;

    auto result = MixLayers(*skeleton_, basePose_, { layer });
    ASSERT_TRUE(result.has_value());

    int armIdx = skeleton_->FindBoneIndex("Arm");
    int spineIdx = skeleton_->FindBoneIndex("Spine");

    // 자식(Arm)은 완전히 덮어써짐.
    EXPECT_NEAR((*result)[armIdx].position.x, 999.0f, 1e-4f);
    // 부모(Spine)는 mask=0이라 base 그대로 - "부모-자식 동일 mask" 규칙이 없다는 증거.
    EXPECT_NEAR((*result)[spineIdx].position.x, 0.0f, 1e-4f);
}

// --- 호환되지 않는 레이어: IncompatibleAtLayerIndex + 실패 인덱스 ---
TEST_F(LayerMixerTest, MixLayers_IncompatibleLayer_ReturnsErrorWithLayerIndex)
{
    std::vector<Bone> otherBones = { MakeBone("CompletelyDifferentRoot", -1) };
    auto otherSkeleton = Skeleton::Create(otherBones);
    ASSERT_TRUE(otherSkeleton.has_value());

    KeyframeClip incompatibleClip;
    incompatibleClip.signature = otherSkeleton->GetSignature();

    LayerSpec badLayer;
    badLayer.clip = &incompatibleClip;
    badLayer.sampledPose = MakeFlatPose(1.0f);  // 길이는 안 맞아도(1본) 호환성 검사에서 먼저 걸림
    badLayer.weight = 1.0f;
    badLayer.maskPreset = MaskPreset::Full;

    int failedIndex = -99;
    auto result = MixLayers(*skeleton_, basePose_, { badLayer }, &failedIndex);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MixerError::IncompatibleAtLayerIndex);
    EXPECT_EQ(failedIndex, 0);
}

// --- Three-layer: 세 번째 레이어에서 실패하면 인덱스 2를 정확히 가리켜야 한다 ---
TEST_F(LayerMixerTest, MixLayers_ThreeLayers_ThirdIncompatible_FailsAtIndexTwo)
{
    LayerSpec goodLayer0;
    goodLayer0.clip = &clip_;
    goodLayer0.sampledPose = MakeFlatPose(1.0f);
    goodLayer0.weight = 0.5f;

    LayerSpec goodLayer1;
    goodLayer1.clip = &clip_;
    goodLayer1.sampledPose = MakeFlatPose(2.0f);
    goodLayer1.weight = 0.5f;

    std::vector<Bone> otherBones = { MakeBone("Other", -1) };
    auto otherSkeleton = Skeleton::Create(otherBones);
    ASSERT_TRUE(otherSkeleton.has_value());
    KeyframeClip incompatibleClip;
    incompatibleClip.signature = otherSkeleton->GetSignature();

    LayerSpec badLayer2;
    badLayer2.clip = &incompatibleClip;
    badLayer2.sampledPose = MakeFlatPose(3.0f);
    badLayer2.weight = 1.0f;

    int failedIndex = -99;
    auto result = MixLayers(*skeleton_, basePose_, { goodLayer0, goodLayer1, badLayer2 }, &failedIndex);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MixerError::IncompatibleAtLayerIndex);
    EXPECT_EQ(failedIndex, 2);
}

// --- EmptyLayerClip ---
TEST_F(LayerMixerTest, MixLayers_NullClip_ReturnsEmptyLayerClipError)
{
    LayerSpec layer;
    layer.clip = nullptr;
    layer.sampledPose = MakeFlatPose(1.0f);

    int failedIndex = -99;
    auto result = MixLayers(*skeleton_, basePose_, { layer }, &failedIndex);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MixerError::EmptyLayerClip);
    EXPECT_EQ(failedIndex, 0);
}

// --- InvalidWeightRangeAfterClamp (NaN 방어) ---
TEST_F(LayerMixerTest, MixLayers_NaNWeight_ReturnsInvalidWeightError)
{
    LayerSpec layer;
    layer.clip = &clip_;
    layer.sampledPose = MakeFlatPose(1.0f);
    layer.weight = std::numeric_limits<float>::quiet_NaN();

    auto result = MixLayers(*skeleton_, basePose_, { layer });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MixerError::InvalidWeightRangeAfterClamp);
}

// --- 빈 레이어 목록은 base 그대로 반환 ---
TEST_F(LayerMixerTest, MixLayers_NoLayers_ReturnsBasePoseUnchanged)
{
    auto result = MixLayers(*skeleton_, basePose_, {});
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), basePose_.size());
    for (size_t i = 0; i < result->size(); ++i)
    {
        EXPECT_NEAR((*result)[i].position.x, basePose_[i].position.x, 1e-6f);
    }
}
