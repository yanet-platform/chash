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
	std::map<IdHash, RealId> to_id_;

public:
	template<typename Real>
	Unweighted(
	        const Real* reals,
	        const RealId* ids,
	        std::size_t cnt,
	        Salt salt)
	{
		std::map<IdHash, std::size_t> seen;
		for (std::size_t i = 0; i < cnt; ++i)
		{
			const Real& real = reals[i];
			const RealId& id = ids[i];
			auto hid = CalcHash(real, salt);
			// Real comparing greater wins collision
			if (auto it = to_id_.find(hid); it != to_id_.end())
			{
				auto& real_old = reals[seen.at(hid)];
				if (real_old < real)
				{
					to_id_[hid] = id;
					seen[hid] = i;
				}
			}
			else
			{
				to_id_[hid] = id;
				seen[hid] = i;
			}
		}

	}

	RealId Match(IdHash hash)
	{
		auto e = to_id_.lower_bound(hash);
		return (e != to_id_.end()) ? e->second : to_id_.begin()->second;
	}

	bool contains(RealId id) const
	{
		return std::any_of(
		        to_id_.begin(),
		        to_id_.end(),
		        [&](const auto& it) {
			        return it.second == id;
		        });
	}

	std::size_t size() const
	{
		return to_id_.size();
	}
};

} // namespace chash