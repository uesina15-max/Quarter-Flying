#include "Bindings.h"
#include "../animation/LayerMixer.h"
#include "../animation/MotionPreviewState.h"
#include "../renderer/RenderMode.h"

namespace Engine {
namespace Bindings {

// Motion Mixer 프리뷰(Phase 4A, 착수 계약서 §C11)를 Python에 노출한다.
// 의도적으로 최소한만 노출한다 - Motion Editor(Python/Qt)는 이 API로 "무엇을 보여줄지"만
// 지정하고, 블렌딩 연산(MixLayers) 자체는 재구현하지 않는다(§C: "UI blending 로직
// 재구현 금지 - Python은 weight 전달까지만, Mixer는 C++").
void RegisterAnimationBindings(pybind11::module_& m) {
    namespace py = pybind11;

    py::enum_<RenderMode>(m, "RenderMode")
        .value("Scene",  RenderMode::Scene)
        .value("Motion", RenderMode::Motion)
        .export_values();

    py::enum_<MaskPreset>(m, "MaskPreset")
        .value("Full",      MaskPreset::Full)
        .value("UpperBody", MaskPreset::UpperBody)
        .value("LowerBody", MaskPreset::LowerBody)
        .value("Custom",    MaskPreset::Custom)
        .export_values();

    py::class_<MotionPreviewState>(m, "MotionPreviewState")
        .def("LoadSkeleton",         &MotionPreviewState::LoadSkeleton)
        .def("LoadBaseClip",         &MotionPreviewState::LoadBaseClip)
        .def("SetBaseFrame",         &MotionPreviewState::SetBaseFrame)
        .def("HasSkeleton",          &MotionPreviewState::HasSkeleton)
        .def("HasBaseClip",          &MotionPreviewState::HasBaseClip)
        .def("GetLastError",         &MotionPreviewState::GetLastError)
        // Phase 5 - 믹서 UI: 호환성 사전 검사 + 레이어 스택 (id 기반, 착수 계약서 §C7).
        .def("CheckClipCompatible",  &MotionPreviewState::CheckClipCompatible)
        .def("AddLayer",             &MotionPreviewState::AddLayer)
        .def("RemoveLayer",          &MotionPreviewState::RemoveLayer)
        .def("ClearLayers",          &MotionPreviewState::ClearLayers)
        .def("GetLayerCount",        &MotionPreviewState::GetLayerCount)
        .def("SetLayerFrame",        &MotionPreviewState::SetLayerFrame)
        .def("SetLayerWeight",       &MotionPreviewState::SetLayerWeight)
        .def("SetLayerMask",         &MotionPreviewState::SetLayerMask);
}

} // namespace Bindings
} // namespace Engine
