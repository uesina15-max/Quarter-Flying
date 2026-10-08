#include "Bindings.h"
#include <pybind11/pybind11.h>

PYBIND11_MODULE(quarterflying, m) {
    m.doc() = "Quarter Flying Engine Python Bindings";
#ifdef QF_ENGINE_VERSION
    // Build Game이 Player 템플릿(runtime_manifest.json)과 같은 엔진인지 확인하는 데 쓴다.
    m.attr("__version__") = QF_ENGINE_VERSION;
#endif

    Engine::Bindings::RegisterMathBindings(m);
    Engine::Bindings::RegisterECSBindings(m);
    Engine::Bindings::RegisterAnimationBindings(m);
    Engine::Bindings::RegisterEngineBindings(m);
    Engine::Bindings::RegisterEditorBindings(m);
}
