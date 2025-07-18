#include "Matrix.h"

#include <cmath>
#include <limits>

#include "UVCoord.h"
#include "Vector.h"
#include "Vertex.h"


void Matrix::transpose()
{
    std::swap(values_[0][1], values_[1][0]);
    std::swap(values_[0][2], values_[2][0]);
    std::swap(values_[1][2], values_[2][1]);
}

UVCoord Matrix::project(const Vertex& vertex) const
{
    return UVCoord{ .u = (values_[0][0] * vertex.x) + (values_[1][0] * vertex.y) + (values_[2][0] * vertex.z),
                    .v = (values_[0][1] * vertex.x) + (values_[1][1] * vertex.y) + (values_[2][1] * vertex.z) };
}

Matrix Matrix::makeOrthogonalBasis(const Vector& normal)
{
    Matrix matrix;
    matrix.values_[2][0] = normal.x();
    matrix.values_[2][1] = normal.y();
    matrix.values_[2][2] = normal.z();

    constexpr float eps = std::numeric_limits<float>::epsilon();
    const float length_squared = Vector(normal.x(), normal.y(), 0.0).lengthSquared();

    if (length_squared > eps)
    {
        const float length = std::sqrt(length_squared);

        matrix.values_[0][0] = normal.y() / length;
        matrix.values_[0][1] = -normal.x() / length;
        matrix.values_[0][2] = 0.0;
        matrix.values_[1][0] = -normal.z() * matrix.values_[0][1];
        matrix.values_[1][1] = normal.z() * matrix.values_[0][0];
        matrix.values_[1][2] = normal.x() * matrix.values_[0][1] - normal.y() * matrix.values_[0][0];
    }
    else
    {
        matrix.values_[0][0] = (normal.z() < 0.0f) ? -1.0f : 1.0f;
        matrix.values_[0][1] = matrix.values_[0][2] = matrix.values_[1][0] = matrix.values_[1][2] = 0.0f;
        matrix.values_[1][1] = 1.0f;
    }

    matrix.transpose();

    return matrix;
}
