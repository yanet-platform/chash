#pragma once
#include <optional>
#include <vector>

#include <chash/config.hpp>
#include <chash/patch.hpp>

namespace chash
{

template<typename RealId>
struct Real
{
	std::vector<Index> heads;
	Index enabled = 0;
	Index requested = 0;

	std::optional<Index> EnableOne();
	std::optional<Index> DisableOne();

	void Update(Patch<RealId>& patch, RealId id, Index enabled_request);
	auto cbegin();
	auto cend();

	bool Disabled();
	bool Full();
};

} // namespace chash

#include <chash/real.ipp>