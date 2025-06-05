// (c) 2025, UltiMaker -- see LICENCE for details

#include "unwrap.hpp"

// NOTE: The UV-coords parameter already has the right size, and a pointer is set up towards its 'innards', please don't append, emplace, etc. or do anything to change its size or location.
void unwrap_algo(
    const std::vector<std::tuple<float, float, float>>& vertices,
    const std::vector<std::tuple<int32_t, int32_t, int32_t>>& indices,
    std::vector<std::tuple<float, float>>& uv_coords
)
{
    // TODO: implement the actual UV-unwrapping here
}
