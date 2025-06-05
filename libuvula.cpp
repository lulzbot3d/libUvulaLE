// (c) 2025, UltiMaker -- see LICENCE for details

#include <pybind11/pybind11.h>
namespace py = pybind11;

#include "./include/unwrap.hpp"

PYBIND11_MODULE(libuvula, lib)
{
	lib.doc() = "UV-unwrapping library (or bindings to library), either ported from [xatlas|blender] or self-written (whichever one we end up with).";

	lib.def("unwrap", &unwrap, "Given the vertices, indices of a mesh, unwrap UV for texture-coordinates.");
}
