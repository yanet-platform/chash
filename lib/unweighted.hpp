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
	std::map<RealId, IdHash> to_hash_;

public:
	template<typename Real>
	Unweighted(const std::vector<std::pair<Real, RealId>>& reals, Salt salt)
	{
		std::map<IdHash, std::size_t> seen;
		for (std::size_t i = 0; i < reals.size(); ++i)
		{
			auto& [real, id] = reals[i];
			auto hid = CalcHash(real, salt);
			// Real comparing greater wins collision
			if (auto it = to_id_.find(hid); it != to_id_.end())
			{
				auto& real_old = reals[seen.at(hid)].first;
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

		for (auto [hash, id]: to_id_)
		{
			to_hash_.emplace(id, hash);
		}
	}

	RealId Match(IdHash hash)
	{
		auto e = to_id_.lower_bound(hash);
		return (e != to_id_.end()) ? e->second : to_id_.begin()->second;
	}

	std::optional<RealId> Substitute(RealId id)
	{
		auto hit = to_hash_.find(id);
		if (hit == to_hash_.end() || (to_id_.size() == 1))
		{
			return std::nullopt;
		}
		auto it = to_id_.find(hit->second);
		if (it == to_id_.begin())
		{
			it = to_id_.end();
		}
		return std::prev(it)->second;
	}

	bool contains(RealId id) const
	{
		return std::any_of(
			to_id_.begin(),
			to_id_.end(),
			[&](const auto& it){
				return it.second == id;
			}
		);
	}

	std::size_t size() const
	{
		return to_id_.size();
	}
};

} // namespace chash