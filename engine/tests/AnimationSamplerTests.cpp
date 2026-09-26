#include <gtest/gtest.h>
#include "../animation/AnimationPlayer.h"
#include "../animation/AnimationSampler.h"
#include "../animation/Skeleton.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
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

    KeyframeTrack::Key MakeKey(float frame, const glm::vec3& pos, const glm::quat& rot)
    {
        KeyframeTrack::Key key;
        key.frame = frame;
        key.position = pos;
        key.rotation = rot;
        return key;
    }
}

class AnimationSamplerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::vector<Bone> bones = {
            MakeBone("Hips", -1, { 0, 1, 0 }),
            MakeBone("Spine", 0, { 0, 0.5f, 0 }),
        };
        auto skeleton = Skeleton::Create(bones);
        ASSERT_TRUE(skeleton.has_value());
        skeleton_ = std::move(skeleton.value());
    }

    std::optional<Skeleton> skeleton_;
};

// --- Sample: 기본 보간 ---
TEST_F(AnimationSamplerTest, Sample_InterpolatesBetweenTwoKeys)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;

    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(0.0f, { 0, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(10.0f, { 10, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    Pose pose = Sample(*skeleton_, clip, 5.0f);  // 정확히 중간
    ASSERT_EQ(pose.size(), 2u);

    EXPECT_NEAR(pose[0].position.x, 5.0f, 1e-4f);  // Hips: lerp(0,10,0.5) = 5
    EXPECT_NEAR(pose[0].position.y, 0.0f, 1e-4f);
}

// --- Sample: 트랙 없는 본은 바인드 포즈 유지 (체크리스트 #13) ---
TEST_F(AnimationSamplerTest, Sample_MissingTrack_KeepsBindPose)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;
    // Hips 트랙만 있고 Spine 트랙은 없음.
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = { MakeKey(0.0f, { 5, 5, 5 }, glm::quat(1, 0, 0, 0)) };
    clip.tracks.push_back(track);

    Pose pose = Sample(*skeleton_, clip, 0.0f);
    ASSERT_EQ(pose.size(), 2u);

    // Spine(인덱스 1)은 바인드 포즈(0, 0.5, 0) 그대로.
    EXPECT_NEAR(pose[1].position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(pose[1].position.y, 0.5f, 1e-4f);
    EXPECT_NEAR(pose[1].position.z, 0.0f, 1e-4f);
}

// --- key border: 첫 키 이전/마지막 키 이후는 clamp(hold) ---
TEST_F(AnimationSamplerTest, Sample_BeforeFirstKey_HoldsFirstKeyValue)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(10.0f, { 100, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(20.0f, { 200, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    Pose pose = Sample(*skeleton_, clip, 3.0f);  // 첫 키(10)보다 앞
    EXPECT_NEAR(pose[0].position.x, 100.0f, 1e-4f);
}

TEST_F(AnimationSamplerTest, Sample_AfterLastKey_HoldsLastKeyValue)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(10.0f, { 100, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(20.0f, { 200, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    Pose pose = Sample(*skeleton_, clip, 45.0f);  // 마지막 키(20)보다 뒤, totalFrames(60) 안쪽
    EXPECT_NEAR(pose[0].position.x, 200.0f, 1e-4f);
}

// --- key crossing: 정확히 키프레임 위의 프레임은 그 키 값과 정확히 일치 ---
TEST_F(AnimationSamplerTest, Sample_ExactlyOnKeyframe_MatchesKeyExactly)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(0.0f, { 0, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(10.0f, { 10, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(20.0f, { 20, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    Pose pose = Sample(*skeleton_, clip, 10.0f);  // 중간 키 위
    EXPECT_NEAR(pose[0].position.x, 10.0f, 1e-4f);
}

// --- 프레임 도메인 clamp (§C5): frame < 0, frame >= totalFrames ---
TEST_F(AnimationSamplerTest, Sample_NegativeFrame_ClampsToZero)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 60;
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = { MakeKey(0.0f, { 7, 0, 0 }, glm::quat(1, 0, 0, 0)) };
    clip.tracks.push_back(track);

    Pose poseNeg = Sample(*skeleton_, clip, -5.0f);
    Pose poseZero = Sample(*skeleton_, clip, 0.0f);
    EXPECT_NEAR(poseNeg[0].position.x, poseZero[0].position.x, 1e-4f);
}

TEST_F(AnimationSamplerTest, Sample_FrameBeyondTotalFrames_ClampsToLastValidFrame)
{
    KeyframeClip clip;
    clip.fps = 30.0f;
    clip.totalFrames = 10;  // 유효 프레임: 0..9
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(0.0f, { 0, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(9.0f, { 90, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    Pose poseAt9 = Sample(*skeleton_, clip, 9.0f);
    Pose poseBeyond = Sample(*skeleton_, clip, 100.0f);  // totalFrames(10) 훨씬 넘음 -> 9로 clamp
    EXPECT_NEAR(poseBeyond[0].position.x, poseAt9[0].position.x, 1e-4f);
}

// --- ShortestSlerp: 부호 정규화 (§C6) ---
TEST_F(AnimationSamplerTest, ShortestSlerp_TakesShortPathEvenWhenSignsDiffer)
{
    // a = Y축 10도 회전. b = "Y축 20도 회전"과 같은 회전이지만 부호가 반전된 표현(-q).
    glm::quat a = glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
    glm::quat bSameRotationNegated = -glm::angleAxis(glm::radians(20.0f), glm::vec3(0, 1, 0));

    // dot(a,b) < 0 이어야 이 테스트가 실제로 "부호가 반대인" 케이스를 검증하는 것.
    ASSERT_LT(glm::dot(a, bSameRotationNegated), 0.0f);

    glm::quat result = ShortestSlerp(a, bSameRotationNegated, 0.5f);

    // 최단경로로 갔다면 10도와 20도의 중간인 15도 근방이어야 한다(부호는 상관없음 - q와 -q는
    // 같은 회전이므로 |dot|로 비교).
    glm::quat expected15deg = glm::angleAxis(glm::radians(15.0f), glm::vec3(0, 1, 0));
    float similarity = std::abs(glm::dot(result, expected15deg));
    EXPECT_NEAR(similarity, 1.0f, 1e-3f);
}

TEST_F(AnimationSamplerTest, ShortestSlerp_SameSign_BehavesLikeNormalSlerp)
{
    glm::quat a = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));
    glm::quat b = glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 1, 0));
    ASSERT_GE(glm::dot(a, b), 0.0f);

    glm::quat result = ShortestSlerp(a, b, 0.5f);
    glm::quat expected = glm::slerp(a, b, 0.5f);
    EXPECT_NEAR(glm::dot(result, expected), 1.0f, 1e-4f);
}

// --- AnimationPlayer: update + currentLocalPose ---
TEST_F(AnimationSamplerTest, AnimationPlayer_UpdateAdvancesTimeAndPose)
{
    KeyframeClip clip;
    clip.fps = 10.0f;         // 1프레임 = 0.1초
    clip.totalFrames = 20;    // 클립 길이 2초
    KeyframeTrack track;
    track.boneName = "Hips";
    track.keys = {
        MakeKey(0.0f, { 0, 0, 0 }, glm::quat(1, 0, 0, 0)),
        MakeKey(10.0f, { 100, 0, 0 }, glm::quat(1, 0, 0, 0)),
    };
    clip.tracks.push_back(track);

    AnimationPlayer player;
    player.load(*skeleton_, clip);
    EXPECT_NEAR(player.currentTime(), 0.0f, 1e-5f);

    player.update(0.5f);  // 0.5초 경과 -> frame = 0.5 * 10fps = 5.0 -> 위치 x=50
    EXPECT_NEAR(player.currentTime(), 0.5f, 1e-4f);

    Pose pose = player.currentLocalPose();
    EXPECT_NEAR(pose[0].position.x, 50.0f, 1e-3f);
}

TEST_F(AnimationSamplerTest, AnimationPlayer_ClampLoop_StopsAtClipEnd)
{
    KeyframeClip clip;
    clip.fps = 10.0f;
    clip.totalFrames = 20;  // 클립 길이 = 2.0초
    clip.tracks.push_back({ "Hips", { MakeKey(0.0f, {0,0,0}, glm::quat(1,0,0,0)) } });

    AnimationPlayer player;
    player.load(*skeleton_, clip, LoopMode::Clamp);

    player.update(100.0f);  // 클립 길이를 훨씬 초과
    EXPECT_NEAR(player.currentTime(), 2.0f, 1e-4f);  // 클립 길이(2초)에서 정지
}
