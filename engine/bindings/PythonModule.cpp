#include "Bindings.h"
#include <pybind11/pybind11.h>

PYBIND11_MODULE(ge_python, m) {
    m.doc() = "GE Engine Python Bindings";

    Engine::Bindings::RegisterMathBindings(m);
    Engine::Bindings::RegisterECSBindings(m);
    Engine::Bindings::RegisterEngineBindings(m);
}
