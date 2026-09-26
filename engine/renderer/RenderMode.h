#pragma once

namespace Engine
{
    /// <summary>
    /// 뷰포트가 그릴 오버레이 종류 (착수 계약서 §C11 - "단일 EngineViewport 인스턴스에
    /// RenderScene 모드 enum"). 카메라/조명/기본 씬은 두 모드가 공유하고, Motion 모드는
    /// 그 위에 스켈레톤 본 라인을 추가로 덮어 그린다.
    ///
    /// Renderer.h가 아니라 별도 파일로 뺀 이유: Engine.h가 이 타입만 필요할 뿐
    /// Renderer.h 전체(RenderGraph/CommandList/InstancedBatchManager 등 무거운 include
    /// 체인)를 끌어올 필요는 없어서 - 가벼운 헤더 하나로 양쪽에서 공유한다.
    /// </summary>
    enum class RenderMode
    {
        Scene,   // 기존 Scene Editor 렌더링
        Motion,  // Scene 위에 Motion Editor 프리뷰(본 라인)를 겹쳐 그림
    };
}
