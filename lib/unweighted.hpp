#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "hash.hpp"

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

	std::vector<Region> regions_;

	Unweighted(std::size_t size)
	{
		regions_.reserve(size + 1);
	}

public:
	template<typename Real>
	static Unweighted Make(
	        const Real* reals,
	        const RealId* ids,
	        std::size_t cnt,
	        Salt salt)
	{
		Unweighted ring(cnt);
		for (std::size_t i = 0; i < cnt; ++i)
		{
			const Real& real = reals[i];
			const RealId& id = ids[i];
			auto hash = CalcHash(real, salt);
			ring.regions_.push_back({id, hash});
		}

		std::sort(ring.regions_.begin(), ring.regions_.end(), [](const Region& a, const Region& b){
			return a.end < b.end;
		});

		if (ring.regions_.back().end != std::numeric_limits<IdHash>::max())
		{
			ring.regions_.push_back({ring.regions_.front().id, std::numeric_limits<IdHash>::max()});
		}

		return ring;
	}

	RealId Match(IdHash hash)
	{
		return std::lower_bound(regions_.cbegin(), regions_.cend(), hash, [](const Region& a, IdHash b) {
			       return a.end < b;
		       })
		        ->id;
	}
};

} // namespace chash