// (c) 2025, UltiMaker -- see LICENCE for details

#pragma once

#include <cstdint>
#include <vector>

using Face = std::tuple<int32_t, int32_t, int32_t>;
using Vertex = std::tuple<float, float, float>;
using UVCoord = std::tuple<float, float>;

bool unwrap_lscm(const std::vector<Vertex>& vertices, const std::vector<Face>& indices, std::vector<UVCoord>& uv_coords, uint32_t& texture_width, uint32_t& texture_height);