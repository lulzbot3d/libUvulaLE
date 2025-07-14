// (c) 2025, UltiMaker -- see LICENCE for details

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "lscm_unwrap.h"
namespace py = pybind11;

#include "unwrap.hpp"

py::tuple unwrap(const py::array_t<float>& vertices_array, const py::array_t<int32_t>& indices_array)
{
    // input shaping
    const pybind11::buffer_info vertices_buf = vertices_array.request();
    const pybind11::buffer_info indices_buf = indices_array.request();
    if (vertices_buf.ndim != 2 || indices_buf.ndim != 2)
    {
        throw std::runtime_error("Vertices should be <float, float, float> and indices should be (grouped by face as) <int, int, int>.");
    }

    const auto* vertices_ptr = static_cast<std::tuple<float, float, float>*>(vertices_buf.ptr);
    const auto* indices_ptr = static_cast<std::tuple<int32_t, int32_t, int32_t>*>(indices_buf.ptr);
    const auto vertices = std::vector<std::tuple<float, float, float>>(vertices_ptr, vertices_ptr + vertices_buf.shape[0]);
    const auto indices = std::vector<std::tuple<int32_t, int32_t, int32_t>>(indices_ptr, indices_ptr + indices_buf.shape[0]);

    // output shaping
    const std::vector<py::ssize_t> shape = { static_cast<py::ssize_t>(vertices.size()), 2 };
    const std::vector<py::ssize_t> strides = { static_cast<py::ssize_t>(sizeof(float) * shape[1]), static_cast<py::ssize_t>(sizeof(float)) };
    std::vector<std::tuple<float, float>> res(shape[0], { 0.0, 0.0 });
    uint32_t texture_width;
    uint32_t texture_height;

    // Do the actual calculation here
    printf("Starting actual unwrap\n");
    if (! unwrap_lscm(vertices, indices, res, texture_width, texture_height))
    {
        throw std::runtime_error("Couldn't unwrap UV's!");
    }
    printf("Unwrap done\n");

    // send output
    return py::make_tuple(py::array(py::buffer_info(res.data(), strides[1], py::format_descriptor<float>::format(), shape.size(), shape, strides)), texture_width, texture_height);
}

PYBIND11_MODULE(pyUvula, module)
{
    module.doc() = "UV-unwrapping library (or bindings to library), either ported from [xatlas|blender] or self-written (whichever one we end up with).";
    module.attr("__version__") = PYUVULA_VERSION;

    module.def("unwrap", &unwrap, "Given the vertices, indices of a mesh, unwrap UV for texture-coordinates.");
}
