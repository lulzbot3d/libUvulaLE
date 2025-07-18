// (c) 2025, UltiMaker -- see LICENCE for details

#pragma once

#include <optional>

class Vertex;

class Vector
{
public:
    explicit Vector() = default;

    Vector(const float x, const float y, const float z);

    Vector(const Vertex& v1, const Vertex& v2);

    float x() const
    {
        return x_;
    }

    float y() const
    {
        return y_;
    }

    float z() const
    {
        return z_;
    }

    [[nodiscard]] float dot(const Vector& other) const;

    [[nodiscard]] Vector cross(const Vector& other) const;

    Vector operator+(const Vector& other) const;

    void operator+=(const Vector& other);

    Vector operator*(const float factor) const;

    void operator*=(const float factor);

    Vector operator/(const float factor) const;

    void operator/=(const float factor);

    [[nodiscard]] float lengthSquared() const;

    [[nodiscard]] float length() const;

    bool normalize();

    [[nodiscard]] std::optional<Vector> normalized() const;

private:
    float x_{ 0.0 };
    float y_{ 0.0 };
    float z_{ 0.0 };
};
