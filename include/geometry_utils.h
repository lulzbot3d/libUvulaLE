// (c) 2025, UltiMaker -- see LICENCE for details

#pragma once

#include <optional>

struct Vertex;
class Vector;

namespace geometry_utils
{

std::optional<Vector> triangleNormal(const Vertex& v1, const Vertex& v2, const Vertex& v3);

float deg2rad(float angle);

}; // namespace geometry_utils
