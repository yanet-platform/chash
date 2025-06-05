#pragma once
#include <cstdint>
#include <random>

#ifndef GCC_BUG_UNUSED
#define GCC_BUG_UNUSED(arg) (void)(arg);
#endif

namespace chash
{

struct DefaultConfig
{
	using RealId = std::uint32_t;
	using Index = std::uint32_t;
	using UnweightedIndex = std::uint32_t;
	using Weight = std::uint32_t;
	static const Weight MaxWeight = 100;
	static const std::size_t UnweightedMultiplier = 1 << 5;
	static constexpr std::mt19937::result_type RNG_SEED = 42;
	static constexpr std::size_t DEFAULT_UNWEIGHTED_SIZE = 1 << 16;
	static constexpr double DEFAULT_DEVIATION_TOLERANCE = 0.05;
	static constexpr std::size_t PATCH_LIMIT = 16000;
};

} // namespace chash