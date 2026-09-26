#include <gtest/gtest.h>
#include "../animation/Skeleton.h"
#include "../animation/SkeletonSignature.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

using namespace Engine;

namespace
{
    Bone MakeBone(const std::string& name, int parentIndex, const glm::vec3& pos = { 0, 0, 0 },
                  const glm::quat& rot = { 1, 0, 0, 0 })
    {
        Bone bone;
        bone.name = name;
        bone.parentIndex = parentIndex;
        bone.bind = Transform(pos, rot, { 1, 1, 1 });
        return bone;
    }
}

class SkeletonTest : public ::testing::Test
{
};

// (a) 3단 계층의 local->world == 손 계산 값
TEST_F(SkeletonTest, LocalToWorld_ThreeLevelChain_MatchesHandComputedValue)
{
    // Hips(root) -> Spine(90도 Z축 회전, (0,1,0) 오프셋) -> Head((0,1,0) 오프셋, 회전 없음)
    glm::quat spineRot = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1)); // 90도 Z

    std::vector<Bone> bones = {
        MakeBone("Hips", -1, { 0, 0, 0 }),
        MakeBone("Spine", 0, { 0, 1, 0 }, spineRot),
        MakeBone("Head", 1, { 0, 1, 0 }),
    };

    auto skeleton = Skeleton::Create(bones);
    ASSERT_TRUE(skeleton.has_value());

    auto world = skeleton->ComputeBindWorldTransforms();
    ASSERT_EQ(world.size(), 3u);

    // 손 계산: Hips.world = (0,0,0)
    EXPECT_NEAR(world[0].position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(world[0].position.y, 0.0f, 1e-4f);
    EXPECT_NEAR(world[0].position.z, 0.0f, 1e-4f);

    // Spine.world = Hips.world + identity*(0,1,0) = (0,1,0)
    EXPECT_NEAR(world[1].position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(world[1].position.y, 1.0f, 1e-4f);
    EXPECT_NEAR(world[1].position.z, 0.0f, 1e-4f);

    // Head.world = Spine.world + Spine.world.rot*(0,1,0)
    //            = (0,1,0) + Rz(90)*(0,1,0) = (0,1,0) + (-1,0,0) = (-1,1,0)
    EXPECT_NEAR(world[2].position.x, -1.0f, 1e-4f);
    EXPECT_NEAR(world[2].position.y, 1.0f, 1e-4f);
    EXPECT_NEAR(world[2].position.z, 0.0f, 1e-4f);

    // Head.world.rotation == Spine.world.rotation (Head의 local rotation이 identity이므로)
    // 검증 방법: (1,0,0)을 Head.world.rotation으로 회전시키면 Rz(90)*(1,0,0) = (0,1,0)이어야 함.
    glm::vec3 rotated = world[2].rotation * glm::vec3(1, 0, 0);
    EXPECT_NEAR(rotated.x, 0.0f, 1e-4f);
    EXPECT_NEAR(rotated.y, 1.0f, 1e-4f);
    EXPECT_NEAR(rotated.z, 0.0f, 1e-4f);
}

// (b) 부모 없는 root 혼재(다중 루트)·사이클 -> expected(Error)
TEST_F(SkeletonTest, Create_MultipleRoots_ReturnsError)
{
    std::vector<Bone> bones = {
        MakeBone("RootA", -1),
        MakeBone("RootB", -1),
    };

    auto skeleton = Skeleton::Create(bones);
    ASSERT_FALSE(skeleton.has_value());
    EXPECT_EQ(skeleton.error(), SkeletonError::MultipleRootBones);
}

TEST_F(SkeletonTest, Create_CyclicHierarchy_ReturnsError)
{
    // Root(정상 루트) + A<->B 간접 사이클(둘 다 루트에 안 닿음)
    std::vector<Bone> bones = {
        MakeBone("Root", -1),
        MakeBone("A", 2),  // A의 부모는 B(인덱스 2)
        MakeBone("B", 1),  // B의 부모는 A(인덱스 1) -> 사이클
    };

    auto skeleton = Skeleton::Create(bones);
    ASSERT_FALSE(skeleton.has_value());
    EXPECT_EQ(skeleton.error(), SkeletonError::CyclicHierarchy);
}

TEST_F(SkeletonTest, Create_DuplicateBoneName_ReturnsError)
{
    std::vector<Bone> bones = {
        MakeBone("Hips", -1),
        MakeBone("Hips", 0),
    };

    auto skeleton = Skeleton::Create(bones);
    ASSERT_FALSE(skeleton.has_value());
    EXPECT_EQ(skeleton.error(), SkeletonError::DuplicateBoneName);
}

TEST_F(SkeletonTest, Create_InvalidParentIndex_ReturnsError)
{
    std::vector<Bone> bones = {
        MakeBone("Hips", -1),
        MakeBone("Spine", 99),  // 존재하지 않는 인덱스
    };

    auto skeleton = Skeleton::Create(bones);
    ASSERT_FALSE(skeleton.has_value());
    EXPECT_EQ(skeleton.error(), SkeletonError::InvalidParentIndex);
}

// (c) 형제 순서만 다른 두 skeleton은 동일 signature
TEST_F(SkeletonTest, Signature_SiblingOrderIndependent)
{
    // skeleton A: Hips -> [Spine -> [Arm, Leg]]  (Arm이 Leg보다 먼저 배열)
    std::vector<Bone> bonesA = {
        MakeBone("Hips", -1),
        MakeBone("Spine", 0),
        MakeBone("Arm", 1),
        MakeBone("Leg", 1),
    };

    // skeleton B: 같은 구조, Arm/Leg 배열 순서만 뒤바뀜(부모 인덱스는 새 배치에 맞게 재계산)
    std::vector<Bone> bonesB = {
        MakeBone("Hips", -1),
        MakeBone("Spine", 0),
        MakeBone("Leg", 1),
        MakeBone("Arm", 1),
    };

    auto skeletonA = Skeleton::Create(bonesA);
    auto skeletonB = Skeleton::Create(bonesB);
    ASSERT_TRUE(skeletonA.has_value());
    ASSERT_TRUE(skeletonB.has_value());

    EXPECT_EQ(skeletonA->GetSignature().hash, skeletonB->GetSignature().hash);
    EXPECT_EQ(skeletonA->GetSignature().canonicalPairs, skeletonB->GetSignature().canonicalPairs);
    EXPECT_EQ(IsCompatible(skeletonA->GetSignature(), skeletonB->GetSignature()), CompatibilityResult::Compatible);
}

// (d) axis/unit이 다른 두 skeleton은 구조가 같아도 정확 매칭 거부
TEST_F(SkeletonTest, Signature_DifferentUnit_RejectedAsMeta)
{
    std::vector<Bone> bonesMeter = {
        MakeBone("Hips", -1),
        MakeBone("Spine", 0),
    };
    std::vector<Bone> bonesCentimeter = bonesMeter; // 구조는 완전히 동일

    auto skeletonMeter = Skeleton::Create(bonesMeter, AxisSystem::Y_UP_RH, LengthUnit::Meter);
    auto skeletonCentimeter = Skeleton::Create(bonesCentimeter, AxisSystem::Y_UP_RH, LengthUnit::Centimeter);
    ASSERT_TRUE(skeletonMeter.has_value());
    ASSERT_TRUE(skeletonCentimeter.has_value());

    // 구조(hash/canonicalPairs)는 같아야 한다 - 여기서 갈리면 안 됨.
    EXPECT_EQ(skeletonMeter->GetSignature().hash, skeletonCentimeter->GetSignature().hash);
    EXPECT_EQ(skeletonMeter->GetSignature().canonicalPairs, skeletonCentimeter->GetSignature().canonicalPairs);

    // 그럼에도 unit이 다르므로 IsCompatible은 거부해야 한다.
    EXPECT_EQ(IsCompatible(skeletonMeter->GetSignature(), skeletonCentimeter->GetSignature()),
              CompatibilityResult::RejectMeta);
}

TEST_F(SkeletonTest, Signature_DifferentStructure_RejectedFast)
{
    std::vector<Bone> bonesA = {
        MakeBone("Hips", -1),
        MakeBone("Spine", 0),
    };
    std::vector<Bone> bonesB = {
        MakeBone("Hips", -1),
        MakeBone("Chest", 0),  // 이름이 다름 -> 구조가 다름
    };

    auto skeletonA = Skeleton::Create(bonesA);
    auto skeletonB = Skeleton::Create(bonesB);
    ASSERT_TRUE(skeletonA.has_value());
    ASSERT_TRUE(skeletonB.has_value());

    EXPECT_EQ(IsCompatible(skeletonA->GetSignature(), skeletonB->GetSignature()), CompatibilityResult::RejectFast);
}
