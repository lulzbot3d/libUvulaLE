// (c) 2025, UltiMaker -- see LICENCE for details

#pragma once

struct UVCoord;
struct Vertex;
class Vector;

class Matrix
{
public:
    explicit Matrix() = default;

    void transpose();

    [[nodiscard]] UVCoord project(const Vertex& vertex) const;

    static Matrix makeOrthogonalBasis(const Vector& normal);

private:
    float values_[3][3];
};
