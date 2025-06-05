// (c) 2025, UltiMaker -- see LICENCE for details

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
namespace py = pybind11;

#include "./include/unwrap.hpp"

py::array_t<float> unwrap(const py::array_t<float>& vertices_, const py::array_t<int32_t>& indices_)
{
    // input shaping
    auto verts_buf = vertices_.request();
    auto indxs_buf = indices_.request();
    if (verts_buf.ndim != 2 || indxs_buf.ndim != 2)
    {
        throw std::runtime_error("Vertices should be <float, float, float> and indices should be (grouped by face as) <int, int, int>.");
    }

    auto verts_ptr = static_cast<std::tuple<float, float, float>*>(verts_buf.ptr);
    auto indxs_ptr = static_cast<std::tuple<int32_t, int32_t, int32_t>*>(indxs_buf.ptr);
    const auto vertices = std::vector<std::tuple<float, float, float>>(verts_ptr, verts_ptr + verts_buf.shape[0]);
    const auto indices = std::vector<std::tuple<int32_t, int32_t, int32_t>>(indxs_ptr, indxs_ptr + indxs_buf.shape[0]);

    // output shaping
    std::vector<py::ssize_t> shape = { static_cast<py::ssize_t>(vertices.size()), 2 };
    std::vector<py::ssize_t> strides = { static_cast<py::ssize_t>(sizeof(float) * shape[1]), static_cast<py::ssize_t>(sizeof(float)) };
    std::vector<std::tuple<float, float>> res(shape[0]);

    // do the actual calculation here
    unwrap_algo(vertices, indices, res);

    // send output
    return py::array(py::buffer_info(
        res.data(),
        strides[1],
        py::format_descriptor<float>::format(),
        shape.size(),
        shape,
        strides
    ));
}

PYBIND11_MODULE(libuvula, lib)
{
    lib.doc() = "UV-unwrapping library (or bindings to library), either ported from [xatlas|blender] or self-written (whichever one we end up with).";

    lib.def("unwrap", &unwrap, "Given the vertices, indices of a mesh, unwrap UV for texture-coordinates.");
}
