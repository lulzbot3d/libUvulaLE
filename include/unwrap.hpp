// (c) 2025, UltiMaker -- see LICENCE for details

#include <vector>
#include <tuple>
#include <cstdint>

bool unwrap_algo(
    const std::vector<std::tuple<float, float, float>>& vertices,
    const std::vector<std::tuple<int32_t, int32_t, int32_t>>& indices,
    uint32_t desired_definition,
    std::vector<std::tuple<float, float>>& uv_coords,
    uint32_t &texture_width,
    uint32_t &texture_height
);
