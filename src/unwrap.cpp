// (c) 2025, UltiMaker -- see LICENCE for details

#include "unwrap.hpp"

#include "xatlas_c.h"
#include "xatlas.h"

// NOTE: The UV-coords parameter already has the right size, and a pointer is set up towards its 'innards', please don't append, emplace, etc. or do anything to change its size or location.
bool unwrap_algo(
    const std::vector<std::tuple<float, float, float>>& vertices,
    const std::vector<std::tuple<int32_t, int32_t, int32_t>>& indices,
    std::vector<std::tuple<float, float>>& uv_coords
)
{
    // NOTE: Moslty copied from the xatlas example(s) at the moment.
    // TODO: Make this not crash!

    xatlas::Atlas* atlas = xatlas::Create();
    uint32_t totalVertices = 0, totalFaces = 0;
    {
        // NOTE: It goes wrong here; the counts, sizes and data-pointers here are probably all wrong.

            xatlas::UvMeshDecl meshDecl;
            meshDecl.vertexCount = vertices.size();
            meshDecl.vertexUvData = uv_coords.data();
            meshDecl.vertexStride = sizeof(float) * 3;
            meshDecl.indexCount = indices.size();
            meshDecl.indexData = indices.data();
            meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
            xatlas::AddMeshError error = xatlas::AddUvMesh(atlas, meshDecl);
            if (error != xatlas::AddMeshError::Success) {
                xatlas::Destroy(atlas);
                printf("\rError adding mesh\n");
                return false;
            }
            totalVertices += meshDecl.vertexCount;
            totalFaces += meshDecl.indexCount / 3;
    }
    printf("   %u total vertices\n", totalVertices);
    printf("   %u total triangles\n", totalFaces);
    // Compute charts.
    printf("Computing charts\n");
    xatlas::ComputeCharts(atlas);
    // Pack charts.
    printf("Packing charts\n");
    xatlas::PackCharts(atlas);
    printf("   %d charts\n", atlas->chartCount);
    printf("   %d atlases\n", atlas->atlasCount);
    for (uint32_t i = 0; i < atlas->atlasCount; i++)
        printf("      %d: %0.2f%% utilization\n", i, atlas->utilization[i] * 100.0f);
    printf("   %ux%u resolution\n", atlas->width, atlas->height);
    
    // TODO: (Remove print-statements and) output the UV-coords from the atlas to `uv_coords`.

    xatlas::Destroy(atlas);
    return true;
}
