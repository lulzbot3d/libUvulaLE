// (c) 2025, UltiMaker -- see LICENCE for details

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

py::array_t<float> unwrap(const py::array_t<float>& vertices, const py::array_t<int32_t>& indices);
