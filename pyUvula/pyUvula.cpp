// (c) 2025, UltiMaker -- see LICENCE for details

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "Face.h"
#include "UVCoord.h"
#include "Vertex.h"
#include "unwrap.h"

namespace py = pybind11;

py::tuple unwrap(const py::array_t<float>& vertices_array, const py::array_t<int32_t>& indices_array)
{
    // input shaping
    const pybind11::buffer_info vertices_buf = vertices_array.request();
    const pybind11::buffer_info indices_buf = indices_array.request();
    if (vertices_buf.ndim != 2 || indices_buf.ndim != 2)
    {
        throw std::runtime_error("Vertices should be <float, float, float> and indices should be (grouped by face as) <int, int, int>.");
    }

    const auto* vertices_ptr = static_cast<Vertex*>(vertices_buf.ptr);
    const auto* indices_ptr = static_cast<Face*>(indices_buf.ptr);
    const auto vertices = std::vector<Vertex>(vertices_ptr, vertices_ptr + vertices_buf.shape[0]);
    const auto indices = std::vector<Face>(indices_ptr, indices_ptr + indices_buf.shape[0]);

    // output shaping
    const std::vector<py::ssize_t> shape = { static_cast<py::ssize_t>(vertices.size()), 2 };
    const std::vector<py::ssize_t> strides = { static_cast<py::ssize_t>(sizeof(float) * shape[1]), static_cast<py::ssize_t>(sizeof(float)) };
    std::vector<UVCoord> res(shape[0], { 0.0, 0.0 });
    uint32_t texture_width;
    uint32_t texture_height;

    {
        py::gil_scoped_release release;

        // Do the actual calculation here
        if (! smartUnwrap(vertices, indices, res, texture_width, texture_height))
        {
            throw std::runtime_error("Couldn't unwrap UV's!");
        }
    }

    // send output
    return py::make_tuple(py::array(py::buffer_info(res.data(), strides[1], py::format_descriptor<float>::format(), shape.size(), shape, strides)), texture_width, texture_height);
}

PYBIND11_MODULE(pyUvula, module)
{
    module.doc() = "UV-unwrapping library (or bindings to library), segmentation uses a classic normal-based grouping and charts packing uses xatlas";
    module.attr("__version__") = PYUVULA_VERSION;

    module.def("unwrap", &unwrap, "Given the vertices, indices of a mesh, unwrap UV for texture-coordinates.");
}
