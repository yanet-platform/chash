#pragma once
#include <cstdint>
#include <limits>

namespace chash
{
using RealId = std::uint32_t;
using Index = std::uint32_t;
using Weight = std::uint32_t;

inline constexpr Weight MAX_WEIGHT = 100;
inline constexpr auto RNG_SEED = 42;
inline constexpr auto SIDE_RINGS_COUNT = 100;
inline constexpr auto DEFAULT_SEGMENTS_PER_WEIGHT = 16;

inline constexpr RealId InvalId = std::numeric_limits<RealId>::max();

inline Index LookupRequiredSize(Index real_count, Index segments_per_weight = DEFAULT_SEGMENTS_PER_WEIGHT)
{
	return real_count * MAX_WEIGHT * segments_per_weight;
}

} // namespace chash