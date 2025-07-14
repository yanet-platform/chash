#pragma once
#include <vector>
#include <optional>

namespace chash
{

struct Patch
{
	using on_t = std::pair<Index, RealId>;
	std::vector<on_t> on;
	std::vector<Index> off;
	std::optional<Index> offstart;
};

} // namespace chash