#pragma once
#include <cstdint>

namespace chash
{

struct DefaultConfig
{
	using RealId = std::uint32_t;
	using Index = std::uint32_t;
	using UnweightedIndex = std::uint32_t;
	using Weight = std::uint32_t;
	static const Weight MaxWeight = 100;
};

} // namespace chash