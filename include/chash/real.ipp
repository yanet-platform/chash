#pragma once
#include <chash/real.hpp>

namespace chash
{
std::optional<Index> Real::EnableOne()
{
	if (enabled == heads.size())
	{
		return std::nullopt;
	}
	return heads[enabled++];
}

std::optional<Index> Real::DisableOne()
{
	if (enabled == 0)
	{
		return std::nullopt;
	}
	return heads[--enabled];
}

void Real::Update(Patch& patch, RealId id, Index enabled_request)
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

auto Real::cbegin()
{
	return heads.cbegin();
}

auto Real::cend()
{
	return heads.cbegin() + enabled;
}

bool Real::Disabled()
{
	return enabled == 0;
}

bool Real::Full()
{
	return enabled == heads.size();
}

} // namespace chash