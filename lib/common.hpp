#pragma once
#include <cstdint>

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
	static constexpr std::mt19937::result_type RNG_SEED = 42;
	static constexpr std::size_t DEFAULT_UNWEIGHTED_SIZE = 65553;
};

} // namespace chash