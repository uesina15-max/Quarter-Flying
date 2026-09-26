#include <gtest/gtest.h>
#include "../animation/AnimationSampler.h"
#include "../animation/ClipJsonIO.h"
#include "../animation/SkeletonJsonIO.h"
#include <optional>

using namespace Engine;

namespace
{
    const char* kValidSkeletonJson = R"JSON(
{
    "format": "core.skeleton",
    "version": 1,
    "axis": "Y_UP_RH",
    "unit": "meter",
    "name": "HumanoidBasic",
    "bones": [
        { "name": "Hips",  "parent": -1, "bindPos": [0, 1.0, 0] },
        { "name": "Spine", "parent": 0,  "bindPos": [0, 0.15, 0] }
    ]
}
)JSON";

    std::string ValidClipJson(const char* signatureHex = nullptr)
    {
        std::string sigField;
        if (signatureHex)
        {
            sigField = std::string("\"signature\": \"") + signatureHex + "\",";
        }
        return std::string(R"JSON(
{
    "format": "core.clip",
    "version": 1,
    "axis": "Y_UP_RH",
    "unit": "meter",
    "skeletonRef": "HumanoidBasic",
    )JSON") + sigField + R"JSON(
    "fps": 30,
    "totalFrames": 60,
    "tracks": [
        {
            "bone": "Hips",
            "keyframes": [
                { "frame": 0,  "pos": [0, 1.0, 0], "rot": [0, 0, 0, 1] },
                { "frame": 30, "pos": [0, 1.1, 0], "rot": [0, 0.05, 0, 0.9987] }
            ]
        }
    ]
}
)JSON";
    }
}

class AnimationJsonIOTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto skeleton = ParseSkeletonJson(kValidSkeletonJson);
        ASSERT_TRUE(skeleton.has_value());
        skeleton_ = std::move(skeleton.value());
    }

    std::optional<Skeleton> skeleton_;
};

// --- SkeletonJsonIO: 성공 케이스 ---
TEST_F(AnimationJsonIOTest, ParseSkeletonJson_Valid_Succeeds)
{
    EXPECT_EQ(skeleton_->GetBoneCount(), 2u);
    EXPECT_EQ(skeleton_->FindBoneIndex("Hips"), 0);
    EXPECT_EQ(skeleton_->FindBoneIndex("Spine"), 1);
}

TEST_F(AnimationJsonIOTest, ParseSkeletonJson_UnsupportedFormat_Rejected)
{
    const char* json = R"JSON({"format":"not.core.skeleton","version":1,"axis":"Y_UP_RH","unit":"meter","bones":[]})JSON";
    auto result = ParseSkeletonJson(json);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SkeletonJsonError::UnsupportedFormat);
}

TEST_F(AnimationJsonIOTest, ParseSkeletonJson_UnsupportedAxis_Rejected)
{
    const char* json = R"JSON({"format":"core.skeleton","version":1,"axis":"Z_UP_LH","unit":"meter","bones":[]})JSON";
    auto result = ParseSkeletonJson(json);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SkeletonJsonError::UnsupportedAxis);
}

TEST_F(AnimationJsonIOTest, ParseSkeletonJson_MalformedJson_Rejected)
{
    auto result = ParseSkeletonJson("{ not valid json ]");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SkeletonJsonError::ParseError);
}

TEST_F(AnimationJsonIOTest, ParseSkeletonJson_CyclicBones_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.skeleton", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "bones": [
            { "name": "A", "parent": 1 },
            { "name": "B", "parent": 0 }
        ]
    })JSON";
    auto result = ParseSkeletonJson(json);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SkeletonJsonError::SkeletonValidationFailed);
}

// --- ClipJsonIO: 성공 케이스 + Sample과의 연결 ---
TEST_F(AnimationJsonIOTest, ParseClipJson_Valid_SucceedsAndIsSampleable)
{
    auto clip = ParseClipJson(ValidClipJson(), *skeleton_);
    ASSERT_TRUE(clip.has_value());
    EXPECT_EQ(clip->tracks.size(), 1u);
    EXPECT_EQ(clip->signature, skeleton_->GetSignature());

    Pose pose = Sample(*skeleton_, *clip, 0.0f);
    ASSERT_EQ(pose.size(), 2u);
    EXPECT_NEAR(pose[0].position.y, 1.0f, 1e-4f);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_MatchingSignatureField_Succeeds)
{
    char hexBuf[32];
    std::snprintf(hexBuf, sizeof(hexBuf), "%llx",
                  static_cast<unsigned long long>(skeleton_->GetSignature().hash));

    auto clip = ParseClipJson(ValidClipJson(hexBuf), *skeleton_);
    EXPECT_TRUE(clip.has_value());
}

TEST_F(AnimationJsonIOTest, ParseClipJson_MismatchedSignatureField_Rejected)
{
    auto clip = ParseClipJson(ValidClipJson("deadbeefdeadbeef"), *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::SignatureMismatch);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_UnknownBone_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "NoSuchBone", "keyframes": [ { "frame": 0, "pos": [0,0,0], "rot": [0,0,0,1] } ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::UnknownBone);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_DuplicateFrame_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "Hips", "keyframes": [
                { "frame": 5, "pos": [0,0,0], "rot": [0,0,0,1] },
                { "frame": 5, "pos": [1,0,0], "rot": [0,0,0,1] }
            ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::DuplicateFrame);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_NonMonotonicFrame_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "Hips", "keyframes": [
                { "frame": 10, "pos": [0,0,0], "rot": [0,0,0,1] },
                { "frame": 5,  "pos": [1,0,0], "rot": [0,0,0,1] }
            ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::NonMonotonicFrame);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_NegativeFrame_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "Hips", "keyframes": [ { "frame": -1, "pos": [0,0,0], "rot": [0,0,0,1] } ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::NegativeFrame);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_FrameOutOfRange_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "Hips", "keyframes": [ { "frame": 60, "pos": [0,0,0], "rot": [0,0,0,1] } ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::FrameOutOfRange);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_InvalidFps_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 0, "totalFrames": 60, "tracks": []
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::InvalidFps);
}

TEST_F(AnimationJsonIOTest, ParseClipJson_ZeroQuaternion_Rejected)
{
    const char* json = R"JSON(
    {
        "format": "core.clip", "version": 1, "axis": "Y_UP_RH", "unit": "meter",
        "skeletonRef": "HumanoidBasic", "fps": 30, "totalFrames": 60,
        "tracks": [
            { "bone": "Hips", "keyframes": [ { "frame": 0, "pos": [0,0,0], "rot": [0,0,0,0] } ] }
        ]
    })JSON";
    auto clip = ParseClipJson(json, *skeleton_);
    ASSERT_FALSE(clip.has_value());
    EXPECT_EQ(clip.error(), ClipJsonError::InvalidQuaternion);
}
