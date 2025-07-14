#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include <chash/hash.hpp>

namespace chash
{

template<typename RealId>
class Unweighted
{
	struct Region
	{
		RealId id;
		IdHash end;
	};

	std::vector<RealId> lookup_;

	Unweighted(IdHash size)
	{
		lookup_.reserve(size);
	}

public:
	template<typename RealIter, typename IdIter>
	static Unweighted Make(
	        IdHash size,
			IdIter ids_begin,
			IdIter ids_end,
			RealIter reals_begin,
	        Salt salt)
	{
		std::vector<Region> regions;
		regions.reserve(std::distance(ids_begin, ids_end));
		Unweighted ring(size);
		for (;ids_begin != ids_end; ++ids_begin, ++reals_begin)
		{
			const auto& real = *reals_begin;
			const RealId& id = *ids_begin;
			auto hash = CalcHash(real, salt);
			regions.push_back({id, hash % size});
		}

		std::sort(regions.begin(), regions.end(), [](const Region& a, const Region& b) {
			if (a.end != b.end)
			{
				return a.end < b.end;
			}
			return a.id < b.id;
		});

		regions.erase(std::unique(regions.begin(), regions.end(), [](const Region& a, const Region& b) {
			              return a.end == b.end;
		              }),
		              regions.end());

		for (auto region : regions)
		{
			while (ring.lookup_.size() < region.end)
			{
				ring.lookup_.push_back(region.id);
			}
		}
		while (ring.lookup_.size() < size)
		{
			ring.lookup_.push_back(ring.lookup_[0]);
		}

		return ring;
	}

	RealId Match(IdHash hash)
	{
		return lookup_[hash % lookup_.size()];
	}
};

} // namespace chash