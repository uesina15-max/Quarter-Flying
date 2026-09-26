#include "Skeleton.h"
#include <functional>
#include <unordered_set>

namespace Engine
{
    namespace
    {
        // 각 본에서 parentIndex를 따라 걸으며 사이클을 감지한다. 배열 순서(어떤 인덱스가
        // 어떤 인덱스보다 먼저 오는지)에 의존하지 않는다 - 형제/배치 순서 무관 계약(§C3)과
        // 일관되게, "부모가 항상 자식보다 앞선 인덱스" 같은 가정을 두지 않는다.
        bool HasCycle(const std::vector<Bone>& bones)
        {
            enum class State : uint8_t { Unvisited, Visiting, Done };
            std::vector<State> state(bones.size(), State::Unvisited);

            for (size_t start = 0; start < bones.size(); ++start)
            {
                if (state[start] == State::Done)
                {
                    continue;
                }

                std::vector<int> path;
                int current = static_cast<int>(start);
                bool cycleFound = false;
                while (current != -1)
                {
                    if (state[current] == State::Visiting)
                    {
                        cycleFound = true;
                        break;
                    }
                    if (state[current] == State::Done)
                    {
                        break; // 이미 사이클 없음이 확인된 노드에 합류
                    }
                    state[current] = State::Visiting;
                    path.push_back(current);
                    current = bones[current].parentIndex;
                }

                for (int idx : path)
                {
                    state[idx] = State::Done;
                }

                if (cycleFound)
                {
                    return true;
                }
            }
            return false;
        }
    }

    std::expected<Skeleton, SkeletonError> Skeleton::Create(
        std::vector<Bone> bonesIn, AxisSystem axis, LengthUnit unit)
    {
        // 1. 이름 중복 검사
        std::unordered_set<std::string> seenNames;
        seenNames.reserve(bonesIn.size());
        for (const Bone& bone : bonesIn)
        {
            if (!seenNames.insert(bone.name).second)
            {
                return std::unexpected(SkeletonError::DuplicateBoneName);
            }
        }

        // 2. parentIndex 유효성 + 루트 개수 검사
        int rootCount = 0;
        for (size_t i = 0; i < bonesIn.size(); ++i)
        {
            int parent = bonesIn[i].parentIndex;
            if (parent == -1)
            {
                ++rootCount;
                continue;
            }
            bool inRange = parent >= 0 && parent < static_cast<int>(bonesIn.size());
            bool selfParent = inRange && parent == static_cast<int>(i);
            if (!inRange || selfParent)
            {
                return std::unexpected(SkeletonError::InvalidParentIndex);
            }
        }

        if (rootCount == 0)
        {
            return std::unexpected(SkeletonError::NoRootBone);
        }
        if (rootCount > 1)
        {
            return std::unexpected(SkeletonError::MultipleRootBones);
        }

        // 3. 사이클 검사 (자기 자신을 부모로 두는 건 위에서 이미 걸렀으므로 여기서는
        //    2개 이상 본이 서로를 부모로 두는 간접 사이클만 남는다)
        if (HasCycle(bonesIn))
        {
            return std::unexpected(SkeletonError::CyclicHierarchy);
        }

        Skeleton skeleton;
        skeleton.bones = std::move(bonesIn);
        skeleton.nameToIndex.reserve(skeleton.bones.size());
        for (size_t i = 0; i < skeleton.bones.size(); ++i)
        {
            skeleton.nameToIndex[skeleton.bones[i].name] = static_cast<int>(i);
        }
        skeleton.signature = ComputeSkeletonSignature(skeleton.bones, axis, unit);

        return skeleton;
    }

    int Skeleton::FindBoneIndex(const std::string& name) const
    {
        auto it = nameToIndex.find(name);
        return (it != nameToIndex.end()) ? it->second : -1;
    }

    std::vector<Transform> Skeleton::ComputeBindWorldTransforms() const
    {
        Pose bindLocal;
        bindLocal.reserve(bones.size());
        for (const Bone& bone : bones)
        {
            bindLocal.push_back(bone.bind);
        }
        return ComputeWorldTransforms(bindLocal);
    }

    std::vector<Transform> Skeleton::ComputeWorldTransforms(const Pose& localPose) const
    {
        std::vector<Transform> world(bones.size());
        std::vector<bool> computed(bones.size(), false);

        // parentIndex가 항상 자기보다 앞선 인덱스라는 보장이 없으므로 재귀로 부모부터 계산한다.
        std::function<Transform(int)> resolve = [&](int index) -> Transform
        {
            if (computed[index])
            {
                return world[index];
            }
            int parent = bones[index].parentIndex;
            Transform result = (parent == -1)
                ? localPose[index]
                : ComposeTransform(resolve(parent), localPose[index]);
            world[index] = result;
            computed[index] = true;
            return result;
        };

        for (size_t i = 0; i < bones.size(); ++i)
        {
            resolve(static_cast<int>(i));
        }

        return world;
    }
}
