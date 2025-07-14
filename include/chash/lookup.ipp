#pragma once
#include <chash/lookup.hpp>
#include <chash/utils.hpp>

namespace chash
{

Lookup::Lookup(RealId* buf, Index size) : enabled_(size + 1, false), data_{buf}
{
	enabled_.back() = true;
}

void Lookup::DisablingUpdate(const std::vector<Index>& off)
{
	for (auto pos : off)
	{
		enabled_[pos] = false;
	}
	if (data_[0] != InvalId)
	{
		std::fill(data_, data_ + size(), InvalId);
		stat_.clear();
		stat_[InvalId] = size();
	}
	return;
}

void Lookup::Update(Patch& patch)
{
	const auto& [on, off, offstart] = patch;

	for (const auto& [pos, id] : on)
	{
		enabled_[pos] = true;
	}

	EnableDirty(on.cbegin(), on.cend());
	FixSeam();

	if (off.empty())
	{
		return;
	}

	auto off_split = std::lower_bound(off.begin(), off.end(), offstart.value());

	DisableDirty(off_split, off.cend());
	FixSeam();
	DisableDirty(off.cbegin(), off_split);
}

// Doesn't handle ring wrap-around
void Lookup::EnableDirty(Index idx, RealId id)
{
	enabled_[idx] = true;
	const RealId old = data_[idx];
	data_[idx] = id;
	Index i = idx + 1;
	for (; !enabled_[i]; ++i)
	{
		data_[i] = id;
	}

	const Index l = i - idx;
	stat_[old] -= l;
	stat_[id] += l;
}

// Doesn't handle ring wrap-around
void Lookup::DisableDirty(const Index pos)
{
	const RealId old = data_[pos];
	enabled_[pos] = false;
	const RealId tint = data_[PrevRingPosition(size(), pos)];
	Index i = pos;
	for (; !enabled_[i]; ++i)
	{
		data_[i] = tint;
	}

	const Index l = i - pos;
	stat_[old] -= l;
	stat_[tint] += l;
}

void Lookup::EnableDirty(std::vector<Patch::on_t>::const_iterator begin,
                         std::vector<Patch::on_t>::const_iterator end)
{
	for (auto hit = begin; hit != end; ++hit)
	{
		EnableDirty(hit->first, hit->second);
	}
}

void Lookup::DisableDirty(std::vector<Index>::const_iterator begin,
                          std::vector<Index>::const_iterator end)
{
	for (auto hit = begin; hit != end; ++hit)
	{
		DisableDirty(*hit);
	}
}

void Lookup::FixSeam()
{
	const RealId old = data_[0];
	const RealId last = data_[size() - 1];
	if (last != old)
	{
		Index i = 0;
		for (; !enabled_[i]; ++i)
		{
			data_[i] = last;
		}
		stat_[old] -= i;
		stat_[last] += i;
	}
}

void Lookup::Enable(Index pos, RealId id)
{
	EnableDirty(pos, id);
	FixSeam();
}
void Lookup::Disable(Index pos)
{
	DisableDirty(pos);
	FixSeam();
}

const RealId* Lookup::data() const
{
	return data_;
}

Index Lookup::size() const
{
	return enabled_.size() - 1;
}

const std::unordered_map<RealId, Index>& Lookup::Stats() const
{
	return stat_;
}

} // namespace chash