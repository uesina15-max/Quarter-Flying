#pragma once

#include "Shader.h"

namespace Engine
{
    class InstancedBatchManager;
    class Camera;

    /// <summary>
    /// ECS RenderSystem이 InstancedBatchManager에 채워 넣은 배치를 실제로 그린다
    /// (ROADMAP.md P0-2). DebugGridRenderer/BoneLineRenderer와 같은 이유로 셰이더를
    /// 파일(assets/shaders/*.glsl)이 아니라 소스 문자열로 직접 컴파일한다 - 상대경로가
    /// 프로세스 CWD(에디터는 engine/editor/에서 실행) 기준이라 파일 경로 방식은 항상 깨진다.
    ///
    /// engine/assets/shaders/pbr_instanced.vert/frag를 그대로 쓰지 않은 이유: 그 프래그먼트
    /// 셰이더는 인스턴스별 color/roughness/metallic을 실제로는 안 쓰고(주석에 "until full
    /// instance buffer integration" 명시) objectColor 등을 uniform으로만 받는 상태라
    /// InstancedBatchManager가 이미 인스턴스 attribute로 업로드해둔 값과 어긋난다. 또한
    /// point-light 배열(1/distance^2 감쇠)을 쓰는데, 채우지 않은 광원을 원점에 두면
    /// 오브젝트가 원점에 있을 때 거리 0으로 나누는 문제가 생길 수 있다. 이 클래스는 그 대신
    /// 인스턴스 attribute(location 7/8/9)를 실제로 읽고, 감쇠 없는 방향광 하나만 쓰는
    /// 단순한 셰이더를 쓴다.
    /// </summary>
    class SceneMeshRenderer
    {
    public:
        SceneMeshRenderer() = default;
        ~SceneMeshRenderer() = default;

        SceneMeshRenderer(const SceneMeshRenderer&) = delete;
        SceneMeshRenderer& operator=(const SceneMeshRenderer&) = delete;

        bool Initialize();
        void Shutdown();
        bool IsInitialized() const { return initialized; }

        // RenderSystem이 이번 프레임에 채워둔 배치를 그린다. batchManager는 UpdateDynamicBatches()
        // (dirty 배치 GPU 업로드)까지 이 함수 안에서 처리한다 - 호출자가 순서를 신경 쓸 필요 없음.
        void Render(InstancedBatchManager& batchManager, const Camera& camera);

    private:
        Shader shader;
        bool initialized = false;
    };
}
