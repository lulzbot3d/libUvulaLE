// (c) 2025, UltiMaker -- see LICENCE for details

#pragma once

struct Vertex
{
    float x{ 0.0 };
    float y{ 0.0 };
    float z{ 0.0 };
};

static bool operator<(const Vertex& lhs, const Vertex& rhs)
{
    if (lhs.x != rhs.x)
    {
        return lhs.x < rhs.x;
    }

    if (lhs.y != rhs.y)
    {
        return lhs.y < rhs.y;
    }

    return lhs.z < rhs.z;
}
