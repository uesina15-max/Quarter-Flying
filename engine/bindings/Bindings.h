#pragma once
#include <pybind11/pybind11.h>

namespace Engine {
namespace Bindings {

void RegisterMathBindings(pybind11::module_& m);
void RegisterECSBindings(pybind11::module_& m);
void RegisterEngineBindings(pybind11::module_& m);

void RegisterEntityBindings(pybind11::module_& m);
void RegisterComponentBindings(pybind11::module_& m);
void RegisterRegistryBindings(pybind11::module_& m);
void RegisterWorldBindings(pybind11::module_& m);

} // namespace Bindings
} // namespace Engine
