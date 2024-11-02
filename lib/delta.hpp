#pragma once

#include "common.hpp"

namespace chash
{

struct Slice
{
	Index begin;
	Index end;
	RealId id;
};

bool operator<(const Slice& a, const Slice& b)
{
	return a.begin < b.end;
}

template<typename Real>
struct Delta
{
	std::map<Real, RealId> add;
	std::set<RealId> remove;
	std::set<Slice> slices;
};

template<typename Real>
class DeltaBuilder
{
	Delta<Real> delta_;

public:
	void Remove(const Real& real, RealId id)
	{
		delta_.remove.insert(id);
		delta_.add.erase(real);
	}

	void Add(const Real& real, RealId id)
	{
		delta_.add[real] = id;
		delta_.remove.erase(id);
	}

	void Add(const Slice& slice)
	{
		auto& slices = delta_.slices;

		if (slices.empty())
		{
			slices.insert(slice);
			return;
		}

		auto it = slices.lower_bound(slice);

		// Clip partially intersecting left
		if (it != slices.begin())
		{
			auto left = std::prev(it);
			if (left->end >= slice.begin)
			{
				if (left->id == slice.id)
				{
					slice.begin = left->begin;
					slices.erase(left);
				}
				else
				{
					left.end = slice.begin;
				}
			}
		}

		// Delete fully intersecting
		while ((it != slices.end()) && (it->end <= slice.end))
		{
			auto curr = it;
			++it;
			slices.erase(curr);
		}

		// Clip partially intersecting right
		if ((it != slices.end()) && (it->begin <= slice.end))
		{
			if (it->id == slice.id)
			{
				slice.end = it->end;
				slices.erase(it);
			}
			else
			{
				it->begin = slice.end;
			}
		}

		slices.insert(slice);
	}

	void Add(const std::vector<Slice>& slices)
	{
		for (auto& slice : slices)
		{
			Add(slice);
		}
	}

	const Delta<Real>& GetDelta()
	{
		return delta_;
	}
};

} // namespace chash