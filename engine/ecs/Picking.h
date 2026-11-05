#pragma once

#include "Entity.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <functional>
#include <optional>

namespace Engine
{
    class ECSRegistry;
    struct RenderableComponent;

    // 에디터 뷰포트 피킹(클릭한 물체 고르기)의 유일한 구현. GL 없이 돈다 - 메시 경계만 주입받는다.
    //
    // 정밀도: 메시마다 로컬 축 정렬 상자(정점 경계)를 월드 행렬로 옮긴 상자와 광선을 비교한다. 삼각형 단위가
    // 아니므로 피라미드의 빈 모서리처럼 상자 안이지만 메시 밖인 곳을 눌러도 그 물체가 잡힌다.

    // 월드 공간 광선. direction은 정규화돼 있어서 t가 곧 월드 거리다.
    struct PickRay
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    // 메시 로컬 공간의 축 정렬 경계.
    struct LocalBounds
    {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct PickHit
    {
        Entity entity;
        float distance = 0.0f;   // 광선 시작점부터 맞은 곳까지 월드 거리
        glm::vec3 point{0.0f};   // 맞은 곳(월드). 뷰포트 드래그가 이 점을 잡고 움직인다
    };

    // 유닛 큐브(RenderSystem의 절차적 큐브)의 경계. 메시를 아직 모르거나 불러오지 못했을 때도 화면에는 이 큐브가
    // 그려지므로 같은 경계를 쓴다.
    inline constexpr LocalBounds kUnitCubeBounds{glm::vec3(-0.5f), glm::vec3(0.5f)};

    // 메시가 없는 카메라·파티클 이미터를 고를 수 있게 하는 대리 상자의 반 크기(월드 단위, 회전·스케일 무시).
    inline constexpr float kPickProxyHalfExtent = 0.35f;

    // 정규화 장치 좌표(x, y는 -1..1, y는 위가 +)와 그 화면을 그린 카메라 행렬로 광선을 만든다.
    // 행렬이 뒤집히지 않으면 nullopt.
    std::optional<PickRay> MakePickRay(const glm::mat4& view, const glm::mat4& projection, float ndcX, float ndcY);

    // model로 옮긴 로컬 상자와 광선의 교차 거리. 시작점이 상자 안이면 빠져나가는 거리.
    std::optional<float> IntersectRayBounds(const PickRay& ray, const glm::mat4& model, const LocalBounds& bounds);

    // RenderableComponent가 그리는 메시의 로컬 경계(보통 RenderSystem::GetMeshBounds).
    using MeshBoundsLookup = std::function<LocalBounds(const RenderableComponent&)>;

    // 광선에 가장 가까이 맞은 엔티티. 대상:
    //   - Transform + RenderableComponent: 메시 경계 x 월드 행렬(ComputeWorldMatrix, 부모 포함)
    //   - Transform + (CameraComponent 또는 ParticleEffectComponent), Renderable 없음: 월드 위치의 대리 상자
    // meshBounds가 비어 있으면 모든 메시를 유닛 큐브로 본다.
    std::optional<PickHit> PickEntity(ECSRegistry& registry, const PickRay& ray, const MeshBoundsLookup& meshBounds);
}
