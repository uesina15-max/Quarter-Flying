#include "Bindings.h"
#include "../core/Types.h"
#include <string>

namespace Engine {
namespace Bindings {

void RegisterMathBindings(pybind11::module_& m) {
    namespace py = pybind11;

    py::class_<Vec3>(m, "Vec3")
        .def(py::init<>())
        .def(py::init<float, float, float>())
        .def_readwrite("x", &Vec3::x)
        .def_readwrite("y", &Vec3::y)
        .def_readwrite("z", &Vec3::z)
        .def("__repr__", [](const Vec3& v) {
            return "Vec3(" + std::to_string(v.x) + ", "
                           + std::to_string(v.y) + ", "
                           + std::to_string(v.z) + ")";
        });

    py::class_<Quaternion>(m, "Quaternion")
        .def(py::init<>())
        .def(py::init<float, float, float, float>())
        .def_readwrite("x", &Quaternion::x)
        .def_readwrite("y", &Quaternion::y)
        .def_readwrite("z", &Quaternion::z)
        .def_readwrite("w", &Quaternion::w);
}

} // namespace Bindings
} // namespace Engine
