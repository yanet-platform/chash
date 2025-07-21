#pragma once
#include <chash/lookup.hpp>
#include <chash/utils.hpp>

namespace chash
{

template<typename RealId>
Lookup<RealId>::Lookup(RealId* buf, Index size) : enabled_(size + 1, false), data_{buf}
{
	enabled_.back() = true;
}

template<typename RealId>
void Lookup<RealId>::Init(SegmentIterator<RealId> begin, SegmentIterator<RealId> end)
{
	std::fill(data_, data_ + size(), InvalId<RealId>);
	stat_.clear();

	for (auto it = begin; it != end; ++it)
	{
		data_[it.pos()] = it.id();
		enabled_[it.pos()] = true;
	}

	RealId tint = data_[0];
	Index prev {};
	for (Index idx = 0; idx < size(); ++idx)
	{
		if (enabled_[idx])
		{
			stat_[tint] += idx - prev;
			tint = data_[idx];
			prev = idx;
		}
		data_[idx] = tint;
	}

	FixSeam();
}

template<typename RealId>
void Lookup<RealId>::DisablingUpdate(const std::vector<Index>& off)
{
	for (auto pos : off)
	{
		enabled_[pos] = false;
	}
	if (data_[0] != InvalId<RealId>)
	{
		std::fill(data_, data_ + size(), InvalId<RealId>);
		stat_.clear();
		stat_[InvalId<RealId>] = size();
	}
	return;
}

template<typename RealId>
void Lookup<RealId>::Update(Patch<RealId>& patch)
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
template<typename RealId>
void Lookup<RealId>::EnableDirty(Index idx, RealId id)
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
template<typename RealId>
void Lookup<RealId>::DisableDirty(const Index pos)
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

template<typename RealId>
void Lookup<RealId>::EnableDirty(typename std::vector<typename Patch<RealId>::on_t>::const_iterator begin,
                         typename std::vector<typename Patch<RealId>::on_t>::const_iterator end)
{
	for (auto hit = begin; hit != end; ++hit)
	{
		EnableDirty(hit->first, hit->second);
	}
}

template<typename RealId>
void Lookup<RealId>::DisableDirty(std::vector<Index>::const_iterator begin,
                          std::vector<Index>::const_iterator end)
{
	for (auto hit = begin; hit != end; ++hit)
	{
		DisableDirty(*hit);
	}
}

template<typename RealId>
void Lookup<RealId>::FixSeam()
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

template<typename RealId>
void Lookup<RealId>::Enable(Index pos, RealId id)
{
	EnableDirty(pos, id);
	FixSeam();
}

template<typename RealId>
void Lookup<RealId>::Disable(Index pos)
{
	DisableDirty(pos);
	FixSeam();
}

template<typename RealId>
const RealId* Lookup<RealId>::data() const
{
	return data_;
}

template<typename RealId>
Index Lookup<RealId>::size() const
{
	return enabled_.size() - 1;
}

template<typename RealId>
const std::unordered_map<RealId, Index>& Lookup<RealId>::Stats() const
{
	return stat_;
}

} // namespace chash