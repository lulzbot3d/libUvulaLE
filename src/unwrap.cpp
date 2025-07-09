// (c) 2025, UltiMaker -- see LICENCE for details

#include "unwrap.hpp"

#include <cstdio>
#include <iostream>
#include <ostream>

#include "xatlas_c.h"
#include "xatlas.h"

// NOTE: The UV-coords parameter already has the right size, and a pointer is set up towards its 'innards', please don't append, emplace, etc. or do anything to change its size or location.
bool unwrap_algo(
    const std::vector<std::tuple<float, float, float>>& vertices,
    const std::vector<std::tuple<int32_t, int32_t, int32_t>>& indices,
    uint32_t desired_definition,
    std::vector<std::tuple<float, float>>& uv_coords,
    uint32_t &texture_width,
    uint32_t &texture_height)
{
    xatlas::Atlas* atlas = xatlas::Create();

    xatlas::MeshDecl meshDecl;
    meshDecl.vertexPositionData = vertices.data();
    meshDecl.indexData = indices.data();
    meshDecl.vertexCount = vertices.size();
    meshDecl.vertexPositionStride = sizeof(float) * 3;
    meshDecl.indexCount = indices.size() * 3;
    meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
    if (xatlas::AddMesh(atlas, meshDecl) != xatlas::AddMeshError::Success)
    {
        xatlas::Destroy(atlas);
        printf("\rError adding mesh\n");
        return false;
    }

    constexpr xatlas::ChartOptions chart_options;
    const xatlas::PackOptions pack_options{.resolution = desired_definition};

    xatlas::Generate(atlas, chart_options, pack_options);

    // For some reason, the width and height need to be inverted to make the coordinates consistent
    texture_width = atlas->height;
    texture_height = atlas->width;

    const xatlas::Mesh &output_mesh = *atlas->meshes;

    const auto width = static_cast<float>(atlas->width);
    const auto height = static_cast<float>(atlas->height);

    for (size_t i = 0 ; i<output_mesh.vertexCount ; ++i)
    {
        const xatlas::Vertex &vertex = output_mesh.vertexArray[i];
        uv_coords[vertex.xref] = std::make_tuple(vertex.uv[0] / width, vertex.uv[1] / height);
    }

    xatlas::Destroy(atlas);
    return true;
}
