#include "Bindings.h"
#include <pybind11/pybind11.h>

PYBIND11_MODULE(quarterflying, m) {
    m.doc() = "Quarter Flying Engine Python Bindings";

    Engine::Bindings::RegisterMathBindings(m);
    Engine::Bindings::RegisterECSBindings(m);
    Engine::Bindings::RegisterAnimationBindings(m);
    Engine::Bindings::RegisterEngineBindings(m);
    Engine::Bindings::RegisterEditorBindings(m);
}
