// (c) 2025, UltiMaker -- see LICENCE for details

#include <vector>
#include <tuple>

bool unwrap_algo(
    const std::vector<std::tuple<float, float, float>>& vertices,
    const std::vector<std::tuple<int32_t, int32_t, int32_t>>& indices,
    std::vector<std::tuple<float, float>>& uv_coords
);
