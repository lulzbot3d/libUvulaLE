#include "geometry_utils.h"

#include <numbers>

#include "Vector.h"


namespace geometry_utils
{

std::optional<Vector> triangleNormal(const Vertex& v1, const Vertex& v2, const Vertex& v3)
{
    return Vector(v1, v2).cross(Vector(v1, v3)).normalized();
}

float deg2rad(float angle)
{
    return angle * std::numbers::pi / 180.0;
}

}; // namespace geometry_utils
