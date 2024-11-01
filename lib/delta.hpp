#pragma once

#include "common.hpp"

namespace chash
{

struct Slice
{
	Index head;
	Index size;
	RealId id;
};

bool operator<(const Slice& a, const Slice& b)
{
	return a.head < b.head;
}

template<typename Real>
struct Delta
{
	std::map<Real, RealId> add;
	std::map<Real, RealId> remove;
	std::set<Slice> slices;
};

template<typename Real>
class DeltaBuilder
{
	Delta<Real> delta_;
public:
	void Add(Slice slice)
	{
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

}