#pragma once
#include <chash/real.hpp>

namespace chash
{

template<typename RealId>
std::optional<Index> Real<RealId>::EnableOne()
{
	if (enabled == heads.size())
	{
		return std::nullopt;
	}
	return heads[enabled++];
}

template<typename RealId>
std::optional<Index> Real<RealId>::DisableOne()
{
	if (enabled == 0)
	{
		return std::nullopt;
	}
	return heads[--enabled];
}

template<typename RealId>
void Real<RealId>::Update(Patch<RealId>& patch, RealId id, Index enabled_request)
{
	auto l = heads.begin() + enabled;
	requested = std::min<Index>(enabled_request, heads.size());
	auto r = heads.begin() + requested;
	if (requested < enabled)
	{
		patch.off.insert(patch.off.end(), r, l);
		enabled = requested;
		return;
	}

	for (; l != r; ++l)
	{
		patch.on.emplace_back(*l, id);
	}

	enabled = requested;
}

template<typename RealId>
std::vector<Index>::const_iterator Real<RealId>::cbegin() const
{
	return heads.cbegin();
}

template<typename RealId>
std::vector<Index>::const_iterator Real<RealId>::cend() const
{
	std::vector<Index>::const_iterator tmp = heads.cbegin();
	std::advance(tmp, enabled);
	return tmp;
}

template<typename RealId>
bool Real<RealId>::Disabled() const
{
	return enabled == 0;
}

template<typename RealId>
bool Real<RealId>::Full() const
{
	return enabled == heads.size();
}

} // namespace chash