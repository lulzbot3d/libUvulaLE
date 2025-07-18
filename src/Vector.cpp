#include "Vector.h"

#include <cmath>
#include <limits>

#include "Vertex.h"

Vector::Vector(const float x, const float y, const float z)
    : x_(x)
    , y_(y)
    , z_(z)
{
}

Vector::Vector(const Vertex& v1, const Vertex& v2)
    : x_(v2.x - v1.x)
    , y_(v2.y - v1.y)
    , z_(v2.z - v1.z)
{
}

float Vector::dot(const Vector& other) const
{
    return (x_ * other.x_) + (y_ * other.y_) + (z_ * other.z_);
}

Vector Vector::cross(const Vector& other) const
{
    return Vector((y_ * other.z_) - (z_ * other.y_), (z_ * other.x_) - (x_ * other.z_), (x_ * other.y_) - (y_ * other.x_));
}

Vector Vector::operator+(const Vector& other) const
{
    return Vector(x_ + other.x_, y_ + other.y_, z_ + other.z_);
}

void Vector::operator+=(const Vector& other)
{
    x_ += other.x_;
    y_ += other.y_;
    z_ += other.z_;
}

Vector Vector::operator*(const float factor) const
{
    return Vector(x_ * factor, y_ * factor, z_ * factor);
}

void Vector::operator*=(const float factor)
{
    x_ *= factor;
    y_ *= factor;
    z_ *= factor;
}

Vector Vector::operator/(const float factor) const
{
    return Vector(x_ / factor, y_ / factor, z_ / factor);
}

void Vector::operator/=(const float factor)
{
    x_ /= factor;
    y_ /= factor;
    z_ /= factor;
}

float Vector::lengthSquared() const
{
    return (x_ * x_) + (y_ * y_) + (z_ * z_);
}

float Vector::length() const
{
    return std::sqrt(lengthSquared());
}

bool Vector::normalize()
{
    const float actual_length = length();
    if (actual_length > std::numeric_limits<float>::epsilon()) [[likely]]
    {
        *this /= actual_length;
        return true;
    }

    x_ = 0.0;
    y_ = 0.0;
    z_ = 0.0;
    return false;
}

std::optional<Vector> Vector::normalized() const
{
    const float actual_length = length();
    if (actual_length > std::numeric_limits<float>::epsilon()) [[likely]]
    {
        return *this / actual_length;
    }

    return std::nullopt;
}
